#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif _MSC_VER

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <memory>
#include <utility>  // std::move
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <deque>

#include <enet/enet.h>
#include <git2.h>

#include <gittest/misc.h>
#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/frame.h>
#include <gittest/gittest_selfupdate.h>

#include <gittest/net.h>

/*
* = Packet size vs Frame size =
* currently sizes are checked against Packet size, instead of the size field of the sent Frame.
* = Inferred size vs Explicit size for frame vector serialization =
*/

struct ClntState;

gs_packet_unique_t gs_packet_unique_t_null();

int aux_packet_bare_send(ENetHost *host, ENetPeer *peer, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags);
int aux_packet_full_send(ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags);
int aux_packet_response_queue_interrupt_request_reliable(ServAuxData *ServAuxData, ServWorkerData *WorkerDataSend, ServWorkerRequestData *Request, const char *Data, uint32_t DataSize);
int clnt_state_make_default(ClntState *oState);
int clnt_state_crank(
	const sp<ClntState> &State, const confmap_t &ClntKeyVal,
	const sp<ServAuxData> &ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend);

void gs_packet_unique_t_deleter::operator()(ENetPacket **xpacket) const {
		if (xpacket)
			if (*xpacket)  /* NOTE: reading enet source, enet_packet_destroy can be called with null, but check */
				enet_packet_destroy(*xpacket);
		delete xpacket;
}

PacketUniqueWithOffset::PacketUniqueWithOffset(PacketUniqueWithOffset &&other)
	: mPacket(std::move(other.mPacket)),
	mOffsetSize(other.mOffsetSize),
	mOffsetObject(other.mOffsetObject)
{}

PacketUniqueWithOffset & PacketUniqueWithOffset::operator=(PacketUniqueWithOffset &&other) {
	if (this != &other)
	{
		mPacket = std::move(other.mPacket);
		mOffsetSize = other.mOffsetSize;
		mOffsetObject = other.mOffsetObject;
	}
	return *this;
}

ServWorkerRequestData::ServWorkerRequestData(gs_packet_unique_t *ioPacket, ENetHost *Host, ENetPeer *Peer)
	: mPacket(),
	mHost(Host),
	mPeer(Peer)
{
	mPacket = std::move(*ioPacket);
}

bool ServWorkerRequestData::isReconnectRequest() {
	return ! mPacket;
}

ServWorkerData::ServWorkerData()
	: mWorkerQueue(new std::deque<sp<ServWorkerRequestData> >),
	mWorkerDataMutex(new std::mutex),
	mWorkerDataCond(new std::condition_variable)
{}

void ServWorkerData::RequestEnqueue(const sp<ServWorkerRequestData> &RequestData) {
	{
		std::unique_lock<std::mutex> lock(*mWorkerDataMutex);
		mWorkerQueue->push_back(RequestData);
	}
	mWorkerDataCond->notify_one();
}

void ServWorkerData::RequestDequeue(sp<ServWorkerRequestData> *oRequestData) {
	sp<ServWorkerRequestData> RequestData;
	{
		std::unique_lock<std::mutex> lock(*mWorkerDataMutex);
		mWorkerDataCond->wait(lock, [&]() { return !mWorkerQueue->empty(); });
		assert(! mWorkerQueue->empty());
		RequestData = mWorkerQueue->front();
		mWorkerQueue->pop_front();
	}
	if (oRequestData)
		oRequestData->swap(RequestData);
}

void ServWorkerData::RequestDequeueAllOpt(std::deque<sp<ServWorkerRequestData> > *oRequestData) {
	{
		std::unique_lock<std::mutex> lock(*mWorkerDataMutex);
		oRequestData->clear();
		oRequestData->swap(*mWorkerQueue);
	}
}

ServAuxData::ServAuxData()
	: mInterruptRequested(0),
	mAuxDataMutex(new std::mutex),
	mAuxDataCond(new std::condition_variable)
{}

void ServAuxData::InterruptRequestedEnqueue() {
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		mInterruptRequested = true;
	}
	mAuxDataCond->notify_one();
}

bool ServAuxData::InterruptRequestedDequeueTimeout(const std::chrono::milliseconds &WaitForMillis) {
	/* @return: Interrupt (aka send message from serv_aux to serv counts as requested
	*    if a thread sets mInterruptRequested and notifies us, or timeout expires but
	*    mInterruptRequested still got set */

	bool IsPredicateTrue = false;
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		IsPredicateTrue = mAuxDataCond->wait_for(lock, WaitForMillis, [&]() { return !!mInterruptRequested; });
		if (IsPredicateTrue)
			InterruptRequestedDequeueMT_();
	}
	return IsPredicateTrue;
}

void ServAuxData::InterruptRequestedDequeueMT_() {
	mInterruptRequested = false;
}

FullConnectionClient::FullConnectionClient(const sp<std::thread> &ThreadWorker, const sp<std::thread> &ThreadAux, const sp<std::thread> &Thread)
	: ThreadWorker(ThreadWorker),
	ThreadAux(ThreadAux),
	Thread(Thread)
{}

gs_packet_t aux_gs_make_packet(ENetPacket *packet) {
	return gs_packet_t(new ENetPacket *(packet), [](ENetPacket **xpacket) { enet_packet_destroy(*xpacket); delete xpacket; });
}

gs_packet_unique_t aux_gs_make_packet_unique(ENetPacket *packet) {
	return gs_packet_unique_t(new ENetPacket *(packet), gs_packet_unique_t_deleter());
}

gs_packet_unique_t gs_packet_unique_t_null() {
	return gs_packet_unique_t(nullptr, gs_packet_unique_t_deleter());
}

int aux_host_peer_pair_reset(sp<gs_host_peer_pair_t> *ioConnection) {
	if (*ioConnection != NULL) {
		ENetHost * const oldhost = (*ioConnection)->first;
		ENetPeer * const oldpeer = (*ioConnection)->second;

		*ioConnection = sp<gs_host_peer_pair_t>();

		enet_peer_disconnect_now(oldpeer, NULL);
		enet_host_destroy(oldhost);
	}

	return 0;
}

int aux_make_packet_with_offset(gs_packet_t Packet, uint32_t OffsetSize, uint32_t OffsetObject, PacketWithOffset *oPacketWithOffset) {
	PacketWithOffset ret;
	ret.mPacket = Packet;
	ret.mOffsetSize = OffsetSize;
	ret.mOffsetObject = OffsetObject;
	if (oPacketWithOffset)
		*oPacketWithOffset = ret;
	return 0;
}

/* http://en.cppreference.com/w/cpp/language/move_assignment */
int aux_make_packet_unique_with_offset(gs_packet_unique_t *ioPacket, uint32_t OffsetSize, uint32_t OffsetObject, PacketUniqueWithOffset *oPacketWithOffset) {
	PacketUniqueWithOffset ret;
	ret.mPacket = std::move(*ioPacket);
	ret.mOffsetSize = OffsetSize;
	ret.mOffsetObject = OffsetObject;
	if (oPacketWithOffset)
		*oPacketWithOffset = std::move(ret);
	return 0;
}

int aux_make_serv_worker_request_data(ENetHost *host, ENetPeer *peer, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oServWorkerRequestData) {
	int r = 0;

	sp<ServWorkerRequestData> ServWorkerRequestData(new ServWorkerRequestData(ioPacket, host, peer));

	if (oServWorkerRequestData)
		*oServWorkerRequestData = ServWorkerRequestData;

clean:

	return r;
}

int aux_make_serv_worker_request_data_for_response(
	ServWorkerRequestData *RequestBeingResponded, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oServWorkerRequestData)
{
	int r = 0;

	sp<ServWorkerRequestData> ServWorkerRequestData(new ServWorkerRequestData(
		ioPacket, RequestBeingResponded->mHost, RequestBeingResponded->mPeer));

	if (oServWorkerRequestData)
		*oServWorkerRequestData = ServWorkerRequestData;

clean:

	return r;
}

void aux_get_serv_worker_request_private(ServWorkerRequestData *Request, ENetHost **oHost, ENetPeer **oPeer) {

	if (oHost)
		*oHost = Request->mHost;

	if (oPeer)
		*oPeer = Request->mPeer;
}

int aux_serv_worker_thread_service_request_blobs(
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataSend, ServWorkerRequestData *Request,
	ENetPacket *Packet, uint32_t OffsetSize, git_repository *Repository, const GsFrameType &FrameTypeResponse)
{
	int r = 0;

	std::string ResponseBuffer;
	uint32_t Offset = OffsetSize;
	uint32_t LengthLimit = 0;
	std::vector<git_oid> BloblistRequested;
	std::string SizeBufferBlob;
	std::string ObjectBufferBlob;

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BloblistRequested)))
		GS_GOTO_CLEAN();

	if (!!(r = serv_serialize_blobs(Repository, &BloblistRequested, &SizeBufferBlob, &ObjectBufferBlob)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_response_blobs(
		&ResponseBuffer, FrameTypeResponse,
		BloblistRequested.size(), &SizeBufferBlob, &ObjectBufferBlob)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
		ServAuxData, WorkerDataSend, Request, ResponseBuffer.data(), ResponseBuffer.size())))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int serv_worker_thread_func(const confmap_t &ServKeyVal,
	sp<ServAuxData> ServAuxData, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend)
{
	int r = 0;

	git_repository *Repository = NULL;
	git_repository *RepositorySelfUpdate = NULL;

	std::string ConfRefName;
	std::string ConfRefNameSelfUpdate;
	std::string ConfRepoOpenPath;
	std::string ConfRepoSelfUpdateOpenPath;

	if (!!(r = aux_config_key_ex(ServKeyVal, "RefName", &ConfRefName)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(ServKeyVal, "RefNameSelfUpdate", &ConfRefNameSelfUpdate)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(ServKeyVal, "ConfRepoOpenPath", &ConfRepoOpenPath)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(ServKeyVal, "ConfRepoSelfUpdateOpenPath", &ConfRepoSelfUpdateOpenPath)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(ConfRepoOpenPath.c_str(), &Repository)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(ConfRepoSelfUpdateOpenPath.c_str(), &RepositorySelfUpdate)))
		GS_GOTO_CLEAN();

	while (true) {
		sp<ServWorkerRequestData> Request;

		WorkerDataRecv->RequestDequeue(&Request);

		ENetPacket * const &Packet = *Request->mPacket;

		uint32_t OffsetStart = 0;
		uint32_t OffsetSize = 0;

		GsFrameType FoundFrameType = {};

		if (!!(r = aux_frame_read_frametype(Packet->data, Packet->dataLength, OffsetStart, &OffsetSize, &FoundFrameType)))
			GS_GOTO_CLEAN();

		printf("[worker] packet received [%.*s]\n", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);

		switch (FoundFrameType.mTypeNum)
		{
		case GS_FRAME_TYPE_REQUEST_LATEST_COMMIT_TREE:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid CommitHeadOid = {};
			git_oid TreeHeadOid = {};

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(Repository, ConfRefName.c_str(), &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_latest_commit_tree(&ResponseBuffer, TreeHeadOid.id, GIT_OID_RAWSZ)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(), ResponseBuffer.data(), ResponseBuffer.size())))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_TREELIST:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid TreeOid = {};
			std::vector<git_oid> Treelist;

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, &TreeOid)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_oid_treelist(Repository, &TreeOid, &Treelist)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_treelist(&ResponseBuffer, &Treelist)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(), ResponseBuffer.data(), ResponseBuffer.size())))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_TREES:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			uint32_t LengthLimit = 0;
			std::vector<git_oid> TreelistRequested;
			std::string SizeBufferTree;
			std::string ObjectBufferTree;

			if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &TreelistRequested)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_serialize_trees(Repository, &TreelistRequested, &SizeBufferTree, &ObjectBufferTree)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_trees(&ResponseBuffer, TreelistRequested.size(), &SizeBufferTree, &ObjectBufferTree)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(), ResponseBuffer.data(), ResponseBuffer.size())))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_BLOBS:
		{
			if (!!(r = aux_serv_worker_thread_service_request_blobs(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(),
				Packet, OffsetSize, Repository, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_BLOBS_SELFUPDATE:
		{
			if (!!(r = aux_serv_worker_thread_service_request_blobs(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(),
				Packet, OffsetSize, RepositorySelfUpdate, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS_SELFUPDATE))))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_LATEST_SELFUPDATE_BLOB:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid CommitHeadOid = {};
			git_oid TreeHeadOid = {};
			git_oid BlobSelfUpdateOid = {};

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(RepositorySelfUpdate, ConfRefNameSelfUpdate.c_str(), &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_oid_tree_blob_byname(RepositorySelfUpdate, &TreeHeadOid, GS_STR_PARENT_EXPECTED_SUFFIX, &BlobSelfUpdateOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_latest_selfupdate_blob(&ResponseBuffer, BlobSelfUpdateOid.id, GIT_OID_RAWSZ)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(), ResponseBuffer.data(), ResponseBuffer.size())))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		default:
		{
			printf("[worker] unknown frametype received [%.*s]\n", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);
			if (1)
				GS_ERR_CLEAN(1);
		}
		break;
		}

	}

clean:
	if (RepositorySelfUpdate)
		git_repository_free(RepositorySelfUpdate);

	if (Repository)
		git_repository_free(Repository);

	return r;
}

int clnt_worker_thread_func(const confmap_t &ClntKeyVal,
	sp<ServAuxData> ServAuxData, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend,
	ENetHost *clnt, ENetPeer *peer)
{
	int r = 0;

	sp<ClntState> State(new ClntState);

	gs_packet_unique_t NullPacket;
	sp<ServWorkerRequestData> RequestForSend(new ServWorkerRequestData(&NullPacket, clnt, peer));

	if (!!(r = clnt_state_make_default(State.get())))
		GS_GOTO_CLEAN();

	while (true) {
		if (!!(r = clnt_state_crank(State, ClntKeyVal,
			ServAuxData, WorkerDataRecv.get(), WorkerDataSend.get(),
			RequestForSend.get())))
		{
			GS_GOTO_CLEAN();
		}
	}

clean:

	return r;
}

int aux_enet_host_create_serv(uint32_t EnetAddressPort, ENetHost **oServer) {
	int r = 0;

	ENetAddress address = {};
	ENetHost *host = NULL;

	address.host = ENET_HOST_ANY;
	address.port = EnetAddressPort;

	if (!(host = enet_host_create(&address, 256, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (oServer)
		*oServer = host;

clean:
	if (!!r) {
		if (host)
			enet_host_destroy(host);
	}

	return r;
}

int aux_enet_host_client_create_addr(ENetHost **oHost, ENetAddress *oAddressHost) {
	/**
	* https://msdn.microsoft.com/en-us/library/windows/desktop/ms738543(v=vs.85).aspx
	* To apply the INTERRUPT_REQUESTED workaround to a socket,
	* a local connection (from a second socket) is required.
	* To establish that local connection, the socket must have an address assigned.
	* Address assignment occurs if the first socket is bound,
	* either explicitly (call to bind) or 'implicitly'
	* (call to connect, (also recvfrom and some others?)).
	* The socket being bound allows its address to be retrieved via getsockname.
	* enet codepath through enet_host_create only calls bind and getsockname if
	* the ENetAddress parameter is specified (non-null).
	*
	* As we are creating a client socket, we do not wish to bind to a specific port.
	* (to allow multiple client connections be established from the same host, for example)
	* To 'request' a dynamically assigned port via the ENetAddress structure,
	* set port value as ENET_PORT_ANY (aka zero).
	*
	* Once enet_host_create completes, the assigned port can be retrieved via
	* ENetHost->address.
	*/

	int r = 0;

	ENetAddress address = {};
	ENetHost *host = NULL;

	uint32_t ClntHostIp = ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24);
	// FIXME: want ENET_HOST_ANY (0) but then host->address will also have 0 as host
	//   whereas I'll need a connectable host for client's servaux
	//address.host = ENET_HOST_ANY;
	address.host = ClntHostIp;
	address.port = ENET_PORT_ANY;

	// FIXME: two peer limit (for connection, and for INTERRUPT_REQUESTED workaround local connection)
	if (!(host = enet_host_create(&address, 2, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (oHost)
		*oHost = host;

	if (oAddressHost)
		*oAddressHost = host->address;

clean:
	if (!!r) {
		if (host)
			enet_host_destroy(host);
	}

	return r;
}

int aux_enet_host_connect_addr(ENetHost *host, ENetAddress *address, ENetPeer **oPeer) {
	int r = 0;

	ENetPeer *peer = NULL;

	if (!(peer = enet_host_connect(host, address, 1, NULL)))
		GS_GOTO_CLEAN();

	if (oPeer)
		*oPeer = peer;

clean:
	if (!!r) {
		if (peer)
			enet_peer_disconnect_now(peer, NULL);
	}

	return r;
}

int aux_enet_host_create_connect_addr(
	ENetAddress *address,
	ENetHost **oHost, ENetPeer **oPeer)
{
	int r = 0;

	ENetHost *host = NULL;
	ENetPeer *peer = NULL;

	if (!(host = enet_host_create(NULL, 1, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (!(peer = enet_host_connect(host, address, 1, 0)))
		GS_ERR_CLEAN(1);

	if (oHost)
		*oHost = host;

	if (oPeer)
		*oPeer = peer;

clean:
	if (!!r) {
		if (peer)
			enet_peer_disconnect_now(peer, NULL);

		if (host)
			enet_host_destroy(host);
	}

	return r;
}

int aux_enet_address_create_ip(
	uint32_t EnetAddressPort, uint32_t EnetAddressHostNetworkByteOrder,
	ENetAddress *oAddress)
{
	ENetAddress address;

	address.host = EnetAddressHostNetworkByteOrder;
	address.port = EnetAddressPort;

	if (oAddress)
		*oAddress = address;

	return 0;
}

int aux_enet_address_create_hostname(
	uint32_t EnetAddressPort, const char *EnetHostName,
	ENetAddress *oAddress)
{
	int r = 0;

	ENetAddress address = {};

	if (!!(r = enet_address_set_host(&address, EnetHostName)))
		GS_ERR_CLEAN(1);
	address.port = EnetAddressPort;

	if (oAddress)
		*oAddress = address;

clean:

	return r;
}

int aux_packet_bare_send(ENetHost *host, ENetPeer *peer, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags) {
	int r = 0;

	ENetPacket *packet = NULL;

	/* only flag expected to be useful with this function is ENET_PACKET_FLAG_RELIABLE, really */
	assert((EnetPacketFlags & ~(ENET_PACKET_FLAG_RELIABLE)) == 0);

	if (!(packet = enet_packet_create(Data, DataSize, EnetPacketFlags)))
		GS_ERR_CLEAN(1);

	if (!!(r = enet_peer_send(peer, 0, packet)))
		GS_GOTO_CLEAN();
	packet = NULL;  /* lost ownership after enet_peer_send */

	enet_host_flush(host);

clean:
	if (packet)
		enet_packet_destroy(packet);

	return r;
}

int aux_packet_full_send(ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags) {
	int r = 0;

	if (!!(aux_packet_bare_send(host, peer, Data, DataSize, EnetPacketFlags)))
		GS_GOTO_CLEAN();

	ServAuxData->InterruptRequestedEnqueue();

clean:

	return r;
}

int aux_packet_response_queue_interrupt_request_reliable(ServAuxData *ServAuxData, ServWorkerData *WorkerDataSend, ServWorkerRequestData *Request, const char *Data, uint32_t DataSize) {
	int r = 0;

	ENetPacket *Packet = NULL;
	gs_packet_unique_t GsPacket;
	sp<ServWorkerRequestData> RequestResponseData;

	if (!(Packet = enet_packet_create(Data, DataSize, ENET_PACKET_FLAG_RELIABLE)))
		GS_ERR_CLEAN(1);

	GsPacket = aux_gs_make_packet_unique(Packet);
	Packet = NULL; /* lost ownership */

	if (!!(r = aux_make_serv_worker_request_data_for_response(Request, &GsPacket, &RequestResponseData)))
		GS_GOTO_CLEAN();

	WorkerDataSend->RequestEnqueue(RequestResponseData);

	ServAuxData->InterruptRequestedEnqueue();

clean:
	if (!!r) {
		if (Packet)
			enet_packet_destroy(Packet);
	}

	return r;
}

int aux_packet_request_dequeue(ServWorkerData *WorkerDataRecv, sp<ServWorkerRequestData> *oRequestForRecv) {
	int r = 0;

	sp<ServWorkerRequestData> RequestForRecv;

	WorkerDataRecv->RequestDequeue(&RequestForRecv);

	if (RequestForRecv->isReconnectRequest())
		GS_ERR_CLEAN(1);

	if (oRequestForRecv)
		*oRequestForRecv = RequestForRecv;

clean:

	return r;
}

int aux_packet_request_dequeue_packet(ServWorkerData *WorkerDataRecv, gs_packet_unique_t *oPacket) {
	int r = 0;

	sp<ServWorkerRequestData> RequestForRecv;

	if (!!(r = aux_packet_request_dequeue(WorkerDataRecv, &RequestForRecv)))
		GS_GOTO_CLEAN();

	if (oPacket)
		*oPacket = std::move(RequestForRecv->mPacket);

clean:

	return r;
}

int aux_host_service_one_type_receive(ENetHost *host, uint32_t TimeoutMs, gs_packet_t *oPacket) {
	/* NOTE: special errorhandling */

	int r = 0;

	int retcode = 0;
	ENetEvent event = {};
	gs_packet_t Packet;

	retcode = enet_host_service(host, &event, TimeoutMs);

	if (retcode > 0 && event.type != ENET_EVENT_TYPE_RECEIVE)
		GS_ERR_CLEAN(1);

	Packet = aux_gs_make_packet(event.packet);
	// FIXME: really allowed to set fields within an ENetPacket?
	event.packet = NULL; /* lost ownership */

	if (oPacket)
		*oPacket = Packet;

clean:
	if (event.packet)
		enet_packet_destroy(event.packet);

	return r;
}

int aux_host_service(ENetHost *host, uint32_t TimeoutMs, std::vector<ENetEvent> *oEvents) {
	/* http://lists.cubik.org/pipermail/enet-discuss/2012-June/001927.html */

	/* NOTE: special errorhandling */

	int retcode = 0;
	ENetEvent event = {};
	std::vector<ENetEvent> Events;

	for ((retcode = enet_host_service(host, &event, TimeoutMs))
		; retcode > 0
		; (retcode = enet_host_check_events(host, &event)))
	{
		// FIXME: copies an ENetEvent structure. not sure if part of the official enet API.
		Events.push_back(event);
	}

	if (oEvents)
		oEvents->swap(Events);

	return retcode < 0;
}

/* FIXME: race condition between server startup and client connection.
*   connect may send packet too early to be seen. subsequently enet_host_service call here will timeout.
*   the fix is having the connect be retried multiple times. */
int aux_host_connect_ensure_timeout(ENetHost *client, uint32_t TimeoutMs, uint32_t *oHasTimedOut) {
	int r = 0;

	int retcode = 0;
	ENetEvent event = {};

	if ((retcode = enet_host_service(client, &event, TimeoutMs)) < 0)
		GS_ERR_CLEAN(1);

	assert(retcode >= 0);

	if (retcode > 0 && event.type != ENET_EVENT_TYPE_CONNECT)
		GS_ERR_CLEAN(2);

	if (oHasTimedOut)
		*oHasTimedOut = (retcode == 0);

clean:

	return r;
}

int aux_host_connect(
	ENetAddress *address,
	uint32_t NumRetry, uint32_t RetryTimeoutMs,
	ENetHost **oClient, ENetPeer **oPeer)
{
	int r = 0;

	ENetHost *nontimedout_client = NULL;
	ENetPeer *nontimedout_peer = NULL;

	for (uint32_t i = 0; i < NumRetry; i++) {
		ENetHost *client = NULL;
		ENetPeer *peer = NULL;
		uint32_t HasTimedOut = 0;

		if (!!(r = aux_enet_host_create_connect_addr(address, &client, &peer)))
			GS_GOTO_CLEANSUB();

		if (!!(r = aux_host_connect_ensure_timeout(client, RetryTimeoutMs, &HasTimedOut)))
			GS_GOTO_CLEANSUB();

		if (!HasTimedOut) {
			nontimedout_client = client;
			nontimedout_peer = peer;
			break;
		}

	cleansub:
		if (!!r || HasTimedOut) {
			if (peer)
				enet_peer_disconnect_now(peer, NULL);
			if (client)
				enet_host_destroy(client);
		}
		if (!!r)
			GS_GOTO_CLEAN();
	}

	if (!nontimedout_client || !nontimedout_peer)
		GS_ERR_CLEAN(1);

	if (oClient)
		*oClient = nontimedout_client;

	if (oPeer)
		*oPeer = nontimedout_peer;

clean:
	if (!!r) {
		if (nontimedout_peer)
			enet_peer_disconnect_now(nontimedout_peer, NULL);
		if (nontimedout_client)
			enet_host_destroy(nontimedout_client);
	}

	return r;
}

int aux_selfupdate_basic(const char *HostName, const char *FileNameAbsoluteSelfUpdate, uint32_t *oHaveUpdate, std::string *oBufferUpdate) {
	int r = 0;

	uint32_t HaveUpdate = 0;
	std::string BufferUpdate;

	git_repository *RepositoryMemory = NULL;

	ENetAddress address = {};
	ENetHost *host = NULL;
	ENetPeer *peer = NULL;

	std::string Buffer;
	gs_packet_t GsPacketBlobOid;
	gs_packet_t GsPacketBlob;
	uint32_t Offset = 0;
	uint32_t DataLengthLimit = 0;

	git_oid BlobSelfUpdateOidT = {};

	std::vector<git_oid> BlobSelfUpdateOidVec(1, {});
	git_oid * const &BlobSelfUpdateOid = &BlobSelfUpdateOidVec.at(0);

	uint32_t BlobPairedVecLen = 0;
	uint32_t BlobOffsetSizeBuffer = 0;
	uint32_t BlobOffsetObjectBuffer = 0;

	if (!!(r = aux_memory_repository_new(&RepositoryMemory)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_address_create_hostname(GS_PORT, HostName, &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &host, &peer)))
		GS_GOTO_CLEAN_L(E, PF, "failure connecting [host=[%s]]", HostName);

	if (!!(r = aux_frame_full_write_request_latest_selfupdate_blob(&Buffer)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_bare_send(host, peer, Buffer.data(), Buffer.size(), ENET_PACKET_FLAG_RELIABLE)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_service_one_type_receive(host, GS_RECEIVE_TIMEOUT_MS, &GsPacketBlobOid)))
		GS_GOTO_CLEAN();

	ENetPacket * const &PacketBlobOid = *GsPacketBlobOid;

	Offset = 0;

	if (!!(r = aux_frame_ensure_frametype(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_SELFUPDATE_BLOB))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, BlobSelfUpdateOid)))
		GS_GOTO_CLEAN();

	/* empty as_path parameter means no filters applied */
	if (!!(r = git_repository_hashfile(&BlobSelfUpdateOidT, RepositoryMemory, FileNameAbsoluteSelfUpdate, GIT_OBJ_BLOB, "")))
		GS_GOTO_CLEAN_L(E, PF, "failure hashing [filename=[%s]]", FileNameAbsoluteSelfUpdate);

	if (git_oid_cmp(&BlobSelfUpdateOidT, BlobSelfUpdateOid) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &BlobSelfUpdateOidT);
		GS_LOG(I, PF, "have latest [oid=[%.*s]]", GIT_OID_HEXSZ, buf);
	}

	if (!!(r = aux_frame_full_write_request_blobs_selfupdate(&Buffer, &BlobSelfUpdateOidVec)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_bare_send(host, peer, Buffer.data(), Buffer.size(), ENET_PACKET_FLAG_RELIABLE)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_service_one_type_receive(host, GS_RECEIVE_TIMEOUT_MS, &GsPacketBlob)))
		GS_GOTO_CLEAN();

	ENetPacket * const &PacketBlob = *GsPacketBlob;

	Offset = 0;

	if (!!(r = aux_frame_ensure_frametype(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS_SELFUPDATE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &DataLengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(
		PacketBlob->data, DataLengthLimit, Offset, &Offset,
		&BlobPairedVecLen, &BlobOffsetSizeBuffer, &BlobOffsetObjectBuffer)))
	{
		GS_GOTO_CLEAN();
	}

	if (BlobPairedVecLen != 1)
		GS_ERR_CLEAN(1);

	{
		uint32_t BlobZeroSize = 0;
		git_oid BlobZeroOid = {};
		
		git_blob *BlobZero = NULL;
		git_buf BlobZeroBuf = {};

		aux_LE_to_uint32(&BlobZeroSize, (char *)(PacketBlob->data + BlobOffsetSizeBuffer), GS_FRAME_SIZE_LEN);

		if (!!(r = git_blob_create_frombuffer(&BlobZeroOid, RepositoryMemory, PacketBlob->data + BlobOffsetObjectBuffer, BlobZeroSize)))
			GS_GOTO_CLEANSUB();

		if (!!(r = git_blob_lookup(&BlobZero, RepositoryMemory, &BlobZeroOid)))
			GS_GOTO_CLEANSUB();

		/* wtf? was the wrong blob sent? */
		if (git_oid_cmp(&BlobZeroOid, BlobSelfUpdateOid) != 0)
			GS_ERR_CLEANSUB(1);

		// FIXME: git_blob_filtered_content: actually this whole API is trash.
		//   - it is not clear if empty string passed as 'path' parameter is ok.
		//   - 'check_for_binary_data' MUST ALWAYS BE ZERO PLEASE - according to current libgit2 source
		//   - freeing the buffer before freeing the blob seems to be the right thing in all cases?
		// FIXME: this actually does filter the content!
		//   file server side: "dummy\r\n"; blob server side: "dummy\n"; blob filtered client side: "dummy\r\n"
		if (!!(r = git_blob_filtered_content(&BlobZeroBuf, BlobZero, "", 0)))
			GS_GOTO_CLEANSUB();

		HaveUpdate = 1;
		BufferUpdate = std::string(BlobZeroBuf.ptr, BlobZeroBuf.size);

	cleansub:

		git_buf_free(&BlobZeroBuf);

		if (BlobZero)
			git_blob_free(BlobZero);

		if (!!r)
			GS_GOTO_CLEAN();
	}

	if (oHaveUpdate)
		*oHaveUpdate = HaveUpdate;

	if (oBufferUpdate)
		oBufferUpdate->swap(BufferUpdate);

clean:
	if (peer)
		enet_peer_disconnect_now(peer, NULL);

	if (host)
		enet_host_destroy(host);

	if (RepositoryMemory)
		git_repository_free(RepositoryMemory);

	return r;
}

int aux_serv_aux_host_service(ENetHost *client) {
	int r = 0;

	std::vector<ENetEvent> Events;

	if (!!(r = aux_host_service(client, 0, &Events)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < Events.size(); i++) {
		switch (Events[i].type)
		{
		case ENET_EVENT_TYPE_CONNECT:
		case ENET_EVENT_TYPE_DISCONNECT:
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			assert(0);
			enet_packet_destroy(Events[i].packet);
			break;
		}
	}

clean:

	return r;
}

int aux_serv_aux_thread_func(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, ENetAddress address /* by val */) {
	int r = 0;

	std::string BufferFrameInterruptRequested;

	ENetHost *client = NULL;
	ENetPeer *peer = NULL;

	if (!!(r = aux_frame_full_write_serv_aux_interrupt_requested(&BufferFrameInterruptRequested)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &client, &peer)))
		GS_GOTO_CLEAN();

	while (true) {

		if (!!(r = aux_serv_aux_host_service(client)))
			GS_GOTO_CLEAN();

		/* set a timeout to ensure serv_aux_host_service cranks the enet event loop regularly */

		bool IsInterruptRequested = ServAuxData->InterruptRequestedDequeueTimeout(
			std::chrono::milliseconds(GS_SERV_AUX_ARBITRARY_TIMEOUT_MS));

		if (IsInterruptRequested) {
			/* NOTE: searching enet source for uses of ENET_PACKET_FLAG_NO_ALLOCATE turns out a fun easter egg:
			*   enet_packet_resize essentially chokes and sets new size without validation so never call that */

			ENetPacket *packet = enet_packet_create(
				BufferFrameInterruptRequested.data(), BufferFrameInterruptRequested.size(), ENET_PACKET_FLAG_NO_ALLOCATE);

			/* NOTE: enet tutorial claims that enet_packet_destroy need not be called after packet handoff via enet_peer_send.
			*   but reading enet source reveals obvious leaks on some error conditions. (undistinguishable observing return code) */

			if (enet_peer_send(peer, 0, packet) < 0)
				GS_ERR_CLEAN(1);

			/* enet packet sends are ensured sufficiently by enet_host_flush. only receives require serv_aux_host_service.
			* however serv_aux send only, does not really receive anything. serv_aux_host_service just helps crank internal
			* enet state such as acknowledgment packets.
			* FIXME: refactor to only call serv_aux_host_service every GS_SERV_AUX_ARBITRARY_TIMEOUT_MS ms
			*   instead of after every IsInterruptRequested. */

			enet_host_flush(client);
		}
	}

clean:

	return r;
}

int serv_serv_aux_thread_func(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;

	uint32_t ServPort = 0;
	uint32_t ServHostIp = ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24);
	ENetAddress address = {};

	if (!!(r = aux_config_key_uint32(ServKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	assert(ServHostIp == ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24));

	if (!!(r = aux_enet_address_create_ip(ServPort, ServHostIp, &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_serv_aux_thread_func(ServKeyVal, ServAuxData, address)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_serv_aux_thread_func(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, ENetAddress address /* by val */) {
	return aux_serv_aux_thread_func(ServKeyVal, ServAuxData, address);
}

int aux_serv_host_service(ENetHost *server, const sp<ServWorkerData> &WorkerDataRecv, const sp<ServWorkerData> &WorkerDataSend) {
	int r = 0;

	std::vector<ENetEvent> Events;

	if (!!(r = aux_host_service(server, GS_SERV_AUX_ARBITRARY_TIMEOUT_MS, &Events)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < Events.size(); i++) {
		switch (Events[i].type)
		{
		case ENET_EVENT_TYPE_CONNECT:
		{
			printf("[serv] A new client connected from %x:%u.\n",
				Events[i].peer->address.host,
				Events[i].peer->address.port);
			Events[i].peer->data = "Client information";
		}
		break;

		case ENET_EVENT_TYPE_RECEIVE:
		{
			ENetPeer *peer = Events[i].peer;

			const GsFrameType &FrameTypeInterruptRequested = GS_FRAME_TYPE_DECL(SERV_AUX_INTERRUPT_REQUESTED);
			GsFrameType FoundFrameType = {};

			if (!!(r = aux_frame_read_frametype(Events[i].packet->data, Events[i].packet->dataLength, 0, NULL, &FoundFrameType)))
				GS_GOTO_CLEAN();

			/* filter out interrupt requested frames and only dispatch other */

			if (! aux_frametype_equals(FoundFrameType, FrameTypeInterruptRequested)) {

				printf("[serv] packet received\n");

				gs_packet_unique_t Packet = aux_gs_make_packet_unique(Events[i].packet);

				sp<ServWorkerRequestData> ServWorkerRequestData;

				if (!!(r = aux_make_serv_worker_request_data(server, peer, &Packet, &ServWorkerRequestData)))
					GS_GOTO_CLEAN();

				WorkerDataRecv->RequestEnqueue(ServWorkerRequestData);
			}

			/* check out if any send requests need servicing */

			{
				std::deque<sp<ServWorkerRequestData> > RequestedSends;

				WorkerDataSend->RequestDequeueAllOpt(&RequestedSends);

				for (uint32_t i = 0; i < RequestedSends.size(); i++) {
					ENetHost *GotHost = NULL;
					ENetPeer *GotPeer = NULL;

					aux_get_serv_worker_request_private(RequestedSends[i].get(), &GotHost, &GotPeer);

					// FIXME: assuming reconnection is a thing, how to assure host and peer are still valid?
					//   likely want to clear outstanding requests on the worker queues before a reconnect.
					//   for now at least assure host from the request is the same as passed to this function.
					assert(GotHost == server);

					/* ownership of packet is lost after enet_peer_send */
					ENetPacket *Packet = *RequestedSends[i]->mPacket.release();

					if (enet_peer_send(GotPeer, 0, Packet) < 0)
						GS_GOTO_CLEAN();
				}

				/* absolutely no reason to flush if nothing was sent */
				/* notice we are flushing 'server', above find an assert equaling against RequestedSends host */

				if (RequestedSends.size())
					enet_host_flush(server);
			}
		}
		break;

		case ENET_EVENT_TYPE_DISCONNECT:
		{
			printf("[serv] %s disconnected.\n", Events[i].peer->data);
			Events[i].peer->data = NULL;
		}
		break;

		}
	}

clean:

	return r;
}

int aux_serv_thread_func(ENetHost *host, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend) {
	int r = 0;

	while (true) {
		if (!!(r = aux_serv_host_service(host, WorkerDataRecv, WorkerDataSend)))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int serv_serv_thread_func(const confmap_t &ServKeyVal, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend) {
	int r = 0;

	ENetHost *server = NULL;

	uint32_t ServPort = 0;

	if (!!(r = aux_config_key_uint32(ServKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_host_create_serv(ServPort, &server)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_serv_thread_func(server, WorkerDataRecv, WorkerDataSend)))
		GS_GOTO_CLEAN();

clean:
	if (server)
		enet_host_destroy(server);

	return r;
}

int clnt_serv_thread_func(const confmap_t &ClntKeyVal, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend, ENetHost *host) {
	int r = 0;

	if (!!(r = aux_serv_thread_func(host, WorkerDataRecv, WorkerDataSend)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_reconnect_make_default(ClntStateReconnect *oStateReconnect) {
	ClntStateReconnect StateReconnect;
	StateReconnect.NumReconnections = GS_CONNECT_NUMRECONNECT;
	StateReconnect.NumReconnectionsLeft = StateReconnect.NumReconnections;
	if (oStateReconnect)
		*oStateReconnect = StateReconnect;
	return 0;
}

int clnt_state_make_default(ClntState *oState) {
	ClntState State;
	if (oState)
		*oState = State;
	return 0;
}

int clnt_state_cpy(ClntState *dst, const ClntState *src) {
	*dst = *src;
	return 0;
}

int clnt_state_code(ClntState *State, uint32_t *oCode) {
	int r = 0;
	
	int Code = 0;

	if (! State->mRepositoryT)
		{ Code = 0; goto s0; }
	//if (0) /* FIXME: unused - refactor */
	//	{ Code = 1; goto s1; }
	if (! State->mTreeHeadOid)
		{ Code = 2; goto s2; }
	if (! State->mTreelist || ! State->mMissingTreelist)
		{ Code = 3; goto s3; }
	if (! State->mMissingBloblist || ! State->mTreePacketWithOffset)
		{ Code = 4; goto s4; }
	if (! State->mWrittenBlob || ! State->mWrittenTree)
		{ Code = 5; goto s5; }
	if (true)
		{ Code = 6; goto s6; }

s0:
	if (State->mTreeHeadOid)
		GS_ERR_CLEAN(1);
s2:
	if (State->mTreelist || State->mMissingTreelist)
		GS_ERR_CLEAN(1);
s3:
	if (State->mMissingBloblist || State->mTreePacketWithOffset)
		GS_ERR_CLEAN(1);
s4:
	if (State->mWrittenBlob || State->mWrittenTree)
		GS_ERR_CLEAN(1);
s5:
s6:

	if (oCode)
		*oCode = Code;

clean:

	return r;
}

int clnt_state_code_ensure(ClntState *State, uint32_t WantedCode) {
	int r = 0;

	uint32_t FoundCode = 0;

	if (!!(r = clnt_state_code(State, &FoundCode)))
		GS_GOTO_CLEAN();

	if (WantedCode != FoundCode)
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int clnt_state_connection_remake(const confmap_t &ClntKeyVal, sp<gs_host_peer_pair_t> *ioConnection) {
	int r = 0;

	std::string ConfServHostName;
	uint32_t ConfServPort = 0;

	ENetAddress address = {};
	ENetHost *newhost = NULL;
	ENetPeer *newpeer = NULL;

	if (!!(r = aux_config_key_ex(ClntKeyVal, "ConfServHostName", &ConfServHostName)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_config_key_uint32(ClntKeyVal, "ConfServPort", &ConfServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_peer_pair_reset(ioConnection)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_address_create_hostname(ConfServPort, ConfServHostName.c_str(), &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &newhost, &newpeer)))
		GS_GOTO_CLEAN();

	if (ioConnection)
		*ioConnection = std::make_shared<gs_host_peer_pair_t>(newhost, newpeer);

clean:
	if (!!r) {
		if (newpeer)
			enet_peer_disconnect_now(newpeer, NULL);

		if (newhost)
			enet_host_destroy(newhost);
	}

	return r;
}

int clnt_state_crank(
	const sp<ClntState> &State, const confmap_t &ClntKeyVal,
	const sp<ServAuxData> &ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend)
{
	int r = 0;

	uint32_t Code = 0;

	if (!!(r = clnt_state_code(State.get(), &Code)))
		GS_GOTO_CLEAN();

	switch (Code) {
	case 0:
	{
		if (!!(r = clnt_state_0_setup(State, ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}
	break;

	case 1:
	{
		assert(0); // FIXME: unused - refactor
		if (!!(r = clnt_state_1_setup(State, ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}
	break;

	case 2:
	{
		if (!!(r = clnt_state_2_setup(State, ClntKeyVal,
			ServAuxData.get(), WorkerDataRecv, WorkerDataSend, RequestForSend)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case 3:
	{
		if (!!(r = clnt_state_3_setup(State, ClntKeyVal,
			ServAuxData.get(), WorkerDataRecv, WorkerDataSend, RequestForSend)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case 4:
	{
		if (!!(r = clnt_state_4_setup(State, ClntKeyVal,
			ServAuxData.get(), WorkerDataRecv, WorkerDataSend, RequestForSend)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case 5:
	{
		if (!!(r = clnt_state_5_setup(State, ClntKeyVal,
			ServAuxData.get(), WorkerDataRecv, WorkerDataSend, RequestForSend)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	default:
	{
		assert(0);
	}
	break;
	}

clean:

	return r;
}

/* FIXME: presumably unused - refactor */
int clnt_state_crank_reconnecter(
	const sp<ClntState> &State, ClntStateReconnect *ioStateReconnect,
	const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData,
	ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend)
{
	assert(0);
	return 1;
	//	int r = 0;
	//
	//	if (!!(r = clnt_state_crank(State, ClntKeyVal, ServAuxData, WorkerDataRecv, WorkerDataSend))) {
	//		printf("reco+\n");
	//		if (ioStateReconnect->NumReconnectionsLeft-- == 0)
	//			GS_GOTO_CLEAN();
	//		if (!!(r = clnt_state_connection_remake(ClntKeyVal, &State->mConnection)))
	//			GS_GOTO_CLEAN();
	//		printf("reco-\n");
	//	}
	//
	//clean:
	//
	//	return r;
}

int clnt_state_0_noown(const char *ConfRepoTOpenPath, git_repository **oRepositoryT) {
	int r = 0;

	if (!!(r = aux_repository_open(ConfRepoTOpenPath, oRepositoryT)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_2_noown(
	const char *ConfRefName, git_repository *RepositoryT,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	git_oid *oTreeHeadOid)
{
	int r = 0;

	std::string Buffer;
	gs_packet_unique_t GsPacket;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	if (!!(r = aux_frame_full_write_request_latest_commit_tree(&Buffer)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(ServAuxData, WorkerDataSend, RequestForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_request_dequeue_packet(WorkerDataRecv, &GsPacket)))
		GS_GOTO_CLEAN();

	ENetPacket * const &Packet = *GsPacket;

	if (! Packet)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, oTreeHeadOid)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_latest_commit_tree_oid(RepositoryT, ConfRefName, &CommitHeadOidT, &TreeHeadOidT)))
		GS_GOTO_CLEAN();

	if (git_oid_cmp(&TreeHeadOidT, oTreeHeadOid) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &CommitHeadOidT);
		printf("[clnt] Have latest [%.*s]\n", GIT_OID_HEXSZ, buf);
	}

clean:

	return r;
}

int clnt_state_3_noown(
	git_repository *RepositoryT,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	git_oid *TreeHeadOid, std::vector<git_oid> *oTreelist, std::vector<git_oid> *oMissingTreelist)
{
	int r = 0;

	std::string Buffer;
	gs_packet_unique_t GsPacket;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	if (!!(r = aux_frame_full_write_request_treelist(&Buffer, TreeHeadOid->id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(ServAuxData, WorkerDataSend, RequestForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_request_dequeue_packet(WorkerDataRecv, &GsPacket)))
		GS_GOTO_CLEAN();

	ENetPacket * const &Packet = *GsPacket;

	if (! Packet)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREELIST))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, oTreelist)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_missing_trees(RepositoryT, oTreelist, oMissingTreelist)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_4_noown(
	git_repository *RepositoryT,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	std::vector<git_oid> *MissingTreelist,
	std::vector<git_oid> *oMissingBloblist,
	gs_packet_unique_t *oPacketTree, uint32_t *oOffsetSizeBufferTree, uint32_t *oOffsetObjectBufferTree)
{
	int r = 0;

	std::string Buffer;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	uint32_t BufferTreeLen;

	if (!!(r = aux_frame_full_write_request_trees(&Buffer, MissingTreelist)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(ServAuxData, WorkerDataSend, RequestForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketTree Lifetime start */

	if (!!(r = aux_packet_request_dequeue_packet(WorkerDataRecv, oPacketTree)))
		GS_GOTO_CLEAN();

	ENetPacket * const &PacketTree = **oPacketTree;

	if (! PacketTree)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(PacketTree->data, PacketTree->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREES))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(PacketTree->data, PacketTree->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketTree Offsets use start */

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketTree->data, LengthLimit, Offset, &Offset,
		&BufferTreeLen, oOffsetSizeBufferTree, oOffsetObjectBufferTree)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_missing_blobs_bare(
		RepositoryT,
		PacketTree->data, LengthLimit, *oOffsetSizeBufferTree,
		PacketTree->data, LengthLimit, *oOffsetObjectBufferTree, MissingTreelist->size(), oMissingBloblist)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int clnt_state_5_noown(
	git_repository *RepositoryT,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	std::vector<git_oid> *MissingTreelist, std::vector<git_oid> *MissingBloblist,
	const gs_packet_unique_t &GsPacketTree, uint32_t OffsetSizeBufferTree, uint32_t OffsetObjectBufferTree,
	std::vector<git_oid> *oWrittenBlob, std::vector<git_oid> *oWrittenTree)
{
	int r = 0;

	std::string Buffer;
	gs_packet_unique_t GsPacketBlob;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	uint32_t BufferBlobLen;
	uint32_t OffsetSizeBufferBlob;
	uint32_t OffsetObjectBufferBlob;

	if (!!(r = aux_frame_full_write_request_blobs(&Buffer, MissingBloblist)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(ServAuxData, WorkerDataSend, RequestForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketBlob Lifetime start */

	if (!!(r = aux_packet_request_dequeue_packet(WorkerDataRecv, &GsPacketBlob)))
		GS_GOTO_CLEAN();

	ENetPacket * const &PacketBlob = *GsPacketBlob;

	if (! PacketBlob)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketBlob Offsets use start */

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketBlob->data, LengthLimit, Offset, &Offset,
		&BufferBlobLen, &OffsetSizeBufferBlob, &OffsetObjectBufferBlob)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_deserialize_blobs(
		RepositoryT,
		PacketBlob->data, LengthLimit, OffsetSizeBufferBlob,
		PacketBlob->data, LengthLimit, OffsetObjectBufferBlob,
		MissingBloblist->size(), oWrittenBlob)))
	{
		GS_GOTO_CLEAN();
	}

	ENetPacket * const &PacketTree = *GsPacketTree;

	// FIXME: using full size (PacketTree->dataLength) instead of LengthLimit of PacketTree (NOT of PacketBlob!)
	if (!!(r = clnt_deserialize_trees(
		RepositoryT,
		PacketTree->data, PacketTree->dataLength, OffsetSizeBufferTree,
		PacketTree->data, PacketTree->dataLength, OffsetObjectBufferTree,
		MissingTreelist->size(), oWrittenTree)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int clnt_state_0_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	sp<git_repository *> RepositoryT(new git_repository *);

	std::string ConfRepoTOpenPath;

	if (!!(r = aux_config_key_ex(ClntKeyVal, "ConfRepoTOpenPath", &ConfRepoTOpenPath)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_state_0_noown(ConfRepoTOpenPath.c_str(), RepositoryT.get())))
		GS_GOTO_CLEAN();

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 2, a,
		{ a.mRepositoryT = RepositoryT; });

clean:
	if (!!r) {
		if (RepositoryT)
			git_repository_free(*RepositoryT);
	}

	return r;
}

int clnt_state_1_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 2, a,
		{ });

clean:

	return r;
}

int clnt_state_2_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend, ServWorkerRequestData *RequestForSend)
{
	int r = 0;

	sp<git_oid> TreeHeadOid(new git_oid);

	git_repository * const RepositoryT = *State->mRepositoryT;

	std::string ConfRefName;

	std::string Buffer;
	gs_packet_t Packet;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	if (!!(r = aux_config_key_ex(ClntKeyVal, "RefName", &ConfRefName)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_state_2_noown(
		ConfRefName.c_str(), RepositoryT,
		ServAuxData, WorkerDataRecv, WorkerDataSend, RequestForSend,
		TreeHeadOid.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 3, a,
		{ a.mTreeHeadOid = TreeHeadOid; });

clean:

	return r;
}

int clnt_state_3_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend, ServWorkerRequestData *RequestForSend)
{
	int r = 0;

	sp<std::vector<git_oid> > Treelist(new std::vector<git_oid>);
	sp<std::vector<git_oid> > MissingTreelist(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<git_oid> &TreeHeadOid = State->mTreeHeadOid;

	if (!!(r = clnt_state_3_noown(
		RepositoryT,
		ServAuxData, WorkerDataRecv, WorkerDataSend, RequestForSend,
		TreeHeadOid.get(), Treelist.get(), MissingTreelist.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 4, a,
		{ a.mTreelist = Treelist;
		  a.mMissingTreelist = MissingTreelist; });

clean:

	return r;
}

int clnt_state_4_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend, ServWorkerRequestData *RequestForSend)
{
	int r = 0;

	sp<std::vector<git_oid> > MissingBloblist(new std::vector<git_oid>);
	sp<PacketUniqueWithOffset> PacketTreeWithOffset(new PacketUniqueWithOffset);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<std::vector<git_oid> > &MissingTreelist = State->mMissingTreelist;

	gs_packet_unique_t PacketTree;

	uint32_t OffsetSizeBufferTree;
	uint32_t OffsetObjectBufferTree;

	sp<PacketUniqueWithOffset> TmpTreePacketWithOffset(new PacketUniqueWithOffset);

	if (!!(r = clnt_state_4_noown(
		RepositoryT,
		ServAuxData, WorkerDataRecv, WorkerDataSend, RequestForSend,
		MissingTreelist.get(), MissingBloblist.get(), &PacketTree, &OffsetSizeBufferTree, &OffsetObjectBufferTree)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_make_packet_unique_with_offset(&PacketTree, OffsetSizeBufferTree, OffsetObjectBufferTree, TmpTreePacketWithOffset.get())))
		GS_GOTO_CLEAN();

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 5, a,
		{ a.mMissingBloblist = MissingBloblist;
		  a.mTreePacketWithOffset = TmpTreePacketWithOffset; });

clean:

	return r;
}

int clnt_state_5_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend, ServWorkerRequestData *RequestForSend)
{
	int r = 0;

	sp<std::vector<git_oid> > WrittenBlob(new std::vector<git_oid>);
	sp<std::vector<git_oid> > WrittenTree(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<std::vector<git_oid> > &MissingTreelist = State->mMissingTreelist;
	const sp<std::vector<git_oid> > &MissingBloblist = State->mMissingBloblist;
	const sp<PacketUniqueWithOffset> &PacketTreeWithOffset = State->mTreePacketWithOffset;
	const gs_packet_unique_t &PacketTree = PacketTreeWithOffset->mPacket;
	const uint32_t &OffsetSizeBufferTree = PacketTreeWithOffset->mOffsetSize;
	const uint32_t &OffsetObjectBufferTree = PacketTreeWithOffset->mOffsetObject;

	if (!!(r = clnt_state_5_noown(
		RepositoryT,
		ServAuxData, WorkerDataRecv, WorkerDataSend, RequestForSend,
		MissingTreelist.get(), MissingBloblist.get(),
		PacketTree, OffsetSizeBufferTree, OffsetObjectBufferTree,
		WrittenBlob.get(), WrittenTree.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 6, a,
		{ a.mWrittenBlob = WrittenBlob;
		  a.mWrittenTree = WrittenTree; });

clean:

	return r;
}

void serv_worker_thread_func_f(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend) {
	int r = 0;
	if (!!(r = serv_worker_thread_func(ServKeyVal, ServAuxData, WorkerDataRecv, WorkerDataSend)))
		assert(0);
	for (;;) {}
}

void serv_serv_aux_thread_func_f(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;
	if (!!(r = serv_serv_aux_thread_func(ServKeyVal, ServAuxData)))
		assert(0);
	for (;;) {}
}

void serv_thread_func_f(const confmap_t &ServKeyVal, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend) {
	int r = 0;
	if (!!(r = serv_serv_thread_func(ServKeyVal, WorkerDataRecv, WorkerDataSend)))
		assert(0);
	for (;;) {}
}

void clnt_worker_thread_func_f(const confmap_t &ServKeyVal,
	sp<ServAuxData> ServAuxData, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend,
	ENetHost *clnt, ENetPeer *peer)
{
	int r = 0;
	if (!!(r = clnt_worker_thread_func(ServKeyVal, ServAuxData, WorkerDataRecv, WorkerDataSend, clnt, peer)))
		assert(0);
	for (;;) {}
}

void clnt_serv_aux_thread_func_f(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, ENetAddress address /* by val */) {
	int r = 0;
	if (!!(r = aux_serv_aux_thread_func(ServKeyVal, ServAuxData, address)))
		assert(0);
	for (;;) {}
}

void clnt_thread_func_f(const confmap_t &ClntKeyVal, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend, ENetHost *host) {
	int r = 0;
	if (!!(r = clnt_serv_thread_func(ClntKeyVal, WorkerDataRecv, WorkerDataSend, host)))
		assert(0);
	for (;;) {}
}

int aux_full_create_connection_server(confmap_t ServKeyVal, sp<FullConnectionClient> *oConnectionClient) {
	int r = 0;

	sp<FullConnectionClient> ConnectionClient;

	{
		sp<ServWorkerData> WorkerDataSend(new ServWorkerData);
		sp<ServWorkerData> WorkerDataRecv(new ServWorkerData);
		sp<ServAuxData> ServAuxData(new ServAuxData);

		sp<std::thread> ServerWorkerThread(new std::thread(serv_worker_thread_func_f, ServKeyVal, ServAuxData, WorkerDataRecv, WorkerDataSend));
		sp<std::thread> ServerAuxThread(new std::thread(serv_serv_aux_thread_func_f, ServKeyVal, ServAuxData));
		sp<std::thread> ServerThread(new std::thread(serv_thread_func_f, ServKeyVal, WorkerDataRecv, WorkerDataSend));

		ConnectionClient = sp<FullConnectionClient>(new FullConnectionClient(ServerWorkerThread, ServerAuxThread, ServerThread));
	}

	if (oConnectionClient)
		*oConnectionClient = ConnectionClient;

clean:

	return r;
}

int aux_full_create_connection_client(confmap_t ClntKeyVal, sp<FullConnectionClient> *oConnectionClient) {
	int r = 0;

	sp<FullConnectionClient> ConnectionClient;

	uint32_t ServPort = 0;

	ENetHost *clnt = NULL;
	ENetAddress AddressClnt = {};
	ENetAddress AddressServ = {};
	ENetPeer *peer = NULL;

	if (!!(r = aux_config_key_uint32(ClntKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_host_client_create_addr(&clnt, &AddressClnt)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_address_create_hostname(ServPort, "localhost", &AddressServ)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_host_connect_addr(clnt, &AddressServ, &peer)))
		GS_GOTO_CLEAN();

	{
		sp<ServWorkerData> WorkerDataSend(new ServWorkerData);
		sp<ServWorkerData> WorkerDataRecv(new ServWorkerData);
		sp<ServAuxData> ServAuxData(new ServAuxData);

		sp<std::thread> ClientWorkerThread(new std::thread(clnt_worker_thread_func_f, ClntKeyVal, ServAuxData, WorkerDataRecv, WorkerDataSend, clnt, peer));
		sp<std::thread> ClientAuxThread(new std::thread(clnt_serv_aux_thread_func_f, ClntKeyVal, ServAuxData, AddressClnt));
		sp<std::thread> ClientThread(new std::thread(clnt_thread_func_f, ClntKeyVal, WorkerDataRecv, WorkerDataSend, clnt));

		ConnectionClient = sp<FullConnectionClient>(new FullConnectionClient(ClientWorkerThread, ClientAuxThread, ClientThread));
	}

	if (oConnectionClient)
		*oConnectionClient = ConnectionClient;

clean:

	return r;
}

int stuff2() {
	int r = 0;

	confmap_t ServKeyVal;
	confmap_t ClntKeyVal;

	sp<FullConnectionClient> FcsServ;
	sp<FullConnectionClient> FcsClnt;

	if (!!(r = aux_config_read("../data/", "gittest_config_serv.conf", &ServKeyVal)))
		GS_GOTO_CLEAN();
	ClntKeyVal = ServKeyVal;

	if (!!(r = aux_full_create_connection_server(ServKeyVal, &FcsServ)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_full_create_connection_client(ServKeyVal, &FcsClnt)))
		GS_GOTO_CLEAN();

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

clean:

	return r;
}
