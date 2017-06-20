#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <gittest/net2_affinity.h>

bool GsPrioDataComparator::operator() (const struct GsPrioData &a, const struct GsPrioData &b) const
{
	return a.mPrio < b.mPrio;
}

int gs_affinity_queue_create(
	size_t NumWorkers,
	struct GsAffinityQueue **oAffinityQueue)
{
	int r = 0;

	struct GsAffinityQueue *AffinityQueue = new GsAffinityQueue();

	for (size_t i = 0; i < NumWorkers; i++)
		AffinityQueue->mAffinityReverse.push_back(std::set<gs_connection_surrogate_id_t>());

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
			AffinityQueue->mAffinityReverse[WorkerIdReady].insert(ConnectionId);
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

		std::set<gs_connection_surrogate_id_t> &Reverse = AffinityQueue->mAffinityReverse[WorkerId];
		for (auto it = Reverse.begin(); it != Reverse.end(); ++it)
			AffinityQueue->mAffinityMap.erase(*it);
		Reverse.clear();

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
