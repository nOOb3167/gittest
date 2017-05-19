#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <gittest/net2.h>

#include <gittest/net2_surrogate.h>

int gs_address_surrogate_setup_addr_name_port(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	struct GsAddressSurrogate *ioAddressSurrogate)
{
	int r = 0;

	if (!!(r = gs_buf_ensure_haszero(ServHostNameBuf, LenServHostName + 1)))
		GS_GOTO_CLEAN();

	if (!!(r = enet_address_set_host(&ioAddressSurrogate->mAddr, ServHostNameBuf)))
		GS_GOTO_CLEAN();

	ioAddressSurrogate->mAddr.port = ServPort;

clean:

	return r;
}

int gs_host_surrogate_setup_host_nobind(
	uint32_t NumMaxPeers,
	struct GsHostSurrogate *ioHostSurrogate)
{
	struct ENetIntrHostCreateFlags FlagsHost = {};
	if (!(ioHostSurrogate->mHost = enet_host_create_interruptible(NULL, NumMaxPeers, 1, 0, 0, &FlagsHost)))
		return 1;
	return 0;
}

int gs_host_surrogate_setup_host_bind_port(
	uint32_t ServPort,
	uint32_t NumMaxPeers,
	struct GsHostSurrogate *ioHostSurrogate)
{
	struct ENetIntrHostCreateFlags FlagsHost = {};
	ENetAddress addr = {};
	/* NOTE: ENET_HOST_ANY (0) binds to all interfaces but will also cause host->address to have 0 as host */
	addr.host = ENET_HOST_ANY;
	addr.port = ServPort;
	if (!(ioHostSurrogate->mHost = enet_host_create_interruptible(&addr, NumMaxPeers, 1, 0, 0, &FlagsHost)))
		return 1;
	return 0;
}

int gs_host_surrogate_connect(
	struct GsHostSurrogate *HostSurrogate,
	struct GsAddressSurrogate *AddressSurrogate,
	struct GsPeerSurrogate *ioPeerSurrogate)
{
	if (!(ioPeerSurrogate->mPeer = enet_host_connect(HostSurrogate->mHost, &AddressSurrogate->mAddr, 1, 0)))
		return 1;
	return 0;
}

int gs_host_surrogate_connect_wait_blocking(
	struct GsHostSurrogate *HostSurrogate,
	struct GsPeerSurrogate *PeerSurrogate)
{
	int r = 0;

	int errService = 0;
	ENetEvent event = {};

	while (0 <= (errService = enet_host_service(HostSurrogate->mHost, &event, GS_TIMEOUT_1SEC))) {
		if (errService > 0 && event.peer == PeerSurrogate->mPeer && event.type == ENET_EVENT_TYPE_CONNECT)
			break;
	}

	/* a connection event must have been setup above */
	if (errService < 0 ||
		event.type != ENET_EVENT_TYPE_CONNECT ||
		event.peer != PeerSurrogate->mPeer)
	{
		GS_ERR_CLEAN(1);
	}

clean:

	return r;
}

int gs_host_surrogate_connect_wait_blocking_register(
	struct GsHostSurrogate *Host,
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t *oAssignedId)
{
	int r = 0;

	struct GsBypartCbDataGsConnectionSurrogateId *ctxstruct = new GsBypartCbDataGsConnectionSurrogateId();

	struct GsAddressSurrogate Address = {};
	struct GsPeerSurrogate Peer = {};
	struct GsConnectionSurrogate ConnectionSurrogate = {};

	if (!!(r = gs_address_surrogate_setup_addr_name_port(ServPort, ServHostNameBuf, LenServHostName, &Address)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_host_surrogate_connect(Host, &Address, &Peer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_host_surrogate_connect_wait_blocking(Host, &Peer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_connection_surrogate_init(Host, &Peer, true, &ConnectionSurrogate)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_connection_surrogate_map_register_bond_transfer_ownership(
		ConnectionSurrogate,
		GS_ARGOWN(&ctxstruct),
		ioConnectionSurrogateMap,
		oAssignedId)))
	{
		GS_GOTO_CLEAN();
	}

clean:
	if (!!r) {
		GS_DELETE(&ctxstruct, GsBypartCbDataGsConnectionSurrogateId);
	}

	return r;
}

/** NOTE: release ownership - not destroy */
int gs_packet_surrogate_release_ownership(struct GsPacketSurrogate *ioPacketSurrogate)
{
	ioPacketSurrogate->mPacket = NULL;
	return 0;
}

int gs_packet_create(
	struct GsPacket **oPacket,
	struct GsPacketSurrogate *valPacketSurrogate)
{
	struct GsPacket *Packet = new GsPacket();
	Packet->mPacket = *valPacketSurrogate;
	Packet->data = valPacketSurrogate->mPacket->data;
	Packet->dataLength = valPacketSurrogate->mPacket->dataLength;

	if (oPacket)
		*oPacket = Packet;

	return 0;
}

int gs_connection_surrogate_init(
	struct GsHostSurrogate *Host,
	struct GsPeerSurrogate *Peer,
	uint32_t IsPrincipalClientConnection,
	struct GsConnectionSurrogate *ioConnectionSurrogate)
{
	ioConnectionSurrogate->mHost = Host->mHost;
	ioConnectionSurrogate->mPeer = Peer->mPeer;
	ioConnectionSurrogate->mIsPrincipalClientConnection = IsPrincipalClientConnection;
	return 0;
}

/** Send Packet (remember enet_peer_send required an ownership release) */
int gs_connection_surrogate_packet_send(
	struct GsConnectionSurrogate *ConnectionSurrogate,
	struct GsPacket *ioPacket)
{
	int r = 0;

	ENetPacket *packet = ioPacket->mPacket.mPacket;

	/* ownership of packet is lost after enet_peer_send */
	if (!!(r = gs_packet_surrogate_release_ownership(&ioPacket->mPacket)))
		GS_GOTO_CLEAN();

	if (enet_peer_send(ConnectionSurrogate->mPeer, 0, packet))
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int gs_connection_surrogate_map_create(
	struct GsConnectionSurrogateMap **oConnectionSurrogateMap)
{
	GsConnectionSurrogateMap *ConnectionSurrogateMap = new GsConnectionSurrogateMap();
	
	ConnectionSurrogateMap->mAtomicCount = std::atomic<uint32_t>(0);
	ConnectionSurrogateMap->mConnectionSurrogateMap = sp<gs_connection_surrogate_map_t>(new gs_connection_surrogate_map_t);

	if (oConnectionSurrogateMap)
		*oConnectionSurrogateMap = ConnectionSurrogateMap;

	return 0;
}

int gs_connection_surrogate_map_destroy(
	struct GsConnectionSurrogateMap *ConnectionSurrogateMap)
{
	GS_DELETE(&ConnectionSurrogateMap, GsConnectionSurrogateMap);
	return 0;
}

int gs_connection_surrogate_map_clear(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	ioConnectionSurrogateMap->mConnectionSurrogateMap->clear();
	return 0;
}

int gs_connection_surrogate_map_insert_id(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	const struct GsConnectionSurrogate valConnectionSurrogate)
{
	int r = 0;

	if (! ioConnectionSurrogateMap->mConnectionSurrogateMap->insert(
			gs_connection_surrogate_map_t::value_type(ConnectionSurrogateId, valConnectionSurrogate)).second)
	{
		GS_ERR_CLEAN_L(1, E, S, "insertion prevented (is a stale element present, and why?)");
	}

clean:

	return r;
}

int gs_connection_surrogate_map_insert(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	const struct GsConnectionSurrogate valConnectionSurrogate,
	gs_connection_surrogate_id_t *oConnectionSurrogateId)
{
	int r = 0;

	gs_connection_surrogate_id_t Id = ioConnectionSurrogateMap->mAtomicCount.fetch_add(1);

	if (!!(r = gs_connection_surrogate_map_insert_id(ioConnectionSurrogateMap, Id, valConnectionSurrogate)))
		GS_GOTO_CLEAN();

	if (oConnectionSurrogateId)
		*oConnectionSurrogateId = Id;

clean:

	return r;
}

int gs_connection_surrogate_map_get_try(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	struct GsConnectionSurrogate *oConnectionSurrogate,
	uint32_t *oIsPresent)
{
	int r = 0;

	struct GsConnectionSurrogate ConnectionSurrogate = {};
	uint32_t IsPresent = false;

	gs_connection_surrogate_map_t::iterator it =
		ioConnectionSurrogateMap->mConnectionSurrogateMap->find(ConnectionSurrogateId);

	if (it != ioConnectionSurrogateMap->mConnectionSurrogateMap->end()) {
		ConnectionSurrogate = it->second;
		IsPresent = true;
	}

	if (oConnectionSurrogate)
		*oConnectionSurrogate = ConnectionSurrogate;

	if (oIsPresent)
		*oIsPresent = IsPresent;

clean:

	return r;
}

int gs_connection_surrogate_map_get(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	struct GsConnectionSurrogate *oConnectionSurrogate)
{
	int r = 0;

	uint32_t IsPresent = false;

	if (!!(r = gs_connection_surrogate_map_get_try(
		ioConnectionSurrogateMap,
		ConnectionSurrogateId,
		oConnectionSurrogate,
		&IsPresent)))
	{
		GS_GOTO_CLEAN();
	}

	if (! IsPresent)
		GS_ERR_CLEAN_L(1, E, S, "retrieval prevented (is an element missing, and why?)");

clean:

	return r;
}

int gs_connection_surrogate_map_erase(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId)
{
	int r = 0;

	gs_connection_surrogate_map_t::iterator it =
		ioConnectionSurrogateMap->mConnectionSurrogateMap->find(ConnectionSurrogateId);

	if (it == ioConnectionSurrogateMap->mConnectionSurrogateMap->end())
		GS_ERR_NO_CLEAN_L(0, E, S, "removal suppressed (is an element missing, and why?)");
	else
		ioConnectionSurrogateMap->mConnectionSurrogateMap->erase(it);

noclean:

clean:

	return r;
}

/** Two part connection registration.
    The connection is assigned an entry with Id within the connection map.
	That same Id is then bonded to the ENetPeer 'data' field.

   @param valConnectionSurrogate copied / copy-constructed (for shared_ptr use)
*/
int gs_connection_surrogate_map_register_bond_transfer_ownership(
	struct GsConnectionSurrogate valConnectionSurrogate,
	struct GsBypartCbDataGsConnectionSurrogateId *HeapAllocatedDefaultedOwnedCtxstruct, /**< owned */
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t *oAssignedId)
{
	int r = 0;

	HeapAllocatedDefaultedOwnedCtxstruct->Tripwire = GS_BYPART_TRIPWIRE_GsConnectionSurrogateId;
	HeapAllocatedDefaultedOwnedCtxstruct->m0Id = -1;

	/* bond to the peer */
	valConnectionSurrogate.mPeer->data = HeapAllocatedDefaultedOwnedCtxstruct;

	/* assign entry with id */
	/* NOTE: valConnectionSurrogate.mPeer->data pointer gets copied into connection map entry.
	         since data was set to HeapAllocatedDefaultedOwnedCtxstruct, we can assign to its m0Id field. */
	if (!!(r = gs_connection_surrogate_map_insert(
		ioConnectionSurrogateMap,
		valConnectionSurrogate,
		&HeapAllocatedDefaultedOwnedCtxstruct->m0Id)))
	{
		GS_GOTO_CLEAN();
	}

	if (oAssignedId)
		*oAssignedId = HeapAllocatedDefaultedOwnedCtxstruct->m0Id;

clean:
	if (!!r) {
		GS_DELETE(&HeapAllocatedDefaultedOwnedCtxstruct, GsBypartCbDataGsConnectionSurrogateId);
	}

	return r;
}
