#ifndef _NET2_SURROGATE_H_
#define _NET2_SURROGATE_H_

#include <stdint.h>

#include <atomic>
#include <map>

#include <enet/enet.h>

#include <gittest/misc.h>

struct GsConnectionSurrogate;

typedef ::std::map<gs_connection_surrogate_id_t, GsConnectionSurrogate> gs_connection_surrogate_map_t;

/** manual-init struct
    value struct
*/
struct GsIntrTokenSurrogate {
	struct ENetIntrToken *mIntrToken;
};

/** manual-init struct
    value struct

	@sa
	  ::gs_addr_surrogate_setup_addr_name_port
*/
struct GsAddressSurrogate {
	ENetAddress mAddr;
};

/** manual-init struct
    value struct

	@sa
	  ::gs_host_surrogate_setup_host_nobind
	  ::gs_host_surrogate_setup_host_bind_port
	  ::gs_host_surrogate_connect
	  ::gs_host_surrogate_connect_wait_blocking
*/
struct GsHostSurrogate {
	ENetHost *mHost;
};

/** manual-init struct
    value struct
*/
struct GsPeerSurrogate {
	ENetPeer *mPeer;
};

/** manual-init struct
	value struct
*/
struct GsEventSurrogate {
	ENetEvent event;
};

/** manual-init struct
    value struct

	@sa
	   ::gs_packet_surrogate_release_ownership
*/
struct GsPacketSurrogate {
	ENetPacket *mPacket;
};

/** manual-init struct
    value struct

	@sa
	  ::gs_connection_surrogate_init
	  ::gs_connection_surrogate_packet_send
*/
struct GsConnectionSurrogate {
	ENetHost *mHost;
	ENetPeer *mPeer;
	uint32_t mIsPrincipalClientConnection;
};

/** Design:
	Entries enter this structure on connection (ENet ENET_EVENT_TYPE_CONNECT).
	Entries leave this structure on disconnection (ENet ENET_EVENT_TYPE_DISCONNECT).
	Throughout operation, IDs are dealt out (even to other threads eg worker).
	There may be attempted ID uses after an entry has already left the structure.
	Design operations (especially the query/get kind) to handle missing entries.

	@sa
       ::gs_connection_surrogate_map_create
	   ::gs_connection_surrogate_map_destroy
	   ::gs_connection_surrogate_map_clear
	   ::gs_connection_surrogate_map_insert_id
	   ::gs_connection_surrogate_map_insert
	   ::gs_connection_surrogate_map_get_try
	   ::gs_connection_surrogate_map_get
	   ::gs_connection_surrogate_map_erase
	   ::gs_connection_surrogate_map_register_bond_transfer_ownership
*/
struct GsConnectionSurrogateMap {
	std::atomic<uint64_t> mAtomicCount;
	sp<gs_connection_surrogate_map_t> mConnectionSurrogateMap;
};

int gs_address_surrogate_setup_addr_name_port(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	struct GsAddressSurrogate *ioAddressSurrogate);

int gs_host_surrogate_setup_host_nobind(
	uint32_t NumMaxPeers,
	struct GsHostSurrogate *ioHostSurrogate);
int gs_host_surrogate_setup_host_bind_port(
	uint32_t ServPort,
	uint32_t NumMaxPeers,
	struct GsHostSurrogate *ioHostSurrogate);
int gs_host_surrogate_connect(
	struct GsHostSurrogate *HostSurrogate,
	struct GsAddressSurrogate *AddressSurrogate,
	struct GsPeerSurrogate *ioPeerSurrogate);
int gs_host_surrogate_connect_wait_blocking(
	struct GsHostSurrogate *HostSurrogate,
	struct GsPeerSurrogate *PeerSurrogate);
int gs_host_surrogate_connect_wait_blocking_register(
	struct GsHostSurrogate *Host,
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t *oAssignedId);

int gs_packet_surrogate_release_ownership(struct GsPacketSurrogate *ioPacketSurrogate);

int gs_connection_surrogate_init(
	struct GsHostSurrogate *Host,
	struct GsPeerSurrogate *Peer,
	uint32_t IsPrincipalClientConnection,
	struct GsConnectionSurrogate *ioConnectionSurrogate);
int gs_connection_surrogate_packet_send(
	struct GsConnectionSurrogate *ConnectionSurrogate,
	struct GsPacket *ioPacket);

int gs_connection_surrogate_map_create(
	struct GsConnectionSurrogateMap **oConnectionSurrogateMap);
int gs_connection_surrogate_map_destroy(
	struct GsConnectionSurrogateMap *ConnectionSurrogateMap);
int gs_connection_surrogate_map_clear(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_connection_surrogate_map_insert_id(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	const struct GsConnectionSurrogate valConnectionSurrogate);
int gs_connection_surrogate_map_insert(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	const GsConnectionSurrogate valConnectionSurrogate,
	gs_connection_surrogate_id_t *oConnectionSurrogateId);
int gs_connection_surrogate_map_get_try(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	struct GsConnectionSurrogate *oConnectionSurrogate,
	uint32_t *oIsPresent);
int gs_connection_surrogate_map_get(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	struct GsConnectionSurrogate *oConnectionSurrogate);
int gs_connection_surrogate_map_erase(
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId);
int gs_connection_surrogate_map_register_bond_transfer_ownership(
	struct GsConnectionSurrogate valConnectionSurrogate,
	struct GsBypartCbDataGsConnectionSurrogateId *HeapAllocatedDefaultedOwnedCtxstruct, /**< owned */
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t *oAssignedId);

#endif /* _NET2_SURROGATE_H_ */
