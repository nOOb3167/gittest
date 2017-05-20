#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <algorithm>

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

int gs_helper_api_worker_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsWorkerRequestData *oValRequestReconnectRecv)
{
	int r = 0;

	struct GsWorkerRequestData RequestPrepareSend = {};

	if (!!(r = gs_worker_request_data_type_reconnect_prepare_make(&RequestPrepareSend)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_request_enqueue(WorkerDataSend, &RequestPrepareSend)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_request_dequeue_discard_until_reconnect(WorkerDataRecv)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_dequeue_handling_double_notify(WorkerDataRecv, oValRequestReconnectRecv)))
		GS_GOTO_CLEAN();

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

int gs_helper_api_ntwk_reconnect(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	if (!!(r = ExtraHostCreate->cb_destroy_host_t(
		ExtraHostCreate,
		ioHostSurrogate)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_helper_api_ntwk_extra_host_create_and_notify(
		ExtraHostCreate,
		WorkerDataVecRecv,
		ioHostSurrogate,
		ioConnectionSurrogateMap)))
	{
		GS_GOTO_CLEAN();
	}

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
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
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
			GS_ARGOWN(ExtraWorker.data() + i))))
		{
			GS_GOTO_CLEAN();
		}

clean:
	if (!!r) {
		for (size_t i = 0; i < ExtraWorker.size(); i++)
			GS_DELETE_VF(&ExtraWorker[i], cb_destroy_t);
	}

	return r;
}

int gs_packet_with_offset_get_veclen(
	struct GsPacketWithOffset *PacketWithOffset,
	uint32_t *oVecLen)
{
	GS_ASSERT(
		GS_FRAME_SIZE_LEN == sizeof(uint32_t) &&
		PacketWithOffset->mOffsetObject >= PacketWithOffset->mOffsetSize &&
		(PacketWithOffset->mOffsetObject - PacketWithOffset->mOffsetSize) % GS_FRAME_SIZE_LEN == 0);
	return (PacketWithOffset->mOffsetObject - PacketWithOffset->mOffsetSize) / GS_FRAME_SIZE_LEN;
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
	GS_DELETE(&CtrlCon, GsCtrlCon);
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

int gs_affinity_queue_create(
	size_t NumWorkers,
	struct GsAffinityQueue **oAffinityQueue)
{
	int r = 0;

	struct GsAffinityQueue *AffinityQueue = new GsAffinityQueue();

	AffinityQueue->mAffinityInProgress.resize(NumWorkers, GS_AFFINITY_IN_PROGRESS_NONE);

	for (size_t i = 0; i < NumWorkers; i++) {
		struct GsPrioData PrioData = {};
		PrioData.mPrio = 0;
		PrioData.mWorkerId = i;
		AffinityQueue->mPrioSet.insert(PrioData);
	}

	for (gs_prio_set_t::iterator it = AffinityQueue->mPrioSet.begin(); it != AffinityQueue->mPrioSet.end(); it++)
		AffinityQueue->mPrioVec.push_back(it);

	if (oAffinityQueue)
		*oAffinityQueue = AffinityQueue;

clean:

	return r;
}

int gs_affinity_queue_destroy(struct GsAffinityQueue *AffinityQueue)
{
	GS_DELETE(&AffinityQueue, GsAffinityQueue);
	return 0;
}

/**
    NOTE: TAKES HOLD OF MULTIPLE LOCKS
	  lock order: AffinityQueue lock, WorkerData lock (computed)
*/
int gs_affinity_queue_worker_acquire_ready_and_enqueue(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerDataVec *WorkerDataVec,
	struct GsWorkerRequestData *valRequestData,
	gs_connection_surrogate_id_t ConnectionId)
{
	int r = 0;

	gs_worker_id_t WorkerIdReady = 0;

	{
		std::unique_lock<std::mutex> lock(AffinityQueue->mMutexData);

		auto it = AffinityQueue->mAffinityMap.find(ConnectionId);

		if (it != AffinityQueue->mAffinityMap.end()) {
			/* - acquire to worker currently already holding lease for ConnectionId */
			WorkerIdReady = it->second;
			/* - priority inert - no worker handles more or fewer connids */
			/* - connid registration inert - should have already been associated with affinity map */
		}
		else {
			/* - acquire to worker with lowest prio aka handling the fewest connids */
			/* - priority raised - WorkerIdReady handles one more connid */
			if (!!(r = gs_affinity_queue_prio_acquire_lowest_and_increment_nolock(
				AffinityQueue,
				&lock,
				&WorkerIdReady)))
			{
				GS_GOTO_CLEAN();
			}
			/* - connid registration performed - WorkerIdReady now handling ConnectionId */
			AffinityQueue->mAffinityMap[ConnectionId] = WorkerIdReady;
		}

		if (!!(r = gs_worker_request_enqueue(gs_worker_data_vec_id(WorkerDataVec, WorkerIdReady), valRequestData)))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}

/**
    NOTE: TAKES HOLD OF MULTIPLE LOCKS
	  - LockAffinityQueue locked by parent
	  - multiple WorkerData locks (computed)
	  in above lock order
*/
int gs_affinity_queue_worker_completed_all_requests_somelock(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t WorkerId,
	std::unique_lock<std::mutex> *LockAffinityQueue)
{
	/* NOTE: it might be sufficient to perform lease releasing (or work stealing)
	*        once all requests have been completed ie recv queue empty.
	*    could a connection lease be released as soon as all requests for that specific connection have been completed?
	*    yes, but would required tracking (number of connid X requests inside recv queue).
	*    scenario:
	*    (- having work stealing on completion of all requests (@LATE) AND
	*     - work stealing as soon as requests for specific connection complete (@EARLY))
	*      queue A, servicing connections C, D, E.
	*      all C requests are processed, D and E remain.
	*      imagine work stealing / lease releasing is triggered, queue B stealing D.
	*      observe that queue A keeps processing at full capability regardless stealing having occurred.
	*        if queue B was nonempty: queue B keeps processing at full capacity
	*        if queue B was empty: queue B processing capacity improves.
	*                              however! the assumption we are attempting to test is that work stealing
	*                              implemented to trigger @LATE
	*                              (aka once queue is empty) is sufficient. thus B would have stolen
	*                              some (although not necessarily A's D) work and kept processing capacity
	*                              equal to the @EARLY case.
	*     conclusion: @EARLY is not necessary, @LATE is sufficient. */

	int r = 0;

	{
		GS_ASSERT(LockAffinityQueue->owns_lock());

		/* release all connection leases held by WorkerId */

		for (auto it = AffinityQueue->mAffinityMap.begin(); it != AffinityQueue->mAffinityMap.end(); /* empty */) {
			if (it->second == WorkerId)
				AffinityQueue->mAffinityMap.erase(it++);
			else
				it++;
		}

		/* update prio wrt number of connection leases (ex zero) */

		if (!!(r = gs_affinity_queue_prio_zero_nolock(
			AffinityQueue,
			WorkerId,
			LockAffinityQueue)))
		{
			GS_GOTO_CLEAN();
		}

		{
			GS_ASSERT(! AffinityQueue->mPrioSet.empty());
			struct GsPrioData PrioHighest = *(--AffinityQueue->mPrioSet.end());
			if (PrioHighest.mPrio > 1) {
				/* NOTE: even though prio is greater than one, we may still
				         end up stealing nothing (in current design prio
						 can end up overestimated) */
				// FIXME: is this correct? or need to take AffinityQueue lock simultaneously with these
				std::unique_lock<std::mutex> DoubleLock[2];
				const gs_worker_id_t DstWorkerId = WorkerId;
				const gs_worker_id_t SrcWorkerId = PrioHighest.mWorkerId;
				if (!!(r = gs_affinity_queue_helper_worker_double_lock(
					WorkerDataVec,
					DstWorkerId,
					SrcWorkerId,
					DoubleLock)))
				{
					GS_GOTO_CLEAN();
				}
				const gs_connection_surrogate_id_t SrcInProgress = AffinityQueue->mAffinityInProgress.at(SrcWorkerId);
				/* do remember that InProgress can potentially be GS_AFFINITY_IN_PROGRESS_NONE */
				if (!!(r = gs_worker_request_dequeue_steal_except_nolock(
					AffinityQueue,
					WorkerDataVec,
					DstWorkerId,
					SrcWorkerId,
					SrcInProgress,
					LockAffinityQueue,
					DoubleLock)))
				{
					GS_GOTO_CLEAN();
				}
			}
		}
	}

clean:

	return r;
}

int gs_affinity_queue_request_dequeue_and_acquire(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t WorkerId,
	uint32_t TimeoutMs,
	struct GsWorkerRequestData *oValRequest,
	struct GsAffinityToken *ioAffinityToken)
{
	int r = 0;

	struct GsWorkerData *WorkerData = gs_worker_data_vec_id(WorkerDataVec, WorkerId);

	struct GsWorkerRequestData Request = {};

	bool QueueIsEmpty = false;

	// FIXME: TODO: maybe make a _nolock version, and call it in scope of this function's lock
	if (!!(r = gs_affinity_token_release(ioAffinityToken)))
		GS_GOTO_CLEAN();

	{
		/* besides WorkerDataVec[WorkerId], lock used for mAffinityInProgress[WorkerId] */
		std::unique_lock<std::mutex> lock(*WorkerData->mWorkerDataMutex);

		if (!!(r = gs_worker_request_dequeue_timeout_noprepare_nolock(
			WorkerData,
			&Request,
			TimeoutMs,
			&lock)))
		{
			GS_GOTO_CLEAN();
		}

		QueueIsEmpty = gs_worker_request_isempty_nolock(WorkerData, &lock);

		if (!!(r = gs_affinity_token_acquire_raw_nolock(
			ioAffinityToken,
			WorkerId,
			AffinityQueue,
			WorkerData,
			Request)))
		{
			GS_GOTO_CLEAN();
		}
	}

	if (QueueIsEmpty) {
		std::unique_lock<std::mutex> lock(AffinityQueue->mMutexData);
		if (!!(r = gs_affinity_queue_worker_completed_all_requests_somelock(
			AffinityQueue,
			WorkerDataVec,
			WorkerId,
			&lock)))
		{
			GS_GOTO_CLEAN();
		}
	}

	if (oValRequest)
		*oValRequest = Request;

clean:

	return r;
}

int gs_affinity_queue_prio_zero_nolock(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t WorkerId,
	std::unique_lock<std::mutex> *Lock)
{
	int r = 0;

	{
		GS_ASSERT(Lock->owns_lock());

		gs_prio_set_t::iterator it;
		struct GsPrioData PrioData = {};

		it = AffinityQueue->mPrioVec.at(WorkerId);
		PrioData = *it;
		PrioData.mPrio = 0;
		AffinityQueue->mPrioSet.erase(it);
		AffinityQueue->mPrioVec.at(WorkerId) = AffinityQueue->mPrioSet.insert(PrioData);
	}

clean:

	return r;
}

int gs_affinity_queue_prio_increment_nolock(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t WorkerId,
	std::unique_lock<std::mutex> *Lock)
{
	int r = 0;

	{
		GS_ASSERT(Lock->owns_lock());

		gs_prio_set_t::iterator it;
		struct GsPrioData PrioData = {};
		
		it = AffinityQueue->mPrioVec.at(WorkerId);
		PrioData = *it;
		PrioData.mPrio += 1;
		AffinityQueue->mPrioSet.erase(it);
		AffinityQueue->mPrioVec.at(WorkerId) = AffinityQueue->mPrioSet.insert(PrioData);
	}

clean:

	return r;
}

int gs_affinity_queue_prio_decrement_nolock(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t WorkerId,
	std::unique_lock<std::mutex> *Lock)
{
	int r = 0;

	{
		GS_ASSERT(Lock->owns_lock());

		gs_prio_set_t::iterator it;
		struct GsPrioData PrioData = {};
		
		it = AffinityQueue->mPrioVec.at(WorkerId);
		PrioData = *it;
		PrioData.mPrio -= 1;
		AffinityQueue->mPrioSet.erase(it);
		AffinityQueue->mPrioVec.at(WorkerId) = AffinityQueue->mPrioSet.insert(PrioData);
	}

clean:

	return r;
}

int gs_affinity_queue_prio_acquire_lowest_and_increment_nolock(
	struct GsAffinityQueue *AffinityQueue,
	std::unique_lock<std::mutex> *Lock,
	gs_worker_id_t *oWorkerLowestPrioId)
{
	int r = 0;

	gs_worker_id_t WorkerLowestPrioId = 0;

	{
		GS_ASSERT(Lock->owns_lock());

		GS_ASSERT(! AffinityQueue->mPrioSet.empty());

		WorkerLowestPrioId = AffinityQueue->mPrioSet.begin()->mWorkerId;

		if (!!(r = gs_affinity_queue_prio_increment_nolock(
			AffinityQueue,
			WorkerLowestPrioId,
			Lock)))
		{
			GS_GOTO_CLEAN();
		}
	}

	if (oWorkerLowestPrioId)
		*oWorkerLowestPrioId = WorkerLowestPrioId;

clean:

	return r;
}

/** Acquire in consistent order (ascending mutex pointer)
	for deadlock avoidance purposes.
*/
int gs_affinity_queue_helper_worker_double_lock(
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t DstWorkerId,
	gs_worker_id_t SrcWorkerId,
	std::unique_lock<std::mutex> ioDoubleLock[2])
{
	struct GsWorkerData *W0 = gs_worker_data_vec_id(WorkerDataVec, DstWorkerId);
	struct GsWorkerData *W1 = gs_worker_data_vec_id(WorkerDataVec, SrcWorkerId);
	ioDoubleLock[0] = std::move(std::unique_lock<std::mutex>(*GS_MIN(W0->mWorkerDataMutex.get(), W1->mWorkerDataMutex.get())));
	ioDoubleLock[1] = std::move(std::unique_lock<std::mutex>(*GS_MAX(W0->mWorkerDataMutex.get(), W1->mWorkerDataMutex.get())));
	return 0;
}

int gs_affinity_token_acquire_raw_nolock(
	struct GsAffinityToken *ioAffinityToken,
	gs_worker_id_t WorkerId,
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerData *WorkerData,
	struct GsWorkerRequestData valRequest)
{
	int r = 0;

	GS_ASSERT(! ioAffinityToken->mIsAcquired);

	/* only PACKET type requests need to be properly acquired */
	if (valRequest.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET) {
		AffinityQueue->mAffinityInProgress[WorkerId] = valRequest.mId;

		ioAffinityToken->mIsAcquired = 1;
		ioAffinityToken->mExpectedWorker = WorkerId;
		ioAffinityToken->mAffinityQueue = AffinityQueue;
		ioAffinityToken->mWorkerData = WorkerData;
		ioAffinityToken->mValRequest = valRequest;
	}

clean:

	return r;
}

int gs_affinity_token_release(
	struct GsAffinityToken *ioAffinityToken)
{
	int r = 0;

	if (ioAffinityToken->mIsAcquired) {
		/* besides WorkerData, lock used for mAffinityInProgress[WorkerId] */
		std::unique_lock<std::mutex> lock(*ioAffinityToken->mWorkerData->mWorkerDataMutex);

		ioAffinityToken->mAffinityQueue->mAffinityInProgress[ioAffinityToken->mExpectedWorker] = GS_AFFINITY_IN_PROGRESS_NONE;
	}

	ioAffinityToken->mIsAcquired = 0;

clean:

	return r;
}

int gs_ntwk_reconnect_expend(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct ClntStateReconnect *ioStateReconnect,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	struct GsHostSurrogate *ioHostSurrogate)
{
	int r = 0;

	struct GsExtraWorker *ExtraWorker = NULL;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

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
		ioHostSurrogate,
		ioConnectionSurrogateMap)))
	{
		GS_GOTO_CLEAN();
	}

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
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
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

			r2 = gs_helper_api_ntwk_reconnect(
				WorkerDataVecRecv,
				ExtraHostCreate,
				ioHostSurrogate,
				ioConnectionSurrogateMap);
			GS_ERR_CLEAN(r2);
		}
		break;

		case GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT:
		{
			GS_LOG(I, S, "GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT");

			if (!!(r = gs_ntwk_host_service_worker_disconnect(
				ioHostSurrogate,
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
				ioHostSurrogate,
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
		enet_host_flush(ioHostSurrogate->mHost);

clean:

	return r;
}

int gs_ntwk_host_service_event(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsAffinityQueue *AffinityQueue,
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

		/* NOTE: raw new, routinely deleted at ENET_EVENT_TYPE_DISCONNECT */
		struct GsBypartCbDataGsConnectionSurrogateId *ctxstruct = new GsBypartCbDataGsConnectionSurrogateId();

		struct GsPeerSurrogate PeerSurrogate = {};
		struct GsConnectionSurrogate ConnectionSurrogate = {};

		PeerSurrogate.mPeer = Event->event.peer;

		if (!!(r = gs_connection_surrogate_init(HostSurrogate, &PeerSurrogate, false, &ConnectionSurrogate)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_connection_surrogate_map_register_bond_transfer_ownership(
			ConnectionSurrogate,
			GS_ARGOWN(&ctxstruct),
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
		GS_LOG(I, S, "ENET_EVENT_TYPE_DISCONNECT");

		GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, Event->event.peer->data);

		GS_LOG(I, PF, "%llu disconnected",
			(unsigned long long)ctxstruct->m0Id);

		if (!!(r = gs_connection_surrogate_map_erase(ioConnectionSurrogateMap, ctxstruct->m0Id)))
			GS_GOTO_CLEAN();

		/* NOTE: raw delete, routinely new'd at ENET_EVENT_TYPE_CONNECT */
		GS_DELETE(&ctxstruct, GsBypartCbDataGsConnectionSurrogateId);
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

		if (!!(r = gs_worker_request_data_type_packet_make(Packet, IdOfRecv, &RequestRecv)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_affinity_queue_worker_acquire_ready_and_enqueue(
			AffinityQueue,
			WorkerDataVecRecv,
			&RequestRecv,
			IdOfRecv)))
		{
			GS_GOTO_CLEAN();
		}
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
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
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
		ioHostSurrogate->mHost,
		&Event.event,
		GS_TIMEOUT_1SEC,
		StoreNtwk->mIntrToken.mIntrToken,
		&Intr.base)))
	{
		if (!!(r = gs_ntwk_host_service_sends(
			WorkerDataVecRecv,
			WorkerDataSend,
			ExtraHostCreate,
			ioHostSurrogate,
			ioConnectionSurrogateMap)))
		{
			GS_GOTO_CLEAN();
		}

		if (!!(r = gs_ntwk_host_service_event(
			WorkerDataVecRecv,
			StoreNtwk->mAffinityQueue,
			ioHostSurrogate,
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

void gs_ntwk_thread_func(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk *StoreNtwk,
	struct GsExtraHostCreate *ExtraHostCreate,
	const char *ExtraThreadName)
{
	int r = 0;

	struct GsHostSurrogate HostSurrogate = {};

	gs_current_thread_name_set_cstr_2("ntwk_", ExtraThreadName);

	log_guard_t log(GS_LOG_GET_2("ntwk_", ExtraThreadName));

	GS_LOG(I, S, "entering reconnect-service cycle");

	if (!!(r = gs_ntwk_reconnect_expend(
		ExtraHostCreate,
		WorkerDataVecRecv,
		&StoreNtwk->mStateReconnect,
		StoreNtwk->mConnectionSurrogateMap,
		&HostSurrogate)))
	{
		GS_GOTO_CLEAN();
	}

	do {
		r = gs_ntwk_host_service(
			WorkerDataVecRecv,
			WorkerDataSend,
			StoreNtwk,
			ExtraHostCreate,
			&HostSurrogate,
			StoreNtwk->mConnectionSurrogateMap);
	} while (r == GS_ERRCODE_RECONNECT);
	/* NOTE: other return codes handled by fallthrough */

clean:
	if (r == 0)
		GS_LOG(E, S, "ntwk implicit exit");
	else if (r == GS_ERRCODE_EXIT)
		GS_LOG(E, S, "ntwk explicit exit");
	else
		GS_ASSERT(0);

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

	if (!!(r = gs_extra_worker_replace(ioExtraWorker, Reconnect.mExtraWorker)))
		GS_GOTO_CLEAN();

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

	struct GsCrankData *CrankData = NULL;

	struct GsExtraWorker *ExtraWorker = NULL;

	gs_current_thread_name_set_cstr_2("work_", ExtraThreadName);

	log_guard_t log(GS_LOG_GET_2("work_", ExtraThreadName));

	GS_LOG(I, S, "entering reconnect-service cycle");

	if (!!(r = gs_worker_reconnect(
		gs_worker_data_vec_id(WorkerDataVecRecv, WorkerId),
		&ExtraWorker)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_crank_data_create(
		WorkerDataVecRecv,
		WorkerDataSend,
		StoreWorker,
		WorkerId,
		ExtraWorker,
		&CrankData)))
	{
		GS_GOTO_CLEAN();
	}

	do {
		r = StoreWorker->cb_crank_t(CrankData);
	} while (r == GS_ERRCODE_RECONNECT);
	/* NOTE: other return codes handled by fallthrough */

clean:
	GS_DELETE_F(&CrankData, gs_crank_data_destroy);
	GS_DELETE_VF(&ExtraWorker, cb_destroy_t);

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
	struct GsExtraHostCreate *ExtraHostCreate, /**< owned */
	struct GsStoreNtwk       *StoreNtwk, /**< owned */
	struct GsStoreWorker     *StoreWorker, /**< owned */
	struct GsFullConnectionCommonData *ConnectionCommon, /**< owned */
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
		GS_ARGOWN(&WorkerDataVecRecv),
		GS_ARGOWN(&WorkerDataSend),
		GS_ARGOWN(&ExtraHostCreate),
		GS_ARGOWN(&StoreNtwk),
		GS_ARGOWN(&StoreWorker),
		GS_ARGOWN(&ConnectionCommon->mCtrlCon),
		GS_ARGOWN(&ConnectionCommon->mAffinityQueue),
		&Connection)))
	{
		GS_GOTO_CLEAN();
	}

	if (oConnection)
		*oConnection = Connection;

clean:
	if (!!r) {
		GS_DELETE_F(&Connection, gs_full_connection_destroy);
		GS_DELETE_F(&WorkerDataSend, gs_worker_data_destroy);
		GS_DELETE_F(&WorkerDataVecRecv, gs_worker_data_vec_destroy);
		GS_DELETE_VF(&ExtraHostCreate, cb_destroy_t);
		GS_DELETE_VF(&StoreNtwk, cb_destroy_t);
		GS_DELETE_VF(&StoreWorker, cb_destroy_t);
		GS_DELETE_F(&ConnectionCommon, gs_full_connection_common_data_destroy);
	}

	return r;
}

int gs_full_connection_common_data_create(
	uint32_t NumWorkers,
	struct GsFullConnectionCommonData **oConnectionCommon)
{
	int r = 0;

	struct GsFullConnectionCommonData *ConnectionCommon = new GsFullConnectionCommonData();

	struct ENetIntrTokenCreateFlags *IntrTokenFlags = NULL;

	if (!(IntrTokenFlags = enet_intr_token_create_flags_create(ENET_INTR_DATA_TYPE_NONE)))
		GS_GOTO_CLEAN();

	if (!(ConnectionCommon->mIntrToken.mIntrToken = enet_intr_token_create(IntrTokenFlags)))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_ctrl_con_create(1, NumWorkers, &ConnectionCommon->mCtrlCon)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_affinity_queue_create(NumWorkers, &ConnectionCommon->mAffinityQueue)))
		GS_GOTO_CLEAN();

	if (oConnectionCommon)
		*oConnectionCommon = ConnectionCommon;

clean:
	if (!!r) {
		GS_DELETE_F(&ConnectionCommon, gs_full_connection_common_data_destroy);
	}

	return r;
}

int gs_full_connection_common_data_destroy(struct GsFullConnectionCommonData *ioData)
{
	// FIXME: currently no way to destroy token?
	//   if (ioData->mIntrToken.mIntrToken)
	GS_DELETE_F(&ioData->mCtrlCon, gs_ctrl_con_destroy);
	GS_DELETE_F(&ioData->mAffinityQueue, gs_affinity_queue_destroy);
	return 0;
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
	struct GsAffinityQueue *AffinityQueue, /**< owned */
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
	Connection->mAffinityQueue = AffinityQueue;

	if (oConnection)
		*oConnection = Connection;

clean:

	return r;
}

int gs_full_connection_destroy(struct GsFullConnection *Connection)
{
	if (!Connection)
		return 0;

	GS_DELETE_F(&Connection->mWorkerDataVecRecv, gs_worker_data_vec_destroy);
	GS_DELETE_F(&Connection->mWorkerDataSend, gs_worker_data_destroy);
	GS_DELETE_VF(&Connection->mExtraHostCreate, cb_destroy_t);
	GS_DELETE_VF(&Connection->mStoreNtwk, cb_destroy_t);
	GS_DELETE_VF(&Connection->mStoreWorker, cb_destroy_t);
	GS_DELETE_F(&Connection->mCtrlCon, gs_ctrl_con_destroy);
	GS_DELETE_F(&Connection->mAffinityQueue, gs_affinity_queue_destroy);

	GS_DELETE(&Connection, GsFullConnection);

	return 0;
}
