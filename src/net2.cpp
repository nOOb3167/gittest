#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <gittest/misc.h>
#include <gittest/log.h>
#include <gittest/net2.h>

/** Use as a deleter function of std::thread shared pointers
	whom are to be detached before destruction.

    @sa
	   GsFullConnection
*/
static void gs_sp_thread_detaching_deleter(std::thread *t)
{
	if (t->joinable())
		t->detach();
	delete t;
}

int gs_helper_api_worker_exit(struct GsWorkerData *WorkerDataSend)
{
	int r = 0;

	struct GsWorkerRequestData Request = {};

	if (!!(r = gs_worker_request_data_type_exit_make(&Request)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_request_enqueue(WorkerDataSend, &Request)))
		GS_GOTO_CLEAN();

	/* NOTE: special success path return semantics */
	// FIXME: logging success path
	GS_ERR_NO_CLEAN(GS_ERRCODE_EXIT);

noclean:

clean:

	return r;
}

int gs_helper_api_worker_reconnect(struct GsWorkerData *WorkerDataSend)
{
	int r = 0;

	struct GsWorkerRequestData Request = {};

	if (!!(r = gs_worker_request_data_type_reconnect_prepare_make(&Request)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_request_enqueue(WorkerDataSend, &Request)))
		GS_GOTO_CLEAN();

	/* NOTE: special success path return semantics */
	// FIXME: logging success path
	GS_ERR_NO_CLEAN(GS_ERRCODE_RECONNECT);

noclean:

clean:

	return r;
}

int gs_helper_api_ntwk_exit(struct GsWorkerDataVec *WorkerDataVecRecv)
{
	int r = 0;

	struct GsWorkerRequestData Request = {};

	if (!!(r = gs_worker_request_data_type_exit_make(&Request)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < WorkerDataVecRecv->mLen; i++)
		if (!!(r = gs_worker_request_enqueue(gs_worker_data_vec_id(WorkerDataVecRecv, i), &Request)))
			GS_GOTO_CLEAN();

	/* NOTE: special success path return semantics */
	// FIXME: logging success path
	GS_ERR_NO_CLEAN(GS_ERRCODE_EXIT);

noclean:

clean:

	return r;
}

int gs_helper_api_ntwk_reconnect()
{
	int r = 0;

	/* NOTE: special success path return semantics */
	// FIXME: logging success path
	GS_ERR_NO_CLEAN(GS_ERRCODE_RECONNECT);

noclean:

clean:

	return r;
}

int gs_helper_api_ntwk_extra_host_create_and_notify(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	struct GsHostSurrogate *ioHostSurrogate)
{
	int r = 0;

	/* create batch then transfer ownership to notification request */
	std::vector<GsExtraWorker *> ExtraWorker(WorkerDataVecRecv->mLen, NULL);

	if (!!(r = ExtraHostCreate->cb_create_batch_t(
		ExtraHostCreate,
		ioHostSurrogate,
		ioConnectionSurrogateMap,
		ExtraWorker.size(),
		ExtraWorker.data())))
	{
		GS_GOTO_CLEAN();
	}

	// FIXME: change ownership of ExtraWorker param to owned inside double notification function
	//   ex function must release ExtraWorker on failure
	for (uint32_t i = 0; i < WorkerDataVecRecv->mLen; i++)
		if (!!(r = gs_worker_request_enqueue_double_notify(
			gs_worker_data_vec_id(WorkerDataVecRecv, i),
			GS_ARGOWN(ExtraWorker.data() + i, GsExtraWorker))))
		{
			GS_GOTO_CLEAN();
		}

clean:
	if (!!r) {
		for (size_t i = 0; i < ExtraWorker.size(); i++)
			GS_DELETE_VF(ExtraWorker[i], cb_destroy_t);
	}

	return r;
}

int gs_affinity_queue_create(
	size_t NumWorkers,
	struct GsAffinityQueue **oAffinityQueue)
{
	int r = 0;

	struct GsAffinityQueue *AffinityQueue = new GsAffinityQueue();

	for (size_t i = 0; i < NumWorkers; i++)
		AffinityQueue->mAffinityList.push_back(i);

	if (oAffinityQueue)
		*oAffinityQueue = AffinityQueue;

clean:

	return r;
}

int gs_affinity_queue_destroy(struct GsAffinityQueue *AffinityQueue)
{
	GS_DELETE(&AffinityQueue);
	return 0;
}

int gs_affinity_queue_worker_acquire_ready(
	struct GsAffinityQueue *AffinityQueue,
	gs_connection_surrogate_id_t ConnectionId,
	gs_worker_id_t *oWorkerIdReady)
{
	int r = 0;

	gs_worker_id_t WorkerIdReady = 0;

	{
		std::lock_guard<std::mutex> lock(AffinityQueue->mMutexData);

		auto it = AffinityQueue->mAffinityList.begin();

		for (auto it = AffinityQueue->mAffinityList.begin(); it != AffinityQueue->mAffinityList.end(); it++) {
			gs_worker_id_t WorkerId = *it;
			auto it2 = AffinityQueue->mAffinityMap.find(ConnectionId);
			if (it2 != AffinityQueue->mAffinityMap.end()) {
				/* acquire to worker currently already holding lease for ConnectionId */
				WorkerIdReady = it2->second;
			}
			else {
				/* acquire to worker any */
				WorkerIdReady = AffinityQueue->mAffinityList.front();
				// move to back of list as a form of load balancing
				AffinityQueue->mAffinityList.pop_front();
				AffinityQueue->mAffinityList.push_back(WorkerIdReady);
			}
		}
	}

	if (oWorkerIdReady)
		*oWorkerIdReady = WorkerIdReady;

clean:

	return r;
}

int gs_affinity_queue_worker_acquire_lease(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t WorkerId,
	gs_connection_surrogate_id_t ConnectionId)
{
	int r = 0;

	{
		std::unique_lock<std::mutex> lock(AffinityQueue->mMutexData);

		/* lease might already exist (someone else is servicing) - wait */

		AffinityQueue->mCondDataLeaseReleased.wait(lock,
			[&]() { return AffinityQueue->mAffinityMap.find(ConnectionId) == AffinityQueue->mAffinityMap.end(); });

		AffinityQueue->mAffinityMap[ConnectionId] = WorkerId;
	}

clean:

	return r;
}

int gs_affinity_queue_worker_release_lease(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t ExpectedWorkerId,
	gs_connection_surrogate_id_t ConnectionId)
{
	int r = 0;

	bool WasALeaseReleased = false;

	{
		std::unique_lock<std::mutex> lock(AffinityQueue->mMutexData);

		auto it = AffinityQueue->mAffinityMap.find(ConnectionId);

		if (it != AffinityQueue->mAffinityMap.end() &&
			it->second == ExpectedWorkerId)
		{
			AffinityQueue->mAffinityMap.erase(it);
			WasALeaseReleased = true;
		}
	}

	if (WasALeaseReleased)
		AffinityQueue->mCondDataLeaseReleased.notify_all();

clean:

	return r;
}

int gs_affinity_queue_request_dequeue_coupled(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerData *WorkerData,
	gs_worker_id_t WorkerId,
	struct GsWorkerRequestData *oValRequest)
{
	int r = 0;

	GsWorkerRequestData Request = {};

	if (!!(r = gs_worker_request_dequeue(WorkerData, &Request)))
		GS_GOTO_CLEAN();

	if (Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET) {
		if (!!(r = gs_affinity_queue_worker_acquire_lease(AffinityQueue, WorkerId, Request.mId)))
			GS_GOTO_CLEAN();
	}

	if (oValRequest)
		*oValRequest = Request;

clean:

	return r;
}

int gs_affinity_queue_request_finish(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t ExpectedWorkerId,
	struct GsWorkerRequestData valRequest)
{
	int r = 0;

	if (valRequest.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET) {
		if (!!(r = gs_affinity_queue_worker_release_lease(
			AffinityQueue,
			ExpectedWorkerId,
			valRequest.mId)))
		{
			GS_GOTO_CLEAN();
		}
	}

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

int clnt_state_reconnect_make_default(struct ClntStateReconnect *oStateReconnect) {
	ClntStateReconnect StateReconnect;
	StateReconnect.NumReconnections = GS_CONNECT_NUMRECONNECT;
	StateReconnect.NumReconnectionsLeft = StateReconnect.NumReconnections;
	if (oStateReconnect)
		*oStateReconnect = StateReconnect;
	return 0;
}

bool clnt_state_reconnect_have_remaining(struct ClntStateReconnect *StateReconnect) {
	return StateReconnect->NumReconnectionsLeft >= 1;
}

int clnt_state_reconnect_expend(struct ClntStateReconnect *ioStateReconnect) {
	int r = 0;

	if (! clnt_state_reconnect_have_remaining(ioStateReconnect))
		GS_ERR_CLEAN(1);

	ioStateReconnect->NumReconnectionsLeft -= 1;

clean:

	return r;
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

int gs_worker_data_create(struct GsWorkerData **oWorkerData)
{
	struct GsWorkerData *WorkerData = new GsWorkerData();

	WorkerData->mWorkerQueue = sp<std::deque<GsWorkerRequestData> >(new std::deque<GsWorkerRequestData>);
	WorkerData->mWorkerDataMutex = sp<std::mutex>(new std::mutex);
	WorkerData->mWorkerDataCond = sp<std::condition_variable>(new std::condition_variable);

	if (oWorkerData)
		*oWorkerData = WorkerData;

	return 0;
}

int gs_worker_data_destroy(struct GsWorkerData *WorkerData)
{
	GS_DELETE(&WorkerData);
	return 0;
}

int gs_ctrl_con_create(
	uint32_t NumNtwks,
	uint32_t NumWorkers,
	struct GsCtrlCon **oCtrlCon)
{
	struct GsCtrlCon *CtrlCon = new GsCtrlCon();

	CtrlCon->mNumNtwks = NumNtwks;
	CtrlCon->mNumWorkers = NumWorkers;
	CtrlCon->mExitedSignalLeft = NumNtwks + NumWorkers;
	CtrlCon->mCtrlConMutex = sp<std::mutex>(new std::mutex);
	CtrlCon->mCtrlConCondExited = sp<std::condition_variable>(new std::condition_variable);

	if (oCtrlCon)
		*oCtrlCon = CtrlCon;

	return 0;
}

int gs_ctrl_con_destroy(struct GsCtrlCon *CtrlCon)
{
	if (!CtrlCon)
		return 0;

	delete CtrlCon;

	return 0;
}

int gs_ctrl_con_signal_exited(struct GsCtrlCon *CtrlCon)
{
	bool WantNotify = false;
	{
		std::unique_lock<std::mutex> lock(*CtrlCon->mCtrlConMutex);
		if (CtrlCon->mExitedSignalLeft)
			CtrlCon->mExitedSignalLeft -= 1;
		if (!CtrlCon->mExitedSignalLeft)
			WantNotify = true;
	}
	if (WantNotify)
		CtrlCon->mCtrlConCondExited->notify_all();
	return 0;
}

int gs_ctrl_con_wait_exited(struct GsCtrlCon *CtrlCon)
{
	{
		std::unique_lock<std::mutex> lock(*CtrlCon->mCtrlConMutex);
		CtrlCon->mCtrlConCondExited->wait(lock, [&]() { return ! CtrlCon->mExitedSignalLeft; });
	}
	return 0;
}

int gs_ctrl_con_get_num_workers(struct GsCtrlCon *CtrlCon, uint32_t *oNumWorkers)
{
	{
		std::unique_lock<std::mutex> lock(*CtrlCon->mCtrlConMutex);
		*oNumWorkers = CtrlCon->mNumWorkers;
	}
	return 0;
}

int gs_extra_host_create_cb_destroy_host_t_enet_host_destroy(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate)
{
	int r = 0;

	if (ioHostSurrogate->mHost)
		enet_host_destroy(ioHostSurrogate->mHost);
	ioHostSurrogate->mHost = NULL;

clean:

	return r;
}

int gs_extra_host_create_cb_destroy_t_delete(struct GsExtraHostCreate *ExtraHostCreate)
{
	int r = 0;

	GS_DELETE(&ExtraHostCreate);

clean:

	return r;
}

int gs_worker_request_data_type_packet_make(
	struct GsPacket *Packet,
	gs_connection_surrogate_id_t Id,
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET;
	W.mPacket = Packet;
	W.mId = Id;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

int gs_worker_request_data_type_reconnect_prepare_make(
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

int gs_worker_request_data_type_reconnect_reconnect_make(
	struct GsExtraWorker *ExtraWorker,
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT;
	W.mExtraWorker = ExtraWorker;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

int gs_worker_request_data_type_exit_make(
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

int gs_worker_request_data_type_disconnect_make(
	gs_connection_surrogate_id_t Id,
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT;
	W.mId = Id;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

bool gs_worker_request_isempty(struct GsWorkerData *pThis)
{
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		return pThis->mWorkerQueue->empty();
	}
}

int gs_worker_request_enqueue(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *valRequestData)
{
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		pThis->mWorkerQueue->push_back(*valRequestData);
	}
	pThis->mWorkerDataCond->notify_one();
	return 0;
}

int gs_worker_request_enqueue_double_notify(
	struct GsWorkerData *pThis,
	struct GsExtraWorker *ExtraWorker)
{
	int r = 0;

	struct GsWorkerRequestData Prepare = {};
	struct GsWorkerRequestData Reconnect = {};

	if (!!(r = gs_worker_request_data_type_reconnect_prepare_make(&Prepare)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_request_data_type_reconnect_reconnect_make(ExtraWorker, &Reconnect)))
		GS_GOTO_CLEAN();

	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);

		pThis->mWorkerQueue->push_back(Prepare);
		pThis->mWorkerQueue->push_back(Reconnect);
	}
	pThis->mWorkerDataCond->notify_one();

clean:

	return 0;
}

/** @retval GS_ERRCODE_TIMEOUT on timeout */
int gs_worker_request_dequeue_timeout(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs)
{
	std::chrono::milliseconds To = std::chrono::milliseconds(TimeoutMs);
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		if (! pThis->mWorkerDataCond->wait_for(lock, To, [&]() { return !pThis->mWorkerQueue->empty(); }))
			return GS_ERRCODE_TIMEOUT;
		*oValRequestData = pThis->mWorkerQueue->front();
		pThis->mWorkerQueue->pop_front();
	}
	return 0;
}

int gs_worker_request_dequeue(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData)
{
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		pThis->mWorkerDataCond->wait(lock, [&]() { return !pThis->mWorkerQueue->empty(); });
		*oValRequestData = pThis->mWorkerQueue->front();
		pThis->mWorkerQueue->pop_front();
	}
	return 0;
}

int gs_worker_request_dequeue_all_opt_cpp(
	struct GsWorkerData *pThis,
	std::deque<struct GsWorkerRequestData> *oValRequestData)
{
	oValRequestData->clear();
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		oValRequestData->clear();
		oValRequestData->swap(*pThis->mWorkerQueue);
	}
	return 0;
}

int gs_worker_packet_enqueue(
	struct GsWorkerData *pThis,
	struct GsIntrTokenSurrogate *IntrToken,
	gs_connection_surrogate_id_t Id,
	const char *Data, uint32_t DataSize)
{
	int r = 0;

	GsPacketSurrogate PacketSurrogate = {};
	GsPacket *Packet = NULL;
	GsWorkerRequestData Request = {};

	if (!(PacketSurrogate.mPacket = enet_packet_create(Data, DataSize, ENET_PACKET_FLAG_RELIABLE)))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_packet_create(&Packet, &PacketSurrogate)))
		GS_GOTO_CLEAN();

	/* lost ownership */
	if (!!(r = gs_packet_surrogate_release_ownership(&PacketSurrogate)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_request_data_type_packet_make(Packet, Id, &Request)))
		GS_GOTO_CLEAN();

	/* NOTE: lost ownership of Request (but can't just modify Request.mPacket since pointer field) */
	if (!!(r = gs_worker_request_enqueue(pThis, &Request)))
		GS_GOTO_CLEAN();

	IntrToken->mIntrToken->cb_token_interrupt(IntrToken->mIntrToken);

clean:
	// FIXME: resource management

	return r;
}

/** Principal blocking read facility for blocking-type worker.

	@retval: GS_ERRCODE_RECONNECT if reconnect request dequeued
*/
int gs_worker_packet_dequeue(
	struct GsWorkerData *pThis,
	struct GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId)
{
	int r = 0;

	GsWorkerRequestData Request = {};

	if (!!(r = gs_worker_request_dequeue(pThis, &Request)))
		GS_GOTO_CLEAN();

	/* actually - reconnect_prepare only is expected.
	 * if a double notify is triggered and we somehow get just the reconnect_reconnect,
	 * we, here, have jst discarded it - but it contains possibly valuable extra data. */
	if (Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE ||
		Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT)
	{
		GS_ERR_CLEAN(GS_ERRCODE_RECONNECT);
	}

	/* ensure request field validity by type check */
	if (Request.type != GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET)
		GS_ERR_CLEAN(1);

	if (oPacket)
		*oPacket = Request.mPacket;

	if (oId)
		*oId = Request.mId;

clean:

	return r;
}

/** Principal read facility for blocking-type worker.

	Queue WorkerDataSend message GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE on timeout.

	@retval: GS_ERRCODE_TIMEOUT   if timeout attempting to dequeue
	@retval: GS_ERRCODE_RECONNECT if reconnect request dequeued
*/
int gs_worker_packet_dequeue_timeout_reconnects(
	struct GsWorkerData *pThis,
	struct GsWorkerData *WorkerDataSend,
	uint32_t TimeoutMs,
	struct GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId)
{
	int r = 0;

	GsWorkerRequestData Request = {};

	r = gs_worker_request_dequeue_timeout(pThis, &Request, TimeoutMs);
	if (!!r && r == GS_ERRCODE_TIMEOUT) {
		int r2 = gs_helper_api_worker_reconnect(WorkerDataSend);
		GS_ERR_NO_CLEAN(r2);
	}
	if (!!r)
		GS_GOTO_CLEAN();

	if (Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE ||
		Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT)
	{
		GS_ERR_NO_CLEAN(GS_ERRCODE_RECONNECT);
	}

	if (Request.type != GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET)
		GS_ERR_CLEAN(1);

noclean:
	if (oPacket && Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET)
		*oPacket = Request.mPacket;

	if (oId && Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET)
		*oId = Request.mId;

clean:

	return r;
}

int gs_worker_data_vec_create(
	uint32_t NumWorkers,
	struct GsWorkerDataVec **oWorkerDataVec)
{
	int r = 0;

	struct GsWorkerDataVec *WorkerDataVec = new GsWorkerDataVec();

	WorkerDataVec->mLen = NumWorkers;
	WorkerDataVec->mData = new GsWorkerData * [NumWorkers];

	for (size_t i = 0; i < WorkerDataVec->mLen; i++)
		if (!!(r = gs_worker_data_create(&WorkerDataVec->mData[i])))
			GS_GOTO_CLEAN();

	if (oWorkerDataVec)
		*oWorkerDataVec = WorkerDataVec;

clean:

	return r;
}

int gs_worker_data_vec_destroy(
	struct GsWorkerDataVec *WorkerDataVec)
{
	for (size_t i = 0; i < WorkerDataVec->mLen; i++)
		GS_DELETE_F(WorkerDataVec->mData[i], gs_worker_data_destroy);
	delete [] WorkerDataVec->mData;
	delete WorkerDataVec;
	return 0;
}

struct GsWorkerData * gs_worker_data_vec_id(
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t WorkerId)
{
	if (WorkerId >= WorkerDataVec->mLen)
		GS_ASSERT(0);
	return WorkerDataVec->mData[WorkerId];
}

int gs_ntwk_reconnect_expend(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct ClntStateReconnect *ioStateReconnect,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	struct GsHostSurrogate *ioHostSurrogate,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

	if (*ioWantReconnect) {

		GsExtraWorker *ExtraWorker = NULL;

		GS_LOG(I, S, "reconnection wanted - disconnecting");

		if (!!(r = ExtraHostCreate->cb_destroy_host_t(
			ExtraHostCreate,
			ioHostSurrogate)))
		{
			GS_GOTO_CLEAN();
		}

		GS_LOG(I, S, "reconnection wanted - performing (create extra worker, use for notification)");

		if (!!(r = gs_helper_api_ntwk_extra_host_create_and_notify(
			ExtraHostCreate,
			WorkerDataVecRecv,
			ioConnectionSurrogateMap,
			ioHostSurrogate)))
		{
			GS_GOTO_CLEAN();
		}
	}

	/* connection is ensured if no errors occurred (either existing or newly established)
	*  in either case we no longer need to reconnect */
	if (ioWantReconnect)
		*ioWantReconnect = false;

clean:

	return r;
}

/** Two part connection registration.
    The connection is assigned an entry with Id within the connection map.
	That same Id is then bonded to the ENetPeer 'data' field.

   @param valConnectionSurrogate copied / copy-constructed (for shared_ptr use)
*/
int gs_aux_aux_aux_connection_register_transfer_ownership(
	struct GsConnectionSurrogate valConnectionSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t *oAssignedId)
{
	int r = 0;

	ENetPeer *peer = valConnectionSurrogate.mPeer;

	gs_connection_surrogate_id_t Id = 0;

	GS_BYPART_DATA_VAR(GsConnectionSurrogateId, ctxstruct);

	/* assign entry with id */
	if (!!(r = gs_connection_surrogate_map_insert(
		ioConnectionSurrogateMap,
		valConnectionSurrogate,
		&Id)))
	{
		GS_GOTO_CLEAN();
	}

	GS_BYPART_DATA_INIT(GsConnectionSurrogateId, ctxstruct, Id);

	/* bond to the peer */
	/* NOTE: making use of property that mPeer pointer field was copied into the connection surrogate map */
	// FIXME: sigh raw allocation, deletion occurs in principle on receipt of ENET_EVENT_TYPE_DISCONNECT events
	peer->data = new GsBypartCbDataGsConnectionSurrogateId(ctxstruct);

	if (oAssignedId)
		*oAssignedId = Id;

clean:

	return r;
}

int gs_aux_aux_aux_cb_last_chance_t(
	struct ENetIntr *Intr,
	struct ENetIntrToken *IntrToken)
{
	int r = 0;

	ENetIntrNtwk *pIntr = (ENetIntrNtwk *) Intr;

	if (! gs_worker_request_isempty(pIntr->WorkerDataSend))
		return 1;

	return 0;
}

int gs_ntwk_host_service_worker_disconnect(
	struct GsHostSurrogate *ReferenceHostSurrogate,
	struct GsWorkerRequestData *RequestSend,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	struct GsConnectionSurrogate Con = {};
	uint32_t ConIsPresent = false;

	if (!!(r = gs_connection_surrogate_map_get_try(
		ioConnectionSurrogateMap,
		RequestSend->mId,
		&Con,
		&ConIsPresent)))
	{
		GS_GOTO_CLEAN();
	}

	if (!ConIsPresent)
		GS_ERR_NO_CLEAN_L(0, W, PF, "suppressing disconnect for GsConnectionSurrogate [%llu]", (unsigned long long) RequestSend->mId);

	if (Con.mHost != ReferenceHostSurrogate->mHost)
		GS_ERR_NO_CLEAN_L(0, W, PF, "suppressing disconnect for GsConnectionSurrogate [%llu]", (unsigned long long) RequestSend->mId);

	if (!!(r = gs_connection_surrogate_map_erase(ioConnectionSurrogateMap, RequestSend->mId)))
		GS_GOTO_CLEAN();

	enet_peer_disconnect(Con.mPeer, 0);

noclean:

clean:

	return r;
}

int gs_ntwk_host_service_worker_packet(
	struct GsHostSurrogate *ReferenceHostSurrogate,
	struct GsWorkerRequestData *RequestSend,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	gs_connection_surrogate_id_t IdOfSend = RequestSend->mId;

	struct GsConnectionSurrogate Con = {};
	uint32_t ConIsPresent = false;

	if (!!(r = gs_connection_surrogate_map_get_try(
		ioConnectionSurrogateMap,
		IdOfSend,
		&Con,
		&ConIsPresent)))
	{
		GS_GOTO_CLEAN();
	}

	/* if a reconnection occurred, outstanding send requests would have missing send IDs */
	if (! ConIsPresent)
		GS_ERR_NO_CLEAN_L(0, W, PF, "suppressing packet for GsConnectionSurrogate [%llu]", (unsigned long long) IdOfSend);

	/* did not remove a surrogate ID soon enough? */
	/* FIXME: racing against worker, right?
			just before reconnect, worker may have queued some requests against the current host.
			after reconnect, the queued requests get processed (ex here), and the host mismatches. */
	if (Con.mHost != ReferenceHostSurrogate->mHost)
		GS_ERR_NO_CLEAN_L(0, W, PF, "suppressing packet for GsConnectionSurrogate [%llu]", (unsigned long long) IdOfSend);

	if (!!(r = gs_connection_surrogate_packet_send(
		&Con,
		RequestSend->mPacket)))
	{
		GS_GOTO_CLEAN();
	}

noclean:

clean:

	return r;
}

int gs_ntwk_host_service_sends(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	std::deque<GsWorkerRequestData> RequestSend;

	if (!!(r = gs_worker_request_dequeue_all_opt_cpp(WorkerDataSend, &RequestSend)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < RequestSend.size(); i++)
	{
		switch (RequestSend[i].type)
		{
		case GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT:
		{
			int r2 = 0;

			GS_LOG(I, S, "GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT");

			r2 = gs_helper_api_ntwk_exit(WorkerDataVecRecv);
			GS_ERR_CLEAN(r2);
		}
		break;

		case GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE:
		{
			int r2 = 0;

			GS_LOG(I, S, "GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE");

			r2 = gs_helper_api_ntwk_reconnect();
			GS_ERR_CLEAN(r2);
		}
		break;

		case GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT:
		{
			GS_LOG(I, S, "GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT");

			if (!!(r = gs_ntwk_host_service_worker_disconnect(
				HostSurrogate,
				&RequestSend[i],
				ioConnectionSurrogateMap)))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET:
		{
			GS_LOG(I, S, "GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET");

			if (!!(r = gs_ntwk_host_service_worker_packet(
				HostSurrogate,
				&RequestSend[i],
				ioConnectionSurrogateMap)))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		default:
			GS_ASSERT(0);
			break;
		}
	}

	/* absolutely no reason to flush if nothing was sent */
	// FIXME: flush only if any GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET were serviced

	if (RequestSend.size())
		enet_host_flush(HostSurrogate->mHost);

clean:

	return r;
}

int gs_ntwk_host_service_event(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	int errService,
	struct GsEventSurrogate *Event)
{
	int r = 0;

	if (errService == 0)
		{ r = 0; goto clean; } // not GS_ERR_NO_CLEAN(0) due to high call volume

	switch (Event->event.type)
	{
		
	case ENET_EVENT_TYPE_CONNECT:
	{
		GS_LOG(I, S, "ENET_EVENT_TYPE_CONNECT");

		gs_connection_surrogate_id_t AssignedId = 0;

		GsConnectionSurrogate ConnectionSurrogate = {};

		ConnectionSurrogate.mHost = HostSurrogate->mHost;
		ConnectionSurrogate.mPeer = Event->event.peer;
		ConnectionSurrogate.mIsPrincipalClientConnection = false;

		if (!!(r = gs_aux_aux_aux_connection_register_transfer_ownership(
			ConnectionSurrogate,
			ioConnectionSurrogateMap,
			&AssignedId)))
		{
			GS_GOTO_CLEAN();
		}

		GS_LOG(I, PF, "%llu connected [from %x:%u]",
			(unsigned long long)AssignedId,
			Event->event.peer->address.host,
			Event->event.peer->address.port);
	}
	break;

	case ENET_EVENT_TYPE_DISCONNECT:
	{
		gs_connection_surrogate_id_t Id = 0;

		GS_LOG(I, S, "ENET_EVENT_TYPE_DISCONNECT");

		/* get the id, then just dispose of the structure */
		{
			GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, Event->event.peer->data);
			Id = ctxstruct->m0Id;
			// FIXME: sigh raw deletion, should have been allocated at ENET_EVENT_TYPE_CONNECT
			delete ctxstruct;
		}

		if (!!(r = gs_connection_surrogate_map_erase(ioConnectionSurrogateMap, Id)))
			GS_GOTO_CLEAN();

		GS_LOG(I, PF, "%llu disconnected",
			(unsigned long long)Id);
	}
	break;

	case ENET_EVENT_TYPE_RECEIVE:
	{
		gs_connection_surrogate_id_t IdOfRecv = 0;
		GsPacketSurrogate PacketSurrogate = {};
		GsPacket *Packet = {};
		GsWorkerRequestData RequestRecv = {};

		PacketSurrogate.mPacket = Event->event.packet;

		GS_LOG(I, S, "ENET_EVENT_TYPE_RECEIVE");

		GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, Event->event.peer->data);

		IdOfRecv = ctxstruct->m0Id;

		if (!!(r = gs_packet_create(&Packet, &PacketSurrogate)))
			GS_GOTO_CLEAN();

		{
			GsFrameType FoundFrameType = {};
			uint32_t Offset = 0;

			if (!!(r = aux_frame_read_frametype(Packet->data, Packet->dataLength, Offset, &Offset, &FoundFrameType)))
				GS_GOTO_CLEAN();

			GS_LOG(I, PF, "packet received [%.*s]", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);
		}

		if (!!(r = gs_worker_request_data_type_packet_make(Packet, ctxstruct->m0Id, &RequestRecv)))
			GS_GOTO_CLEAN();

		// FIXME: arbitrarily choose worker zero
		if (!!(r = gs_worker_request_enqueue(gs_worker_data_vec_id(WorkerDataVecRecv, 0), &RequestRecv)))
			GS_GOTO_CLEAN();
	}
	break;

	}

noclean:

clean:

	return r;
}

int gs_ntwk_host_service(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk  *StoreNtwk,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	int errService = 0;

	GsEventSurrogate Event = {};

	ENetIntrNtwk Intr = {};
	Intr.base.cb_last_chance = gs_aux_aux_aux_cb_last_chance_t;
	Intr.WorkerDataSend = WorkerDataSend;

	GS_LOG(I, S, "host service interruptible");

	while (0 <= (errService = enet_host_service_interruptible(
		HostSurrogate->mHost,
		&Event.event,
		GS_TIMEOUT_1SEC,
		StoreNtwk->mIntrToken.mIntrToken,
		&Intr.base)))
	{
		if (!!(r = gs_ntwk_host_service_sends(
			WorkerDataVecRecv,
			WorkerDataSend,
			HostSurrogate,
			ioConnectionSurrogateMap)))
		{
			GS_GOTO_CLEAN();
		}

		if (!!(r = gs_ntwk_host_service_event(
			WorkerDataVecRecv,
			HostSurrogate,
			ioConnectionSurrogateMap,
			errService,
			&Event)))
		{
			GS_GOTO_CLEAN();
		}
	}

	if (errService < 0)
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int gs_ntwk_reconnecter(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk *StoreNtwk,
	struct GsExtraHostCreate *ExtraHostCreate)
{
	int r = 0;

	ClntStateReconnect StateReconnect = {};

	GsConnectionSurrogateMap *rawConnectionSurrogateMap = NULL;

	sp<GsConnectionSurrogateMap> ConnectionSurrogateMap;

	GsHostSurrogate HostSurrogate = {};

	uint32_t WantReconnect = true;

	GS_LOG(I, S, "entering reconnect-service cycle");

	if (!!(r = gs_connection_surrogate_map_create(&rawConnectionSurrogateMap)))
		GS_GOTO_CLEAN();

	GS_SP_SET_RAW_NULLING(ConnectionSurrogateMap, rawConnectionSurrogateMap, GsConnectionSurrogateMap);

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

	while (true) {

		/* NOTE: no_clean */
		if (!!(r = gs_ntwk_reconnect_expend(
			ExtraHostCreate,
			WorkerDataVecRecv,
			&StateReconnect,
			ConnectionSurrogateMap.get(),
			&HostSurrogate,
			&WantReconnect)))
		{
			GS_ERR_NO_CLEAN(r);
		}

		/* NOTE: special error handling */
		r = gs_ntwk_host_service(
			WorkerDataVecRecv,
			WorkerDataSend,
			StoreNtwk,
			&HostSurrogate,
			ConnectionSurrogateMap.get());

		if (r == GS_ERRCODE_RECONNECT) {
			GS_LOG(E, S, "ntwk reconnect attempt");
			WantReconnect = true;
			continue;
		}
		else if (r == GS_ERRCODE_EXIT) {
			GS_LOG(E, S, "ntwk exit attempt");
			GS_ERR_NO_CLEAN(0);
		}
		else if (!!r)
			GS_ERR_NO_CLEAN(r);
	}

noclean:

clean:

	return r;
}

void gs_ntwk_thread_func(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk *StoreNtwk,
	struct GsExtraHostCreate *ExtraHostCreate,
	const char *ExtraThreadName)
{
	int r = 0;

	gs_current_thread_name_set_cstr_2("ntwk_", ExtraThreadName);

	log_guard_t log(GS_LOG_GET_2("ntwk_", ExtraThreadName));

	r = gs_ntwk_reconnecter(
		WorkerDataVecRecv,
		WorkerDataSend,
		StoreNtwk,
		ExtraHostCreate);

	if (!!r) {
		//GS_ASSERT(0);
		// FIXME:
		gs_log_crash_handler_dump_global_log_list_suffix_2("ntwk_", ExtraThreadName);
	}

clean:

	GS_LOG(I, S, "thread exiting");

	if (!!(r = gs_ctrl_con_signal_exited(StoreNtwk->mCtrlCon)))
		GS_GOTO_CLEAN();

	/* NOTE: void return */
}

int gs_worker_exit(
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker)
{
	int r = 0;

	struct GsWorkerRequestData RequestExit = {};

	if (!!(r = gs_worker_request_data_type_exit_make(&RequestExit)))
		GS_ERR_CLEAN(r);

	if (!!(r = gs_worker_request_enqueue(WorkerDataSend, &RequestExit)))
		GS_ERR_CLEAN(r);

	if (!!(r = gs_ctrl_con_signal_exited(StoreWorker->mCtrlCon)))
		GS_ERR_CLEAN(r);

clean:

	return r;
}

/** we are expecting: possibly a reconnect_prepare, mandatorily a reconnect
    @return GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT type request
	@sa
	   GsWorkerRequestDataType
*/
int gs_worker_dequeue_handling_double_notify(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerRequestData *outValRequest)
{
	int r = 0;

	GsWorkerRequestData Request = {};

	/* possibly reconnect_prepare */
	if (!!(r = gs_worker_request_dequeue(WorkerDataRecv, &Request)))
		GS_GOTO_CLEAN();

	/* if indeed reconnect_prepare, skip it */
	if (Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE) {
		if (!!(r = gs_worker_request_dequeue(WorkerDataRecv, &Request)))
			GS_GOTO_CLEAN();
	}

	if (Request.type != GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT)
		GS_ERR_CLEAN_L(1, E, S, "suspected invalid double notify sequence");

	if (outValRequest)
		*outValRequest = Request;

clean:

	return r;
}

/** @param ioExtraWorker beware of leaking a pointed-to pre-existing ExtraWorker */
int gs_worker_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsExtraWorker **ioExtraWorker)
{
	int r = 0;

	GsWorkerRequestData Reconnect = {};

	GS_LOG(I, S, "reconnection wanted");

	GS_LOG(I, S, "receiving notification");

	if (!!(r = gs_worker_dequeue_handling_double_notify(WorkerDataRecv, &Reconnect)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "received notification");

	// FIXME: cb_destroy_t return value
	if (ioExtraWorker && (*ioExtraWorker))
		GS_DELETE_VF(*ioExtraWorker, cb_destroy_t);

	if (ioExtraWorker)
		*ioExtraWorker = Reconnect.mExtraWorker;

clean:

	return r;
}

void gs_worker_thread_func(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	gs_worker_id_t WorkerId,
	const char *ExtraThreadName)
{
	int r = 0;

	GsExtraWorker *ExtraWorker = NULL;

	gs_current_thread_name_set_cstr_2("work_", ExtraThreadName);

	log_guard_t log(GS_LOG_GET_2("work_", ExtraThreadName));

	GS_LOG(I, S, "entering reconnect-service cycle");

	if (!!(r = gs_worker_reconnect(
		gs_worker_data_vec_id(WorkerDataVecRecv, WorkerId),
		&ExtraWorker)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = StoreWorker->cb_crank_t(
		gs_worker_data_vec_id(WorkerDataVecRecv, WorkerId),
		WorkerDataSend,
		StoreWorker,
		&ExtraWorker,
		WorkerId)))
	{
		GS_GOTO_CLEAN();
	}

noclean:

clean:
	GS_DELETE_VF(ExtraWorker, cb_destroy_t);

	if (r == 0)
		GS_LOG(E, S, "worker implicit exit");
	else if (r == GS_ERRCODE_EXIT)
		GS_LOG(E, S, "worker explicit exit");
	else
		GS_ASSERT(0);

	if (!!(r = gs_worker_exit(WorkerDataSend, StoreWorker)))
		GS_GOTO_CLEAN();

	/* NOTE: void return */
}

int gs_net_full_create_connection(
	uint32_t ServPort,
	struct GsCtrlCon *CtrlCon, /**< owned */
	struct GsExtraHostCreate *ExtraHostCreate, /**< owned */
	struct GsStoreNtwk       *StoreNtwk, /**< owned */
	struct GsStoreWorker     *StoreWorker, /**< owned */
	struct GsFullConnection **oConnection,
	const char *ExtraThreadName)
{
	int r = 0;

	struct GsWorkerDataVec *WorkerDataVecRecv = NULL;
	struct GsWorkerData *WorkerDataSend = NULL;

	std::vector<sp<std::thread> > ThreadWorker;
	sp<std::thread> NtwkThread;

	struct GsFullConnection *Connection = NULL;

	if (!!(r = gs_worker_data_vec_create(StoreWorker->mNumWorkers, &WorkerDataVecRecv)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_data_create(&WorkerDataSend)))
		GS_GOTO_CLEAN();

	for (size_t i = 0; i < StoreWorker->mNumWorkers; i++) {
		gs_worker_id_t WorkerId = i;
		ThreadWorker.push_back(
			sp<std::thread>(
				new std::thread(
					gs_worker_thread_func,
					WorkerDataVecRecv,
					WorkerDataSend,
					StoreWorker,
					WorkerId,
					ExtraThreadName),
				gs_sp_thread_detaching_deleter));
	}

	NtwkThread = sp<std::thread>(
		new std::thread(
			gs_ntwk_thread_func,
			WorkerDataVecRecv,
			WorkerDataSend,
			StoreNtwk,
			ExtraHostCreate,
			ExtraThreadName),
		gs_sp_thread_detaching_deleter);

	if (!!(r = gs_full_connection_create(
		NtwkThread,
		ThreadWorker,
		GS_ARGOWN(&WorkerDataVecRecv, struct GsWorkerDataVec),
		GS_ARGOWN(&WorkerDataSend, struct GsWorkerData),
		GS_ARGOWN(&ExtraHostCreate, struct GsExtraHostCreate),
		GS_ARGOWN(&StoreNtwk, struct GsStoreNtwk),
		GS_ARGOWN(&StoreWorker, struct GsStoreWorker),
		GS_ARGOWN(&CtrlCon, struct GsCtrlCon),
		&Connection)))
	{
		GS_GOTO_CLEAN();
	}

	if (oConnection)
		*oConnection = Connection;

clean:
	if (!!r) {
		GS_DELETE_F(Connection, gs_full_connection_destroy);
		GS_DELETE_F(WorkerDataSend, gs_worker_data_destroy);
		GS_DELETE_F(WorkerDataVecRecv, gs_worker_data_vec_destroy);
		GS_DELETE_F(CtrlCon, gs_ctrl_con_destroy);
		GS_DELETE_VF(ExtraHostCreate, cb_destroy_t);
		GS_DELETE_VF(StoreNtwk, cb_destroy_t);
		GS_DELETE_VF(StoreWorker, cb_destroy_t);
	}

	return r;
}

int gs_full_connection_create(
	sp<std::thread> ThreadNtwk,
	std::vector<sp<std::thread> > ThreadWorker,
	struct GsWorkerDataVec *WorkerDataVecRecv, /**< owned */
	struct GsWorkerData *WorkerDataSend, /**< owned */
	struct GsExtraHostCreate *ExtraHostCreate, /**< owned */
	struct GsStoreNtwk       *StoreNtwk,      /**< owned */
	struct GsStoreWorker     *StoreWorker,    /**< owned */
	struct GsCtrlCon *CtrlCon, /**< owned */
	struct GsFullConnection **oConnection)
{
	int r = 0;

	struct GsFullConnection *Connection = new GsFullConnection();

	Connection->ThreadNtwk = ThreadNtwk;
	Connection->mThreadWorker = ThreadWorker;
	Connection->mWorkerDataVecRecv = WorkerDataVecRecv;
	Connection->mWorkerDataSend = WorkerDataSend;
	Connection->mExtraHostCreate = ExtraHostCreate;
	Connection->mStoreNtwk = StoreNtwk;
	Connection->mStoreWorker = StoreWorker;
	Connection->mCtrlCon = CtrlCon;

	if (oConnection)
		*oConnection = Connection;

clean:

	return r;
}

int gs_full_connection_destroy(struct GsFullConnection *Connection)
{
	if (!Connection)
		return 0;

	GS_DELETE_F(Connection->mWorkerDataVecRecv, gs_worker_data_vec_destroy);
	GS_DELETE_F(Connection->mWorkerDataSend, gs_worker_data_destroy);
	GS_DELETE_VF(Connection->mExtraHostCreate, cb_destroy_t);
	GS_DELETE_VF(Connection->mStoreNtwk, cb_destroy_t);
	GS_DELETE_VF(Connection->mStoreWorker, cb_destroy_t);
	GS_DELETE_F(Connection->mCtrlCon, gs_ctrl_con_destroy);

	GS_DELETE(&Connection);

	return 0;
}
