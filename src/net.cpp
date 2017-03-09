#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

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
#include <map>

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

GsHostSurrogate::GsHostSurrogate(ENetHost *host)
	: mHost(host)
{}

GsConnectionSurrogate::GsConnectionSurrogate(ENetHost *host, ENetPeer *peer, uint32_t IsPrincipalClientConnection)
	: mIsValid(1),
	mIsPrincipalClientConnection(IsPrincipalClientConnection),
	mHost(host),
	mPeer(peer)
{}

void GsConnectionSurrogate::Invalidate() {
	mIsValid.store(0);
}

ServWorkerRequestData::ServWorkerRequestData(
	gs_packet_unique_t *ioPacket,
	uint32_t IsPrepare,
	uint32_t IsWithId,
	gs_connection_surrogate_id_t Id)
{
	mPacket = std::move(*ioPacket);
	mIsPrepare = IsPrepare;
	mIsWithId = IsWithId;
	mId = Id;
}

bool ServWorkerRequestData::isReconnectRequest() {
	return ! mPacket;
}

bool ServWorkerRequestData::isReconnectRequestPrepare() {
	return ! mPacket && mIsPrepare;
}

bool ServWorkerRequestData::isReconnectRequestRegular() {
	return ! mPacket && ! mIsPrepare;
}

bool ServWorkerRequestData::isReconnectRequestRegularWithId() {
	return ! mPacket && ! mIsPrepare && mIsWithId;
}

bool ServWorkerRequestData::isReconnectRequestRegularNoId() {
	return ! mPacket && ! mIsPrepare && ! mIsWithId;
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
		GS_ASSERT(! mWorkerQueue->empty());
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

ServAuxRequestData::ServAuxRequestData() {
	mIsReconnect = false;
	mIsPrepare = false;
	mAddress = {};
}

ServAuxRequestData::ServAuxRequestData(
	uint32_t IsReconnect,
	uint32_t IsPrepare,
	ENetAddress *AddressIfReconnectOpt)
{
	ENetAddress EmptyAddress = {};

	GS_ASSERT(! AddressIfReconnectOpt || (IsReconnect && ! IsPrepare));

	mIsReconnect = IsReconnect;
	mIsPrepare = IsPrepare;
	mAddress = AddressIfReconnectOpt ? *AddressIfReconnectOpt : EmptyAddress;

}

bool ServAuxRequestData::IsReconnectRequest() {
	return !!mIsReconnect;
}

bool ServAuxRequestData::isReconnectRequestPrepare() {
	return mIsReconnect && mIsPrepare;
}

bool ServAuxRequestData::isReconnectRequestRegular() {
	return mIsReconnect && ! mIsPrepare;
}

ServAuxData::ServAuxData()
	: mAuxQueue(new std::deque<ServAuxRequestData>),
	mAuxDataMutex(new std::mutex),
	mAuxDataCond(new std::condition_variable)
{}

void ServAuxData::InterruptRequestedEnqueue() {
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		mAuxQueue->push_back(ServAuxRequestData(false, false, NULL));
	}
	mAuxDataCond->notify_one();
}

void ServAuxData::InterruptRequestedEnqueueData(ServAuxRequestData RequestData) {
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		mAuxQueue->push_back(RequestData);
	}
	mAuxDataCond->notify_one();
}

void ServAuxData::InterruptRequestedDequeue(ServAuxRequestData *oRequestData) {
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		mAuxDataCond->wait(lock, [&]() { return !mAuxQueue->empty(); });
		if (oRequestData)
			*oRequestData = InterruptRequestedDequeueMT_();
	}
}

bool ServAuxData::InterruptRequestedDequeueTimeout(
	const std::chrono::milliseconds &WaitForMillis,
	ServAuxRequestData *oRequestData)
{
	// FIXME: outdated comment?
	/* @return: Interrupt (aka send message from serv_aux to serv counts as requested
	*    if a thread sets mInterruptRequested and notifies us, or timeout expires but
	*    mInterruptRequested still got set */

	bool IsPredicateTrue = false;
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		IsPredicateTrue = mAuxDataCond->wait_for(lock, WaitForMillis, [&]() { return !mAuxQueue->empty(); });
		if (IsPredicateTrue)
			if (oRequestData)
				*oRequestData = InterruptRequestedDequeueMT_();
	}
	return IsPredicateTrue;
}

ServAuxRequestData ServAuxData::InterruptRequestedDequeueMT_() {
	ServAuxRequestData RequestData = mAuxQueue->front();
	mAuxQueue->pop_front();
	return RequestData;
}

FullConnectionClient::FullConnectionClient(const sp<std::thread> &ThreadWorker, const sp<std::thread> &ThreadAux, const sp<std::thread> &Thread)
	: ThreadWorker(ThreadWorker),
	ThreadAux(ThreadAux),
	Thread(Thread)
{}

int gs_bypart_cb_OidVector(void *ctx, const char *d, int64_t l) {
	int r = 0;

	git_oid Oid = {};
	GS_BYPART_DATA_VAR_CTX_NONUCF(OidVector, Data, ctx);

	if (!!(r = aux_frame_read_oid((uint8_t *)d, (uint32_t)l, 0, NULL, (uint8_t *)Oid.id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	Data->m0OidVec->push_back(Oid);

clean:

	return r;
}

int gs_connection_surrogate_map_clear(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	ioConnectionSurrogateMap->mConnectionSurrogateMap->clear();
	return 0;
}

int gs_connection_surrogate_map_insert_id(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	const sp<GsConnectionSurrogate> &ConnectionSurrogate)
{
	int r = 0;

	if (! ioConnectionSurrogateMap->mConnectionSurrogateMap->insert(
			gs_connection_surrogate_map_t::value_type(ConnectionSurrogateId, ConnectionSurrogate)).second)
	{
		GS_ERR_CLEAN_L(1, E, S, "insertion prevented (is a stale element present, and why?)");
	}

clean:

	return r;
}

int gs_connection_surrogate_map_insert(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	const sp<GsConnectionSurrogate> &ConnectionSurrogate,
	gs_connection_surrogate_id_t *oConnectionSurrogateId)
{
	int r = 0;

	gs_connection_surrogate_id_t Id = ioConnectionSurrogateMap->mAtomicCount.fetch_add(1);

	if (!!(r = gs_connection_surrogate_map_insert_id(ioConnectionSurrogateMap, Id, ConnectionSurrogate)))
		GS_GOTO_CLEAN();

	if (oConnectionSurrogateId)
		*oConnectionSurrogateId = Id;

clean:

	return r;
}

int gs_connection_surrogate_map_get_try(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	sp<GsConnectionSurrogate> *oConnectionSurrogate)
{
	int r = 0;

	sp<GsConnectionSurrogate> ConnectionSurrogate;

	gs_connection_surrogate_map_t::iterator it =
		ioConnectionSurrogateMap->mConnectionSurrogateMap->find(ConnectionSurrogateId);

	if (it != ioConnectionSurrogateMap->mConnectionSurrogateMap->end())
		ConnectionSurrogate = it->second;

	if (oConnectionSurrogate)
		*oConnectionSurrogate = ConnectionSurrogate;

clean:

	return r;
}

int gs_connection_surrogate_map_get(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	sp<GsConnectionSurrogate> *oConnectionSurrogate)
{
	int r = 0;

	if (!!(r = gs_connection_surrogate_map_get_try(ioConnectionSurrogateMap, ConnectionSurrogateId, oConnectionSurrogate)))
		GS_GOTO_CLEAN();

	if (! oConnectionSurrogate->get())
		GS_ERR_CLEAN_L(1, E, S, "retrieval prevented (is an element missing, and why?)");

clean:

	return r;
}

int gs_connection_surrogate_map_erase(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId)
{
	int r = 0;

	gs_connection_surrogate_map_t::iterator it =
		ioConnectionSurrogateMap->mConnectionSurrogateMap->find(ConnectionSurrogateId);

	if (it == ioConnectionSurrogateMap->mConnectionSurrogateMap->end())
		GS_ERR_CLEAN_L(1, E, S, "removal prevented (is an element missing, and why?)");

	ioConnectionSurrogateMap->mConnectionSurrogateMap->erase(it);

clean:

	return r;
}

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

		enet_peer_disconnect_now(oldpeer, 0);
		enet_host_destroy(oldhost);
	}

	return 0;
}

GsConnectionSurrogateMap::GsConnectionSurrogateMap()
	: mAtomicCount(0),
	mConnectionSurrogateMap(new gs_connection_surrogate_map_t)
{}

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

int aux_make_serv_worker_request_data(gs_connection_surrogate_id_t Id, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oRequestWorker) {
	int r = 0;

	sp<ServWorkerRequestData> RequestWorker;

	if (! ioPacket->get())
		GS_ERR_CLEAN_L(1, E, S, "ServWorkerRequestData uses null packet as special value");

	RequestWorker = sp<ServWorkerRequestData>(new ServWorkerRequestData(
		ioPacket,
		false,
		true,
		Id));

	if (oRequestWorker)
		*oRequestWorker = RequestWorker;

clean:

	return r;
}

int aux_make_serv_worker_request_data_reconnect_prepare(
	sp<ServWorkerRequestData> *oRequestWorker)
{
	int r = 0;

	gs_packet_unique_t GsPacket;

	sp<ServWorkerRequestData> RequestWorker(new ServWorkerRequestData(
		&GsPacket,
		true,
		false,
		-1));

	if (oRequestWorker)
		*oRequestWorker = RequestWorker;

clean:

	return r;
}

int aux_make_serv_worker_request_data_reconnect_regular_no_id(
	sp<ServWorkerRequestData> *oRequestWorker)
{
	int r = 0;

	gs_packet_unique_t GsPacket;

	sp<ServWorkerRequestData> RequestWorker(new ServWorkerRequestData(
		&GsPacket,
		false,
		false,
		-1));

	if (oRequestWorker)
		*oRequestWorker = RequestWorker;

clean:

	return r;
}

int aux_make_serv_worker_request_data_reconnect_regular_with_id(
	gs_connection_surrogate_id_t Id,
	sp<ServWorkerRequestData> *oRequestWorker)
{
	int r = 0;

	gs_packet_unique_t GsPacket;

	sp<ServWorkerRequestData> RequestWorker(new ServWorkerRequestData(
		&GsPacket,
		false,
		true,
		Id));

	if (oRequestWorker)
		*oRequestWorker = RequestWorker;

clean:

	return r;
}

int aux_make_serv_worker_request_data_for_response(
	ServWorkerRequestData *RequestBeingResponded, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oRequestWorker)
{
	int r = 0;

	GS_ASSERT(RequestBeingResponded->mIsWithId);

	sp<ServWorkerRequestData> RequestWorker(new ServWorkerRequestData(
		ioPacket,
		false,
		true,
		RequestBeingResponded->mId));

	if (oRequestWorker)
		*oRequestWorker = RequestWorker;

clean:

	return r;
}

void aux_get_serv_worker_request_private(ServWorkerRequestData *Request, gs_connection_surrogate_id_t *oId) {
	if (oId)
		*oId = Request->mId;
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

	GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
	GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

	GS_BYPART_DATA_VAR(OidVector, BypartBloblistRequested);
	GS_BYPART_DATA_INIT(OidVector, BypartBloblistRequested, &BloblistRequested);

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartBloblistRequested, gs_bypart_cb_OidVector)))
		GS_GOTO_CLEAN();

	if (!!(r = serv_serialize_blobs(Repository, &BloblistRequested, &SizeBufferBlob, &ObjectBufferBlob)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_response_blobs(
		FrameTypeResponse, BloblistRequested.size(),
		(uint8_t *)SizeBufferBlob.data(), SizeBufferBlob.size(),
		(uint8_t *)ObjectBufferBlob.data(), ObjectBufferBlob.size(),
		gs_bysize_cb_String, &BysizeResponseBuffer)))
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

int aux_worker_enqueue_reconnect_prepare(
	ServWorkerData *WorkerDataRecv)
{
	int r = 0;

	sp<ServWorkerRequestData> Request;

	if (!!(r = aux_make_serv_worker_request_data_reconnect_prepare(&Request)))
		GS_GOTO_CLEAN();

	WorkerDataRecv->RequestEnqueue(Request);

clean:

	return r;
}

int aux_worker_enqueue_reconnect_regular_no_id(
	ServWorkerData *WorkerDataRecv)
{
	int r = 0;

	sp<ServWorkerRequestData> Request;

	if (!!(r = aux_make_serv_worker_request_data_reconnect_regular_no_id(&Request)))
		GS_GOTO_CLEAN();

	WorkerDataRecv->RequestEnqueue(Request);

clean:

	return r;
}

int aux_worker_enqueue_reconnect_regular_with_id(
	ServWorkerData *WorkerDataRecv,
	gs_connection_surrogate_id_t Id)
{
	int r = 0;

	sp<ServWorkerRequestData> Request;

	if (!!(r = aux_make_serv_worker_request_data_reconnect_regular_with_id(Id, &Request)))
		GS_GOTO_CLEAN();

	WorkerDataRecv->RequestEnqueue(Request);

clean:

	return r;
}

int aux_worker_enqueue_reconnect_double_notify_no_id(
	ServWorkerData *WorkerDataRecv)
{
	int r = 0;

	if (!!(r = aux_worker_enqueue_reconnect_prepare(WorkerDataRecv)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_worker_enqueue_reconnect_regular_no_id(WorkerDataRecv)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int aux_worker_enqueue_reconnect_double_notify_with_id(
	ServWorkerData *WorkerDataRecv,
	gs_connection_surrogate_id_t Id)
{
	int r = 0;

	if (!!(r = aux_worker_enqueue_reconnect_prepare(WorkerDataRecv)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_worker_enqueue_reconnect_regular_with_id(WorkerDataRecv, Id)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

/* we are expecting: possibly a reconnect_prepare, mandatorily a reconnect */
int aux_worker_dequeue_handling_double_notify(
	ServWorkerData *WorkerDataRecv,
	sp<ServWorkerRequestData> *oRequest)
{
	int r = 0;

	sp<ServWorkerRequestData> Request;

	/* possibly reconnect_prepare */
	WorkerDataRecv->RequestDequeue(&Request);

	/* if indeed reconnect_prepare, skip it */
	if (Request->isReconnectRequestPrepare())
		WorkerDataRecv->RequestDequeue(&Request);

	if (!Request->isReconnectRequestRegular())
		GS_ERR_CLEAN_L(1, E, S, "suspected invalid double notify sequence");

	if (oRequest)
		*oRequest = Request;

clean:

	return r;
}

int aux_serv_worker_reconnect_expend_reconnect_discard_request_for_send(
	ServWorkerData *WorkerDataRecv,
	ClntStateReconnect *ioStateReconnect,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	sp<ServWorkerRequestData> RequestReconnect;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

	if (*ioWantReconnect) {

		if (!!(r = aux_worker_dequeue_handling_double_notify(WorkerDataRecv, &RequestReconnect)))
			GS_GOTO_CLEAN();

		GS_ASSERT(RequestReconnect->isReconnectRequestRegularNoId());

	}

	if (ioWantReconnect)
		*ioWantReconnect = false;

clean:

	return r;
}

int serv_worker_thread_func_reconnecter(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const char *RepoSelfUpdateOpenPathBuf, size_t LenRepoSelfUpdateOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend)
{
	int r = 0;

	ClntStateReconnect StateReconnect = {};

	uint32_t WantReconnect = true;

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

	/* NOTE: special error handling */
	while (true) {

		/* NOTE: no_clean */
		if (!!(r = aux_serv_worker_reconnect_expend_reconnect_discard_request_for_send(
			WorkerDataRecv.get(),
			&StateReconnect,
			&WantReconnect)))
		{
			GS_ERR_NO_CLEAN(1);
		}

		/* NOTE: cleansub */
		if (!!(r = serv_worker_thread_func(
			RefNameMainBuf, LenRefNameMain,
			RefNameSelfUpdateBuf, LenRefNameSelfUpdate,
			RepoMainOpenPathBuf, LenRepoMainOpenPath,
			RepoSelfUpdateOpenPathBuf, LenRepoSelfUpdateOpenPath,
			ServAuxData,
			WorkerDataRecv,
			WorkerDataSend,
			&WantReconnect)))
		{
			GS_GOTO_CLEANSUB();
		}

	cleansub:
		if (!!r) {
			GS_LOG(E, S, "serv_worker error into reconnect attempt");
		}
	}

noclean:

clean :

	return r;
}

int serv_state_crank(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const char *RepoSelfUpdateOpenPathBuf, size_t LenRepoSelfUpdateOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend)
{
	int r = 0;

	git_repository *Repository = NULL;
	git_repository *RepositorySelfUpdate = NULL;

	if (!!(r = aux_repository_open(RepoMainOpenPathBuf, &Repository)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoSelfUpdateOpenPathBuf, &RepositorySelfUpdate)))
		GS_GOTO_CLEAN();

	while (true) {
		sp<ServWorkerRequestData> Request;

		if (!!(r = aux_packet_request_dequeue(WorkerDataRecv.get(), &Request)))
			GS_GOTO_CLEAN();

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

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(Repository, RefNameMainBuf, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_latest_commit_tree(TreeHeadOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
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
			GsStrided TreelistStrided = {};

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = gs_strided_for_oid_vec_cpp(&Treelist, &TreelistStrided)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, TreeOid.id, GIT_OID_RAWSZ)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_oid_treelist(Repository, &TreeOid, &Treelist)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_treelist(TreelistStrided, gs_bysize_cb_String, &BysizeResponseBuffer)))
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

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			GS_BYPART_DATA_VAR(OidVector, BypartTreelistRequested);
			GS_BYPART_DATA_INIT(OidVector, BypartTreelistRequested, &TreelistRequested);

			if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartTreelistRequested, gs_bypart_cb_OidVector)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_serialize_trees(Repository, &TreelistRequested, &SizeBufferTree, &ObjectBufferTree)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_trees(
				TreelistRequested.size(),
				(uint8_t *)SizeBufferTree.data(), SizeBufferTree.size(),
				(uint8_t *)ObjectBufferTree.data(), ObjectBufferTree.size(),
				gs_bysize_cb_String, &BysizeResponseBuffer)))
			{
				GS_GOTO_CLEAN();
			}

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

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(RepositorySelfUpdate, RefNameSelfUpdateBuf, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_oid_tree_blob_byname(RepositorySelfUpdate, &TreeHeadOid, GS_STR_PARENT_EXPECTED_SUFFIX, &BlobSelfUpdateOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_latest_selfupdate_blob(BlobSelfUpdateOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
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

int serv_worker_thread_func(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const char *RepoSelfUpdateOpenPathBuf, size_t LenRepoSelfUpdateOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	uint32_t *oWantReconnect)
{
	int r = 0;

	uint32_t WantReconnect = false;

	if (!!(r = serv_state_crank(
		RefNameMainBuf, LenRefNameMain,
		RefNameSelfUpdateBuf, LenRefNameSelfUpdate,
		RepoMainOpenPathBuf, LenRepoMainOpenPath,
		RepoSelfUpdateOpenPathBuf, LenRepoSelfUpdateOpenPath,
		ServAuxData,
		WorkerDataRecv,
		WorkerDataSend)))
	{
		GS_ERR_NO_CLEAN(r);
	}

noclean:
	if (!!r && r == GS_ERRCODE_RECONNECT) {
		WantReconnect = true;
	}

	if (oWantReconnect)
		*oWantReconnect = WantReconnect;

clean:

	return r;
}

int aux_clnt_worker_reconnect_expend_reconnect_receive_request_for_send(
	ServWorkerData *WorkerDataRecv,
	ClntStateReconnect *ioStateReconnect,
	sp<ServWorkerRequestData> *oRequestForSend,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	sp<ServWorkerRequestData> RequestReconnect;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

	if (*ioWantReconnect) {

		if (!!(r = aux_worker_dequeue_handling_double_notify(WorkerDataRecv, &RequestReconnect)))
			GS_GOTO_CLEAN();

		GS_ASSERT(RequestReconnect->isReconnectRequestRegularWithId());

	}

	// FIXME: rebuild ServWorkerRequestData instead of using RequestReconnect directly
	if (oRequestForSend)
		*oRequestForSend = RequestReconnect;

	if (ioWantReconnect)
		*ioWantReconnect = false;

clean:

	return r;
}

int clnt_worker_thread_func_reconnecter(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend)
{
	int r = 0;

	ClntStateReconnect StateReconnect = {};

	sp<ClntState> State(new ClntState);

	sp<ServWorkerRequestData> RequestForSend;

	uint32_t WantReconnect = true;

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_state_make_default(State.get())))
		GS_GOTO_CLEAN();

	/* NOTE: special error handling */
	while (true) {

		/* NOTE: no_clean */
		if (!!(r = aux_clnt_worker_reconnect_expend_reconnect_receive_request_for_send(
			WorkerDataRecv.get(),
			&StateReconnect,
			&RequestForSend,
			&WantReconnect)))
		{
			GS_ERR_NO_CLEAN(1);
		}

		/* NOTE: cleansub */
		if (!!(r = clnt_worker_thread_func(
			RefNameMainBuf, LenRefNameMain,
			RepoMainOpenPathBuf, LenRepoMainOpenPath,
			ServAuxData,
			WorkerDataRecv,
			WorkerDataSend,
			RequestForSend,
			State,
			&WantReconnect)))
		{
			GS_GOTO_CLEANSUB();
		}

	cleansub:
		if (!!r) {
			GS_LOG(E, S, "clnt_worker error into reconnect attempt");
		}
	}

noclean:

clean:

	return r;
}

int clnt_worker_thread_func(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServWorkerRequestData> RequestForSend,
	sp<ClntState> State,
	uint32_t *oWantReconnect)
{
	int r = 0;

	uint32_t WantReconnect = false;

	while (true) {
		if (!!(r = clnt_state_crank(
			State,
			RefNameMainBuf, LenRefNameMain,
			RepoMainOpenPathBuf, LenRepoMainOpenPath,
			ServAuxData,
			WorkerDataRecv.get(),
			WorkerDataSend.get(),
			RequestForSend.get())))
		{
			GS_ERR_NO_CLEAN(r);
		}
	}

noclean:
	if (!!r && r == GS_ERRCODE_RECONNECT) {
		WantReconnect = true;
	}

	if (oWantReconnect)
		*oWantReconnect = WantReconnect;

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

int aux_enet_host_server_create_addr_extra_for_serv_aux(
	uint32_t ServPort,
	ENetHost **oHost,
	ENetAddress *oAddressForServAux)
{
	int r = 0;

	ENetAddress AddressForServ = {};
	ENetAddress AddressForServAux = {};
	ENetHost *host = NULL;

	uint32_t LoopbackIp = ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24);

	AddressForServ.host = ENET_HOST_ANY;
	AddressForServ.port = ServPort;
	/* ENET_HOST_ANY (0) will listen on all interfaces but also cause host->address to have 0 as host.
	*    an address suitable for connection from serv_aux is needed.
	*    a loopback address is constructed and returned for serv_aux connection purposes. */
	AddressForServAux.host = LoopbackIp;
	AddressForServAux.port = ServPort;

	if (!(host = enet_host_create(&AddressForServ, 256, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (oHost)
		*oHost = host;

	if (oAddressForServAux)
		*oAddressForServAux = AddressForServAux;

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

	if (!(peer = enet_host_connect(host, address, 1, 0)))
		GS_GOTO_CLEAN();

	if (oPeer)
		*oPeer = peer;

clean:
	if (!!r) {
		if (peer)
			enet_peer_disconnect_now(peer, 0);
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
			enet_peer_disconnect_now(peer, 0);

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
	GS_ASSERT((EnetPacketFlags & ~(ENET_PACKET_FLAG_RELIABLE)) == 0);

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

/* @err: GS_ERRCODE_RECONNECT if reconnect request dequeued */
int aux_packet_request_dequeue(ServWorkerData *WorkerDataRecv, sp<ServWorkerRequestData> *oRequestForRecv) {
	int r = 0;

	sp<ServWorkerRequestData> RequestForRecv;

	WorkerDataRecv->RequestDequeue(&RequestForRecv);

	if (RequestForRecv->isReconnectRequest())
		GS_ERR_CLEAN(GS_ERRCODE_RECONNECT);

	if (oRequestForRecv)
		*oRequestForRecv = RequestForRecv;

clean:

	return r;
}

/* @err: GS_ERRCODE_RECONNECT if reconnect request dequeued */
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

	GS_ASSERT(retcode >= 0);

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
				enet_peer_disconnect_now(peer, 0);
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
			enet_peer_disconnect_now(nontimedout_peer, 0);
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

	std::string BufferLatest;
	std::string BufferBlobs;
	gs_packet_t GsPacketBlobOid;
	gs_packet_t GsPacketBlob;
	ENetPacket *PacketBlob = NULL;
	ENetPacket *PacketBlobOid = NULL;
	uint32_t Offset = 0;
	uint32_t DataLengthLimit = 0;

	git_oid BlobSelfUpdateOidT = {};

	std::vector<git_oid> BlobSelfUpdateOidVec(1);
	git_oid * const &BlobSelfUpdateOid = &BlobSelfUpdateOidVec.at(0);
	GsStrided BlobSelfUpdateOidVecStrided = {};

	uint32_t BlobPairedVecLen = 0;
	uint32_t BlobOffsetSizeBuffer = 0;
	uint32_t BlobOffsetObjectBuffer = 0;

	GS_BYPART_DATA_VAR(String, BysizeBufferLatest);
	GS_BYPART_DATA_INIT(String, BysizeBufferLatest, &BufferLatest);

	GS_BYPART_DATA_VAR(String, BysizeBufferBlobs);
	GS_BYPART_DATA_INIT(String, BysizeBufferBlobs, &BufferBlobs);

	if (!!(r = gs_strided_for_oid_vec_cpp(&BlobSelfUpdateOidVec, &BlobSelfUpdateOidVecStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_memory_repository_new(&RepositoryMemory)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_address_create_hostname(GS_PORT, HostName, &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &host, &peer)))
		GS_GOTO_CLEAN_L(E, PF, "failure connecting [host=[%s]]", HostName);

	if (!!(r = aux_frame_full_write_request_latest_selfupdate_blob(gs_bysize_cb_String, &BysizeBufferLatest)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_bare_send(host, peer, BufferLatest.data(), BufferLatest.size(), ENET_PACKET_FLAG_RELIABLE)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_service_one_type_receive(host, GS_RECEIVE_TIMEOUT_MS, &GsPacketBlobOid)))
		GS_GOTO_CLEAN();

	PacketBlobOid = *GsPacketBlobOid;

	Offset = 0;

	if (!!(r = aux_frame_ensure_frametype(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_SELFUPDATE_BLOB))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, BlobSelfUpdateOid->id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	/* empty as_path parameter means no filters applied */
	if (!!(r = git_repository_hashfile(&BlobSelfUpdateOidT, RepositoryMemory, FileNameAbsoluteSelfUpdate, GIT_OBJ_BLOB, "")))
		GS_GOTO_CLEAN_L(E, PF, "failure hashing [filename=[%s]]", FileNameAbsoluteSelfUpdate);

	if (git_oid_cmp(&BlobSelfUpdateOidT, BlobSelfUpdateOid) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &BlobSelfUpdateOidT);
		GS_LOG(I, PF, "have latest [oid=[%.*s]]", GIT_OID_HEXSZ, buf);
	}

	if (!!(r = aux_frame_full_write_request_blobs_selfupdate(BlobSelfUpdateOidVecStrided, gs_bysize_cb_String, &BysizeBufferBlobs)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_bare_send(host, peer, BufferBlobs.data(), BufferBlobs.size(), ENET_PACKET_FLAG_RELIABLE)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_service_one_type_receive(host, GS_RECEIVE_TIMEOUT_MS, &GsPacketBlob)))
		GS_GOTO_CLEAN();

	PacketBlob = *GsPacketBlob;

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
		enet_peer_disconnect_now(peer, 0);

	if (host)
		enet_host_destroy(host);

	if (RepositoryMemory)
		git_repository_free(RepositoryMemory);

	return r;
}

/* FIXME: unused code afaik */
int aux_serv_aux_wait_reconnect(ServAuxData *AuxData, ENetAddress *oAddress) {
	int r = 0;

	bool HaveRequestData = false;
	ServAuxRequestData RequestData;

	while (
		!(
			(HaveRequestData = AuxData->InterruptRequestedDequeueTimeout(
				std::chrono::milliseconds(GS_SERV_AUX_ARBITRARY_TIMEOUT_MS),
				&RequestData))
			&&
			(! RequestData.IsReconnectRequest())
		))
	{ /* dummy */ }

	GS_ASSERT(HaveRequestData && RequestData.IsReconnectRequest());

	if (oAddress)
		*oAddress = RequestData.mAddress;

clean:

	return r;
}

int aux_serv_aux_wait_reconnect_and_connect(ServAuxData *AuxData, sp<GsConnectionSurrogate> *oConnectionSurrogate) {
	int r = 0;

	sp<GsConnectionSurrogate> ConnectionSurrogate;

	ServAuxRequestData RequestReconnect;

	ENetAddress address = {};

	ENetHost *host = NULL;
	ENetPeer *peer = NULL;

	if (!!(r = aux_serv_aux_dequeue_handling_double_notify(AuxData, &RequestReconnect)))
		GS_GOTO_CLEAN();

	GS_ASSERT(RequestReconnect.isReconnectRequestRegular());

	address = RequestReconnect.mAddress;

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &host, &peer)))
		GS_GOTO_CLEAN();

	ConnectionSurrogate = sp<GsConnectionSurrogate>(new GsConnectionSurrogate(host, peer, false));

	if (oConnectionSurrogate)
		*oConnectionSurrogate = ConnectionSurrogate;

clean:
	if (!!r) {
		if (peer)
			enet_peer_disconnect_now(peer, 0);

		if (host)
			enet_host_destroy(host);
	}

	return r;
}

int aux_serv_aux_reconnect_expend_cond_interrupt_perform(
	ServAuxData *AuxData,
	ClntStateReconnect *ioStateReconnect,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

	if (*ioWantReconnect) {

		if (!!(r = aux_serv_aux_wait_reconnect_and_connect(AuxData, ioConnectionSurrogate)))
			GS_GOTO_CLEAN();

		/* NOTE: serv_aux/serv may have swallowed some interrupt requests due to reconnection:
		* sequence:
		*   - serv reconnects
		*   - serv can not receive serv_aux packets
		*   - simultaneously:
		*   -   serv notifies serv_aux about reconnection (ex enqueued through ServAuxData)
		*   -   serv_aux attempts to send some interrupt requested frames
		*   - serv_aux fails to send those frames due to losing serv connection due to the serv reconnect
		*   - serv_aux reconnects
		*   - packets meant for serv which caused the serv_aux interrupt requests might never get dequeued by serv
		*       (presumably serv stuck in the enet host service wait)
		* as a remedy, always send an extra interrupt requested frame upon serv_aux reconnect.
		* this is the extra frame send-site. */
		if (!!(r = aux_serv_aux_interrupt_perform(ioConnectionSurrogate->get())))
			GS_GOTO_CLEAN();

	}

	if (ioWantReconnect)
		*ioWantReconnect = false;

clean:

	return r;
}

int aux_serv_aux_thread_func_reconnecter(sp<ServAuxData> ServAuxData) {
	int r = 0;

	sp<GsConnectionSurrogate> ConnectionSurrogate;

	ClntStateReconnect StateReconnect = {};

	uint32_t WantReconnect = true;

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

	/* NOTE: special error handling */
	while (true) {

		/* NOTE: no_clean */
		if (!!(r = aux_serv_aux_reconnect_expend_cond_interrupt_perform(
			ServAuxData.get(),
			&StateReconnect,
			&ConnectionSurrogate,
			&WantReconnect)))
		{
			GS_ERR_NO_CLEAN(r);
		}

		/* NOTE: cleansub */
		if (!!(r = serv_aux_thread_func(
			ServAuxData,
			&ConnectionSurrogate,
			&WantReconnect)))
		{
			GS_GOTO_CLEANSUB();
		}

	cleansub:
		if (!!r) {
			GS_LOG(E, S, "serv_aux error into reconnect attempt");
		}
	}

noclean:

clean:

	return r;
}

int aux_serv_aux_make_premade_frame_interrupt_requested(std::string *oBufferFrameInterruptRequested) {
	int r = 0;

	std::string BufferFrameInterruptRequested;

	GS_BYPART_DATA_VAR(String, BysizeBufferFrameInterruptRequested);
	GS_BYPART_DATA_INIT(String, BysizeBufferFrameInterruptRequested, &BufferFrameInterruptRequested);

	if (!!(r = aux_frame_full_write_serv_aux_interrupt_requested(gs_bysize_cb_String, &BysizeBufferFrameInterruptRequested)))
		GS_GOTO_CLEAN();

	if (oBufferFrameInterruptRequested)
		oBufferFrameInterruptRequested->swap(BufferFrameInterruptRequested);

clean:

	return r;
}

int aux_serv_aux_enqueue_reconnect_double_notify(ServAuxData *AuxData, ENetAddress *address) {
	AuxData->InterruptRequestedEnqueueData(ServAuxRequestData(true, true, NULL));
	AuxData->InterruptRequestedEnqueueData(ServAuxRequestData(true, false, address));
	return 0;
}

int aux_serv_aux_dequeue_handling_double_notify(
	ServAuxData *AuxData,
	ServAuxRequestData *oRequest)
{
	int r = 0;

	ServAuxRequestData Request;

	AuxData->InterruptRequestedDequeue(&Request);

	if (Request.isReconnectRequestPrepare())
		AuxData->InterruptRequestedDequeue(&Request);

	if (!Request.isReconnectRequestRegular())
		GS_ERR_CLEAN_L(1, E, S, "suspected invalid double notify sequence");

	if (oRequest)
		*oRequest = Request;

clean:

	return r;
}

int aux_serv_aux_interrupt_perform(
	GsConnectionSurrogate *ConnectionSurrogate)
{
	int r = 0;

	ENetHost * const &host = ConnectionSurrogate->mHost;
	ENetPeer * const &peer = ConnectionSurrogate->mPeer;

	std::string BufferFrameInterruptRequested;

	// FIXME: performance note: remaking the frame every time - this frame is reusable, right?
	if (!!(r = aux_serv_aux_make_premade_frame_interrupt_requested(&BufferFrameInterruptRequested)))
		GS_GOTO_CLEAN();

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

	enet_host_flush(host);

clean:

	return r;
}

int aux_serv_aux_host_service_sub(ENetHost *client) {
	int r = 0;

	std::vector<ENetEvent> Events;

	if (!!(r = aux_host_service(client, 0, &Events)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < Events.size(); i++) {
		switch (Events[i].type)
		{
		case ENET_EVENT_TYPE_CONNECT:
			GS_LOG(E, S, "unexpected connection to serv_aux - just passthrough");
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
			GS_ERR_CLEAN_L(1, E, S, "unexpected (?) disconnection from serv_aux");
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			GS_ASSERT(0);
			enet_packet_destroy(Events[i].packet);
			break;
		}
	}

clean:

	return r;
}

int aux_serv_aux_host_service(
	sp<ServAuxData> ServAuxData,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate)
{
	/* NOTE: errors in this function should also reset the ConnectionSurrogate as part of the API */

	int r = 0;

	ENetHost * const &client = (*ioConnectionSurrogate)->mHost;
	ENetPeer * const &peer = (*ioConnectionSurrogate)->mPeer;

	while (true) {

		ServAuxRequestData RequestData;

		/* NOTE: waiting for ServAuxRequestData with mIsReconnectRequest is enough.
		*    receiving an ENET_EVENT_TYPE_DISCONNECT indicates we should reconnect but
		*    the reconnection address is not known at that point. */
		if (!!(r = aux_serv_aux_host_service_sub(client)))
			GS_GOTO_CLEAN();

		/* set a timeout to ensure serv_aux_host_service cranks the enet event loop regularly */

		bool HaveRequestData = ServAuxData->InterruptRequestedDequeueTimeout(
			std::chrono::milliseconds(GS_SERV_AUX_ARBITRARY_TIMEOUT_MS),
			&RequestData);

		if (!HaveRequestData)
			continue;

		// FIXME: GS_ERRCODE_RECONNECT here, right?
		if (RequestData.IsReconnectRequest())
			GS_ERR_CLEAN(1);

		GS_ASSERT(! RequestData.IsReconnectRequest());

		if (!!(r = aux_serv_aux_interrupt_perform(ioConnectionSurrogate->get())))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int serv_aux_thread_func(
	sp<ServAuxData> ServAuxData,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	uint32_t *oWantReconnect)
{
	int r = 0;

	uint32_t WantReconnect = false;

	if (!!(r = aux_serv_aux_thread_func(ServAuxData, ioConnectionSurrogate)))
		GS_ERR_NO_CLEAN(1);

noclean:
	if (!!r) {
		WantReconnect = true;
	}

	if (oWantReconnect)
		*oWantReconnect = WantReconnect;

clean:

	return r;
}

int aux_serv_aux_thread_func(
	sp<ServAuxData> ServAuxData,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate)
{
	int r = 0;

	while (true) {
		if (!!(r = aux_serv_aux_host_service(ServAuxData, ioConnectionSurrogate)))
			GS_ERR_CLEAN(1);
	}

clean:

	return r;
}

int aux_serv_host_service(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	const sp<ServWorkerData> &WorkerDataRecv,
	const sp<ServWorkerData> &WorkerDataSend,
	sp<GsHostSurrogate> *ioHostSurrogate)
{
	int r = 0;

	ENetHost * const &server = (*ioHostSurrogate)->mHost;

	std::vector<ENetEvent> Events;

	// FIXME: serv_aux arbitrary timeout?
	if (!!(r = aux_host_service(server, GS_SERV_AUX_ARBITRARY_TIMEOUT_MS, &Events)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < Events.size(); i++) {
		switch (Events[i].type)
		{
		case ENET_EVENT_TYPE_CONNECT:
		{
			ENetPeer *peer = Events[i].peer;

			GS_BYPART_DATA_VAR(GsConnectionSurrogateId, ctxstruct);

			gs_connection_surrogate_id_t AssignedId = 0;
			sp<GsConnectionSurrogate> ConnectionSurrogate(new GsConnectionSurrogate(server, peer, false));

			// FIXME: also need to store server and peer aka host and peer
			if (!!(r = gs_connection_surrogate_map_insert(ioConnectionSurrogateMap, ConnectionSurrogate, &AssignedId)))
				GS_GOTO_CLEAN();

			GS_BYPART_DATA_INIT(GsConnectionSurrogateId, ctxstruct, AssignedId);

			// FIXME: sigh raw allocation, delete at ENET_EVENT_TYPE_DISCONNECT
			peer->data = new GsBypartCbDataGsConnectionSurrogateId(ctxstruct);

			printf("[serv] %d connected [from %x:%u]\n", (int)AssignedId, peer->address.host, peer->address.port);
		}
		break;

		case ENET_EVENT_TYPE_RECEIVE:
		{
			ENetPeer *peer = Events[i].peer;

			GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, peer->data);

			sp<GsConnectionSurrogate> ConnectionSurrogateRecv;
			
			const GsFrameType &FrameTypeInterruptRequested = GS_FRAME_TYPE_DECL(SERV_AUX_INTERRUPT_REQUESTED);
			GsFrameType FoundFrameType = {};

			if (!!(r = gs_connection_surrogate_map_get(ioConnectionSurrogateMap, ctxstruct->m0Id, &ConnectionSurrogateRecv)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_frametype(Events[i].packet->data, Events[i].packet->dataLength, 0, NULL, &FoundFrameType)))
				GS_GOTO_CLEAN();

			/* filter out interrupt requested frames and only dispatch other */

			if (! aux_frametype_equals(FoundFrameType, FrameTypeInterruptRequested)) {

				printf("[serv] packet received\n");

				gs_packet_unique_t Packet = aux_gs_make_packet_unique(Events[i].packet);

				sp<ServWorkerRequestData> ServWorkerRequestData;

				if (!!(r = aux_make_serv_worker_request_data(ctxstruct->m0Id, &Packet, &ServWorkerRequestData)))
					GS_GOTO_CLEAN();

				WorkerDataRecv->RequestEnqueue(ServWorkerRequestData);
			}

			/* check out if any send requests need servicing */

			{
				std::deque<sp<ServWorkerRequestData> > RequestedSends;

				WorkerDataSend->RequestDequeueAllOpt(&RequestedSends);

				for (uint32_t i = 0; i < RequestedSends.size(); i++) {
					gs_connection_surrogate_id_t IdOfSend;

					sp<GsConnectionSurrogate> ConnectionSurrogateSend;

					aux_get_serv_worker_request_private(RequestedSends[i].get(), &IdOfSend);

					if (!!(r = gs_connection_surrogate_map_get_try(ioConnectionSurrogateMap, IdOfSend, &ConnectionSurrogateSend)))
						GS_GOTO_CLEAN();

					/* if a reconnection occurred, outstanding send requests would have missing send IDs */
					if (! ConnectionSurrogateSend) {
						GS_LOG(W, PF, "suppressing packet for GsConnectionSurrogate [%llu]", (unsigned long long) IdOfSend);
						continue;
					}

					/* ownership of packet is lost after enet_peer_send */
					ENetPacket *Packet = *RequestedSends[i]->mPacket.release();

					/* did not remove a surrogate ID soon enough? */
					GS_ASSERT(ConnectionSurrogateSend->mHost == server);

					if (enet_peer_send(ConnectionSurrogateSend->mPeer, 0, Packet) < 0)
						GS_GOTO_CLEAN();
				}

				/* absolutely no reason to flush if nothing was sent */
				/* notice we are flushing 'server', above find an assert against RequestedSends surrogate ID host */

				if (RequestedSends.size())
					enet_host_flush(server);
			}
		}
		break;

		case ENET_EVENT_TYPE_DISCONNECT:
		{
			ENetPeer *peer = Events[i].peer;

			GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, peer->data);

			if (!!(r = gs_connection_surrogate_map_erase(ioConnectionSurrogateMap, ctxstruct->m0Id)))
				GS_GOTO_CLEAN();

			/* recheck tripwire just to be sure */
			GS_BYPART_DATA_VAR_AUX_TRIPWIRE_CHECK_NONUCF(GsConnectionSurrogateId, ctxstruct);
			// FIXME: sigh raw deletion, should have been allocated at ENET_EVENT_TYPE_CONNECT
			delete peer->data;
			peer->data = NULL;

			printf("[serv] %d disconnected.\n", (int)ctxstruct->m0Id);
		}
		break;

		}
	}

clean:

	return r;
}

int aux_serv_thread_func(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<GsHostSurrogate> *ioHostSurrogate,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	while (true) {
		if (!!(r = aux_serv_host_service(ioConnectionSurrogateMap, WorkerDataRecv, WorkerDataSend, ioHostSurrogate)))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_serv_serv_connect_immediately(
	uint32_t ServPort,
	sp<GsHostSurrogate> *ioHostSurrogate,
	ENetAddress *oAddressForServAux)
{
	int r = 0;

	ENetAddress AddressForServAux = {};
	ENetHost *server = NULL;
	sp<GsHostSurrogate> HostSurrogate;

	if (!!(r = aux_enet_host_server_create_addr_extra_for_serv_aux(ServPort, &server, &AddressForServAux)))
		GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		*ioHostSurrogate = sp<GsHostSurrogate>(new GsHostSurrogate(server));

	if (oAddressForServAux)
		*oAddressForServAux = AddressForServAux;

clean:
	if (!!r) {
		if (server)
			enet_host_destroy(server);
	}

	return r;
}

int aux_serv_serv_reconnect_expend_reconnect_cond_notify_serv_aux_notify_worker(
	ServAuxData *AuxData,
	ServWorkerData *WorkerDataRecv,
	uint32_t ServPort,
	ClntStateReconnect *ioStateReconnect,
	sp<GsHostSurrogate> *ioHostSurrogate,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

	if (*ioWantReconnect) {

		ENetAddress AuxDataNotificationAddress = {};

		if (!!(r = aux_serv_serv_connect_immediately(
			ServPort,
			ioHostSurrogate,
			&AuxDataNotificationAddress)))
		{
			GS_GOTO_CLEAN();
		}

		if (!!(r = aux_serv_aux_enqueue_reconnect_double_notify(AuxData, &AuxDataNotificationAddress)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_worker_enqueue_reconnect_double_notify_no_id(WorkerDataRecv)))
			GS_GOTO_CLEAN();
	}

	/* connection is ensured if no errors occurred (either existing or newly established)
	*  in either case we no longer need to reconnect */
	if (ioWantReconnect)
		*ioWantReconnect = false;

clean :

	return r;
}

int serv_serv_thread_func_reconnecter(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	uint32_t ServPort)
{
	int r = 0;

	ClntStateReconnect StateReconnect = {};

	sp<GsConnectionSurrogateMap> ConnectionSurrogateMap(new GsConnectionSurrogateMap());

	sp<GsHostSurrogate> HostSurrogate;

	uint32_t WantReconnect = true;

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

	/* NOTE: special error handling */
	while (true) {

		/* protocol between serv and serv_aux starts with notifying serv_aux about serv address
		*  after every reconnection (including initial connect) */
		/* NOTE: no_clean */
		if (!!(r = aux_serv_serv_reconnect_expend_reconnect_cond_notify_serv_aux_notify_worker(
			AuxData.get(),
			WorkerDataRecv.get(),
			ServPort,
			&StateReconnect,
			&HostSurrogate,
			&WantReconnect)))
		{
			GS_ERR_NO_CLEAN(r);
		}

		/* NOTE: cleansub */
		if (!!(r = serv_serv_thread_func(
			WorkerDataRecv,
			WorkerDataSend,
			ConnectionSurrogateMap.get(),
			&HostSurrogate,
			&WantReconnect)))
		{
			GS_GOTO_CLEANSUB();
		}

	cleansub:
		if (!!r) {
			GS_LOG(E, S, "serv_serv error into reconnect attempt");
		}
	}

noclean:

clean:

	return r;
}

int serv_serv_thread_func(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	sp<GsHostSurrogate> *ioHostSurrogate,
	uint32_t *oWantReconnect)
{
	int r = 0;

	uint32_t WantReconnect = false;

	if (!!(r = aux_serv_thread_func(WorkerDataRecv, WorkerDataSend, ioHostSurrogate, ioConnectionSurrogateMap)))
		GS_ERR_NO_CLEAN(1);

noclean:
	if (!!r) {
		WantReconnect = true;
	}

	if (oWantReconnect)
		*oWantReconnect = WantReconnect;

clean:

	return r;
}

int aux_clnt_serv_connect_immediately(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	ENetAddress *oAddressClnt)
{
	int r = 0;

	ENetHost *clnt = NULL;
	ENetAddress AddressClnt = {};
	ENetAddress AddressServ = {};
	ENetPeer *peer = NULL;

	if (!!(r = aux_enet_host_client_create_addr(&clnt, &AddressClnt)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_address_create_hostname(ServPort, ServHostNameBuf, &AddressServ)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_host_connect_addr(clnt, &AddressServ, &peer)))
		GS_GOTO_CLEAN();

	if (ioConnectionSurrogate)
		*ioConnectionSurrogate = sp<GsConnectionSurrogate>(new GsConnectionSurrogate(clnt, peer, true));

	if (oAddressClnt)
		*oAddressClnt = AddressClnt;

clean:
	if (!!r) {
		if (peer)
			enet_peer_disconnect_now(peer, 0);

		if (clnt)
			enet_host_destroy(clnt);
	}

	return r;
}

int aux_clnt_serv_reconnect_expend_reconnect_cond_insert_map_notify_serv_aux_notify_worker(
	ServAuxData *AuxData,
	ServWorkerData *WorkerDataRecv,
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	ClntStateReconnect *ioStateReconnect,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

	if (ioWantReconnect) {

		ENetAddress AuxDataNotificationAddress = {};
		gs_connection_surrogate_id_t Id = 0;

		if (!!(r = aux_clnt_serv_connect_immediately(
			ServPort,
			ServHostNameBuf, LenServHostName,
			ioConnectionSurrogate,
			&AuxDataNotificationAddress)))
		{
			GS_GOTO_CLEAN();
		}

		// FIXME: create an atomic clear + insert to ensure principalclientconnection always available (no races)
		// FIXME: clear connection map after reconnect? probably
		if (!!(r = gs_connection_surrogate_map_clear(ioConnectionSurrogateMap)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_connection_surrogate_map_insert(ioConnectionSurrogateMap, *ioConnectionSurrogate, &Id)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_serv_aux_enqueue_reconnect_double_notify(AuxData, &AuxDataNotificationAddress)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_worker_enqueue_reconnect_double_notify_with_id(WorkerDataRecv, Id)))
			GS_GOTO_CLEAN();
	}

	if (ioWantReconnect)
		*ioWantReconnect = false;

clean :

	return r;
}

int clnt_serv_thread_func_reconnecter(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName)
{
	int r = 0;

	ClntStateReconnect StateReconnect = {};

	sp<GsConnectionSurrogateMap> ConnectionSurrogateMap(new GsConnectionSurrogateMap());

	sp<GsConnectionSurrogate> ConnectionSurrogate;

	uint32_t WantReconnect = true;

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

	/* NOTE: special error handling */
	while (true) {

		/* NOTE: no_clean */
		if (!!(r = aux_clnt_serv_reconnect_expend_reconnect_cond_insert_map_notify_serv_aux_notify_worker(
			AuxData.get(),
			WorkerDataRecv.get(),
			ServPort,
			ServHostNameBuf, LenServHostName,
			&StateReconnect,
			ConnectionSurrogateMap.get(),
			&ConnectionSurrogate,
			&WantReconnect)))
		{
			GS_ERR_NO_CLEAN(r);
		}

		/* NOTE: cleansub */
		if (!!(r = clnt_serv_thread_func(
			WorkerDataRecv,
			WorkerDataSend,
			AuxData,
			ConnectionSurrogateMap.get(),
			&ConnectionSurrogate,
			&WantReconnect)))
		{
			GS_GOTO_CLEANSUB();
		}

	cleansub:
		if (!!r) {
			GS_LOG(E, S, "clnt_serv error into reconnect attempt");
		}
	}

noclean:

clean :

	return r;
}

int clnt_serv_thread_func(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	uint32_t *oWantReconnect)
{
	int r = 0;

	uint32_t WantReconnect = false;

	sp<GsHostSurrogate> HostSurrogate(new GsHostSurrogate((*ioConnectionSurrogate)->mHost));

	if (!!(r = aux_serv_thread_func(WorkerDataRecv, WorkerDataSend, &HostSurrogate, ioConnectionSurrogateMap)))
		GS_ERR_NO_CLEAN(1);

noclean:
	if (!!r) {
		WantReconnect = true;
	}

	if (oWantReconnect)
		*oWantReconnect = WantReconnect;

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

bool clnt_state_reconnect_have_remaining(ClntStateReconnect *StateReconnect) {
	return StateReconnect->NumReconnectionsLeft >= 1;
}

int clnt_state_reconnect_expend(ClntStateReconnect *ioStateReconnect) {
	int r = 0;

	if (! clnt_state_reconnect_have_remaining(ioStateReconnect))
		GS_ERR_CLEAN(1);

	ioStateReconnect->NumReconnectionsLeft -= 1;

clean:

	return r;
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
		{ Code = GS_CLNT_STATE_CODE_NEED_REPOSITORY; goto need_repository; }
	if (! State->mTreeHeadOid)
		{ Code = GS_CLNT_STATE_CODE_NEED_TREE_HEAD; goto need_tree_head; }
	if (! State->mTreelist || ! State->mMissingTreelist)
		{ Code = GS_CLNT_STATE_CODE_NEED_TREELIST; goto need_treelist; }
	if (! State->mMissingBloblist || ! State->mTreePacketWithOffset)
		{ Code = GS_CLNT_STATE_CODE_NEED_BLOBLIST; goto need_bloblist; }
	if (! State->mWrittenBlob || ! State->mWrittenTree)
		{ Code = GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE; goto need_written_blob_and_tree; }
	if (true)
		{ Code = GS_CLNT_STATE_CODE_NEED_NOTHING; goto need_nothing; }

need_repository:
	if (State->mTreeHeadOid)
		GS_ERR_CLEAN(1);
need_tree_head:
	if (State->mTreelist || State->mMissingTreelist)
		GS_ERR_CLEAN(1);
need_treelist:
	if (State->mMissingBloblist || State->mTreePacketWithOffset)
		GS_ERR_CLEAN(1);
need_bloblist:
	if (State->mWrittenBlob || State->mWrittenTree)
		GS_ERR_CLEAN(1);
need_written_blob_and_tree:
need_nothing:

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

/* FIXME: presumably unused - refactor */
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
			enet_peer_disconnect_now(newpeer, 0);

		if (newhost)
			enet_host_destroy(newhost);
	}

	return r;
}

int clnt_state_crank(
	const sp<ClntState> &State,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const sp<ServAuxData> &ServAuxData,
	ServWorkerData *WorkerDataRecv,
	ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend)
{
	int r = 0;

	uint32_t Code = 0;

	if (!!(r = clnt_state_code(State.get(), &Code)))
		GS_GOTO_CLEAN();

	switch (Code) {
	case GS_CLNT_STATE_CODE_NEED_REPOSITORY:
	{
		if (!!(r = clnt_state_need_repository_setup(
			State,
			RepoMainOpenPathBuf, LenRepoMainOpenPath,
			ServAuxData)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_TREE_HEAD:
	{
		if (!!(r = clnt_state_need_tree_head_setup(
			State,
			RefNameMainBuf, LenRefNameMain,
			ServAuxData.get(),
			WorkerDataRecv,
			WorkerDataSend,
			RequestForSend)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_TREELIST:
	{
		if (!!(r = clnt_state_need_treelist_setup(State,
			ServAuxData.get(), WorkerDataRecv, WorkerDataSend, RequestForSend)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_BLOBLIST:
	{
		if (!!(r = clnt_state_need_bloblist_setup(State,
			ServAuxData.get(), WorkerDataRecv, WorkerDataSend, RequestForSend)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE:
	{
		if (!!(r = clnt_state_need_written_blob_and_tree_setup(State,
			ServAuxData.get(), WorkerDataRecv, WorkerDataSend, RequestForSend)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_NOTHING:
	{
		GS_ASSERT(0);
	}
	break;

	default:
	{
		GS_ASSERT(0);
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
	GS_ASSERT(0);
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

int clnt_state_need_repository_noown(
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	git_repository **oRepositoryT)
{
	int r = 0;

	if (!!(r = aux_repository_open(RepoMainOpenPathBuf, oRepositoryT)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_need_tree_head_noown(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	git_repository *RepositoryT,
	ServAuxData *ServAuxData,
	ServWorkerData *WorkerDataRecv,
	ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	git_oid *oTreeHeadOid)
{
	int r = 0;

	std::string Buffer;
	gs_packet_unique_t GsPacket;
	ENetPacket *Packet = NULL;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = aux_frame_full_write_request_latest_commit_tree(gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(ServAuxData, WorkerDataSend, RequestForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_request_dequeue_packet(WorkerDataRecv, &GsPacket)))
		GS_GOTO_CLEAN();

	Packet = *GsPacket;

	if (! Packet)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, oTreeHeadOid->id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_latest_commit_tree_oid(RepositoryT, RefNameMainBuf, &CommitHeadOidT, &TreeHeadOidT)))
		GS_GOTO_CLEAN();

	if (git_oid_cmp(&TreeHeadOidT, oTreeHeadOid) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &CommitHeadOidT);
		printf("[clnt] Have latest [%.*s]\n", GIT_OID_HEXSZ, buf);
	}

clean:

	return r;
}

int clnt_state_need_treelist_noown(
	git_repository *RepositoryT,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	git_oid *TreeHeadOid, std::vector<git_oid> *oTreelist, std::vector<git_oid> *oMissingTreelist)
{
	int r = 0;

	std::string Buffer;
	gs_packet_unique_t GsPacket;
	ENetPacket *Packet = NULL;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	GS_BYPART_DATA_VAR(OidVector, BypartTreelist);
	GS_BYPART_DATA_INIT(OidVector, BypartTreelist, oTreelist);

	if (!!(r = aux_frame_full_write_request_treelist(TreeHeadOid->id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(ServAuxData, WorkerDataSend, RequestForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_request_dequeue_packet(WorkerDataRecv, &GsPacket)))
		GS_GOTO_CLEAN();

	Packet = *GsPacket;

	if (! Packet)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREELIST))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartTreelist, gs_bypart_cb_OidVector)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_missing_trees(RepositoryT, oTreelist, oMissingTreelist)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_need_bloblist_noown(
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

	ENetPacket *PacketTree = NULL;

	GsStrided MissingTreelistStrided = {};

	uint32_t BufferTreeLen = 0;

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = gs_strided_for_oid_vec_cpp(MissingTreelist, &MissingTreelistStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_trees(MissingTreelistStrided, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(ServAuxData, WorkerDataSend, RequestForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketTree Lifetime start */

	if (!!(r = aux_packet_request_dequeue_packet(WorkerDataRecv, oPacketTree)))
		GS_GOTO_CLEAN();

	PacketTree = **oPacketTree;

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

int clnt_state_need_written_blob_and_tree_noown(
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
	ENetPacket *PacketBlob = NULL;
	ENetPacket *PacketTree = NULL;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	GsStrided MissingBloblistStrided = {};

	uint32_t BufferBlobLen;
	uint32_t OffsetSizeBufferBlob;
	uint32_t OffsetObjectBufferBlob;

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = gs_strided_for_oid_vec_cpp(MissingBloblist, &MissingBloblistStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_blobs(MissingBloblistStrided, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_packet_response_queue_interrupt_request_reliable(ServAuxData, WorkerDataSend, RequestForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketBlob Lifetime start */

	if (!!(r = aux_packet_request_dequeue_packet(WorkerDataRecv, &GsPacketBlob)))
		GS_GOTO_CLEAN();

	PacketBlob = *GsPacketBlob;

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

	PacketTree = *GsPacketTree;

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

int clnt_state_need_repository_setup(
	const sp<ClntState> &State,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const sp<ServAuxData> &ServAuxData)
{
	int r = 0;

	sp<git_repository *> RepositoryT(new git_repository *);

	if (!!(r = clnt_state_need_repository_noown(
		RepoMainOpenPathBuf, LenRepoMainOpenPath,
		RepositoryT.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 2, a,
		{ a.mRepositoryT = RepositoryT; });

clean:
	if (!!r) {
		if (RepositoryT)
			git_repository_free(*RepositoryT);
	}

	return r;
}

int clnt_state_need_tree_head_setup(
	const sp<ClntState> &State,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	ServAuxData *ServAuxData,
	ServWorkerData *WorkerDataRecv,
	ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend)
{
	int r = 0;

	sp<git_oid> TreeHeadOid(new git_oid);

	git_repository * const RepositoryT = *State->mRepositoryT;

	std::string Buffer;
	gs_packet_t Packet;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	if (!!(r = clnt_state_need_tree_head_noown(
		RefNameMainBuf, LenRefNameMain,
		RepositoryT,
		ServAuxData,
		WorkerDataRecv,
		WorkerDataSend,
		RequestForSend,
		TreeHeadOid.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 3, a,
		{ a.mTreeHeadOid = TreeHeadOid; });

clean:

	return r;
}

int clnt_state_need_treelist_setup(const sp<ClntState> &State,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend, ServWorkerRequestData *RequestForSend)
{
	int r = 0;

	sp<std::vector<git_oid> > Treelist(new std::vector<git_oid>);
	sp<std::vector<git_oid> > MissingTreelist(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<git_oid> &TreeHeadOid = State->mTreeHeadOid;

	if (!!(r = clnt_state_need_treelist_noown(
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

int clnt_state_need_bloblist_setup(const sp<ClntState> &State,
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

	if (!!(r = clnt_state_need_bloblist_noown(
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

int clnt_state_need_written_blob_and_tree_setup(const sp<ClntState> &State,
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

	if (!!(r = clnt_state_need_written_blob_and_tree_noown(
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

void serv_worker_thread_func_f(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const char *RepoSelfUpdateOpenPathBuf, size_t LenRepoSelfUpdateOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend)
{
	int r = 0;

	gs_current_thread_name_set_cstr("serv_worker");

	log_guard_t log(GS_LOG_GET("serv_worker"));

	if (!!(r = serv_worker_thread_func_reconnecter(
		RefNameMainBuf, LenRefNameMain,
		RefNameSelfUpdateBuf, LenRefNameSelfUpdate,
		RepoMainOpenPathBuf, LenRepoMainOpenPath,
		RepoSelfUpdateOpenPathBuf, LenRepoSelfUpdateOpenPath,
		ServAuxData,
		WorkerDataRecv,
		WorkerDataSend)))
	{
		GS_ASSERT(0);
	}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void serv_serv_aux_thread_func_f(sp<ServAuxData> ServAuxData) {
	int r = 0;

	gs_current_thread_name_set_cstr("serv_aux");

	log_guard_t log(GS_LOG_GET("serv_aux"));

	if (!!(r = aux_serv_aux_thread_func_reconnecter(ServAuxData)))
		GS_ASSERT(0);

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void serv_thread_func_f(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	uint32_t ServPort)
{
	int r = 0;

	gs_current_thread_name_set_cstr("serv_serv");

	log_guard_t log(GS_LOG_GET("serv_serv"));

	if (!!(r = serv_serv_thread_func_reconnecter(WorkerDataRecv, WorkerDataSend, AuxData, ServPort)))
		GS_ASSERT(0);

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void clnt_worker_thread_func_f(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend)
{
	int r = 0;

	gs_current_thread_name_set_cstr("clnt_worker");

	log_guard_t log(GS_LOG_GET("clnt_worker"));

	if (!!(r = clnt_worker_thread_func_reconnecter(
		RefNameMainBuf, LenRefNameMain,
		RepoMainOpenPathBuf, LenRepoMainOpenPath,
		ServAuxData,
		WorkerDataRecv,
		WorkerDataSend)))
	{
		GS_ASSERT(0);
	}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void clnt_serv_aux_thread_func_f(sp<ServAuxData> ServAuxData) {
	int r = 0;

	gs_current_thread_name_set_cstr("clnt_aux");

	log_guard_t log(GS_LOG_GET("clnt_aux"));

	if (!!(r = aux_serv_aux_thread_func_reconnecter(ServAuxData)))
		GS_ASSERT(0);

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void clnt_thread_func_f(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName)
{
	int r = 0;

	gs_current_thread_name_set_cstr("clnt_serv");

	log_guard_t log(GS_LOG_GET("clnt_serv"));

	if (!!(r = clnt_serv_thread_func_reconnecter(
		WorkerDataRecv,
		WorkerDataSend,
		AuxData,
		ServPort,
		ServHostNameBuf, LenServHostName)))
	{
		GS_ASSERT(0);
	}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int aux_full_create_connection_server(
	uint32_t ServPort,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	sp<FullConnectionClient> *oConnectionClient)
{
	int r = 0;

	sp<FullConnectionClient> ConnectionClient;

	{
		sp<ServWorkerData> WorkerDataSend(new ServWorkerData);
		sp<ServWorkerData> WorkerDataRecv(new ServWorkerData);
		sp<ServAuxData> DataAux(new ServAuxData);

		sp<std::thread> ServerWorkerThread(new std::thread(
			serv_worker_thread_func_f,
			RefNameMainBuf, LenRefNameMain,
			RefNameSelfUpdateBuf, LenRefNameSelfUpdate,
			RepoMainPathBuf, LenRepoMainPath,
			RepoSelfUpdatePathBuf, LenRepoSelfUpdatePath,
			DataAux,
			WorkerDataRecv,
			WorkerDataSend));

		sp<std::thread> ServerAuxThread(new std::thread(
			serv_serv_aux_thread_func_f,
			DataAux));

		sp<std::thread> ServerThread(new std::thread(
			serv_thread_func_f,
			WorkerDataRecv,
			WorkerDataSend,
			DataAux,
			ServPort));

		ConnectionClient = sp<FullConnectionClient>(new FullConnectionClient(ServerWorkerThread, ServerAuxThread, ServerThread));
	}

	if (oConnectionClient)
		*oConnectionClient = ConnectionClient;

clean:

	return r;
}

int aux_full_create_connection_client(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	sp<FullConnectionClient> *oConnectionClient)
{
	int r = 0;

	sp<FullConnectionClient> ConnectionClient;

	{
		sp<ServWorkerData> WorkerDataSend(new ServWorkerData);
		sp<ServWorkerData> WorkerDataRecv(new ServWorkerData);
		sp<ServAuxData> AuxData(new ServAuxData);

		sp<std::thread> ClientWorkerThread(new std::thread(
			clnt_worker_thread_func_f,
			RefNameMainBuf, LenRefNameMain,
			RepoMainPathBuf, LenRepoMainPath,
			AuxData,
			WorkerDataRecv,
			WorkerDataSend));

		sp<std::thread> ClientAuxThread(new std::thread(
			clnt_serv_aux_thread_func_f,
			AuxData));

		sp<std::thread> ClientThread(new std::thread(
			clnt_thread_func_f,
			WorkerDataRecv,
			WorkerDataSend,
			AuxData,
			ServPort,
			ServHostNameBuf, LenServHostName));

		ConnectionClient = sp<FullConnectionClient>(new FullConnectionClient(ClientWorkerThread, ClientAuxThread, ClientThread));
	}

	if (oConnectionClient)
		*oConnectionClient = ConnectionClient;

clean:

	return r;
}

int stuff2() {
	int r = 0;

	confmap_t KeyVal;

	GsAuxConfigCommonVars CommonVars = {};

	sp<FullConnectionClient> FcsServ;
	sp<FullConnectionClient> FcsClnt;

	if (!!(r = aux_config_read_interpret_relative_current_executable("../data/", "gittest_config_serv.conf", &KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_get_common_vars(KeyVal, &CommonVars)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_full_create_connection_server(
		CommonVars.ServPort,
		CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain,
		CommonVars.RefNameSelfUpdateBuf, CommonVars.LenRefNameSelfUpdate,
		CommonVars.RepoMainPathBuf, CommonVars.LenRepoMainPath,
		CommonVars.RepoSelfUpdatePathBuf, CommonVars.LenRepoSelfUpdatePath,
		&FcsServ)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_full_create_connection_client(
		CommonVars.ServPort,
		CommonVars.ServHostNameBuf, CommonVars.LenServHostName,
		CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain,
		CommonVars.RepoMainPathBuf, CommonVars.LenRepoMainPath,
		&FcsClnt)))
	{
		GS_GOTO_CLEAN();
	}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

clean:

	return r;
}
