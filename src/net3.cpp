#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>
#include <deque>

#include <gittest/misc.h>
#include <gittest/net2.h>

/*
empty -> nonempty : cond signal

maybe GsWorkerData3 mIsDisabled field or such will be needed

if connid already present
  put
else
  create connid queue
  hoist connid queue to a workerdata

if queue is stolen but the old worker triggers a wait on the queue
  is that even possible? can it be avoided? will I have to check ownership on wakeup?

have access to an accurate number of nonempty queues per worker in this design

** consider: BEFORE taking out the last request, worker conditionally puts itself on a 'need-job' queue?
   if the worker puts itself on a 'need-job' queue AFTER taking out last request, race condition with NTWK prioritising 'need-job' workers
     when dealing out requests
the 'need-job' queue does not need to be a separate data structure
  use 'prio' zero aka lowest number of nonempty queues

maybe do round-robin instead of lowest
  no real point in lowest, right?
should be able to get away with array instead of map as well for mConnidHandled
  or use both / or push mConnId into queue
  ** or better: array of (mConnId,mQueue) pair
removal and steal are 'rare' enough events for traversing array to be okay
*/

struct GsConnidQueue;
struct GsConnidQueueCond;
struct GsWorkerData3;

typedef ::std::map<gs_connection_surrogate_id_t, sp<GsConnidQueue> > gs_connid_queue_map_t;
typedef ::std::vector<sp<GsConnidQueue> > gs_connid_queue_vec_t;

struct GsQueuer
{
	std::vector<sp<GsWorkerData3> > mWorkers;
	gs_connid_queue_map_t mConnidQueueMap;

	GsQueuer(uint32_t NumWorkers)
		: mWorkers(NumWorkers),
		  mConnidQueueMap()
	{}
};

struct GsConnidQueue
{
	gs_connection_surrogate_id_t mConnId;
	std::deque<struct GsWorkerRequestData> mQueue;

	sp<GsConnidQueueCond> mCondShared;

	GsConnidQueue(gs_connection_surrogate_id_t ConnId, const sp<GsConnidQueueCond> &CondShared)
		: mConnId(),
		  mQueue(),
		  mCondShared(CondShared)
	{}
};

struct GsConnidQueueCond
{
	std::mutex mMutex;
	std::condition_variable mCond;
	/* maybe there should be a mWakeupRequested field
		    wakeup operations:
			request enqueued or
			ntwk barrier issued - ntwk queues nothing more, waits for all empty, then signals barrier-issued */
	/* while I don't like having this (mConnidHandledNonemptyApprox) field here instead of in GsWorkerData3,
	   that structure pairs with GsConnidQueueCond 1-to-1. since GsWorkerData3 links its
	   GsConnidQueueCond, it may always reach it for updates. */
	std::atomic<uint32_t> mConnidHandledNonemptyApprox;  /**< mutable, special */

	GsConnidQueueCond()
		: mMutex(),
		  mCond(),
		  mConnidHandledNonemptyApprox()
	{}
};

struct GsWorkerData3
{
	gs_connid_queue_vec_t mConnidHandled;
	size_t mRoundRobinCnt;
	sp<GsConnidQueueCond> mCondShared;

	GsWorkerData3()
		: mConnidHandled(),
		  mRoundRobinCnt(),
		  mCondShared()
	{}
};

int gs_queuer_enqueue_type_packet(
	struct GsQueuer *Queuer,
	struct GsWorkerRequestData Request);
int gs_queuer_dequeue(
	struct GsQueuer *Queuer,
	gs_worker_id_t WorkerId,
	struct GsWorkerRequestData *oRequest);
int gs_queuer_nonempty_approx_lowest(
	struct GsQueuer *Queuer,
	gs_worker_id_t *oWorkerIdLowestPrio);
int gs_connid_queue_enqueue(
	struct GsConnidQueue *Queue,
	struct GsWorkerRequestData Request,
	int *oWasEmpty);
int gs_connid_queue_dequeue_try(
	struct GsConnidQueue *Queue,
	struct GsWorkerRequestData *oRequest,
	int    *oWasDequeued,
	size_t *oNewSize);
int gs_connid_queue_cond_notify_nonempty(
	struct GsConnidQueueCond *QueueCond);
int gs_connid_queue_cond_wait_nolock(
	struct GsConnidQueueCond *QueueCond,
	std::unique_lock<std::mutex> *Lock);
int gs_connid_queue_cond_nonempty_approx_inc(
	struct GsConnidQueueCond *QueueCond);
int gs_connid_queue_cond_nonempty_approx_dec(
	struct GsConnidQueueCond *QueueCond);
int gs_connid_queue_cond_nonempty_approx_set(
	struct GsConnidQueueCond *QueueCond,
	uint32_t Val);
int gs_worker_data_3_add_handled(
	struct GsWorkerData3 *WorkerData,
	gs_connection_surrogate_id_t ConnId,
	const sp<GsConnidQueue> &Queue);
int gs_worker_data_3_connid_queue_dequeue_try(
	struct GsWorkerData3 *WorkerData,
	struct GsWorkerRequestData *oRequest,
	int *oWasDequeued,
	size_t *oNewSize);
int gs_worker_data_3_round_robin_advance(
	struct GsWorkerData3 *WorkerData);
int gs_worker_data_3_nonempty_approx_refresh(
	struct GsWorkerData3 *WorkerData);

/** Threading: Designed to be called from a single thread */
int gs_queuer_enqueue_type_packet(
	struct GsQueuer *Queuer,
	struct GsWorkerRequestData Request)
{
	int r = 0;

	GS_ASSERT(Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET);
	
	gs_connid_queue_map_t::iterator it = Queuer->mConnidQueueMap.find(Request.mId);

	if (it != Queuer->mConnidQueueMap.end()) {
		/* = packet from recurring connection = */
		sp<GsConnidQueue> &Queue = it->second;
		int WasEmpty = 0;
		std::unique_lock<std::mutex> Lock(Queue->mCondShared->mMutex);
		if (!!(r = gs_connid_queue_enqueue(Queue.get(), Request, &WasEmpty)))
			GS_GOTO_CLEAN();
		if (!!(r = gs_connid_queue_cond_nonempty_approx_inc(Queue->mCondShared.get())))
			GS_GOTO_CLEAN();
		if (WasEmpty)
			if (!!(r = gs_connid_queue_cond_notify_nonempty(Queue->mCondShared.get())))
				GS_GOTO_CLEAN();
	}
	else {
		/* = packet from newly seen connection = */
		gs_worker_id_t WorkerId = 0;
		struct GsWorkerData3 *WorkerData = NULL;
		sp<GsConnidQueue> Queue;
		int WasEmpty = 0;
		/* == decide initially owning worker == */
		/* === it is the worker handling the least nonempty queues === */
		/* === NOTE: atomics and approx, thus no lock is necessary */
		if (!!(r = gs_queuer_nonempty_approx_lowest(Queuer, &WorkerId)))
			GS_GOTO_CLEAN();
		/* == create queue == */
		WorkerData = Queuer->mWorkers[WorkerId].get();
		Queue = sp<GsConnidQueue>(new GsConnidQueue(Request.mId, WorkerData->mCondShared));
		/* == setup on the GsWorkerData3 side == */
		std::unique_lock<std::mutex> Lock(Queue->mCondShared->mMutex);
		if (!!(r = gs_worker_data_3_add_handled(WorkerData, Request.mId, Queue)))
			GS_GOTO_CLEAN();
		if (!!(r = gs_connid_queue_enqueue(Queue.get(), Request, &WasEmpty)))
			GS_GOTO_CLEAN();
		if (!!(r = gs_connid_queue_cond_nonempty_approx_inc(Queue->mCondShared.get())))
			GS_GOTO_CLEAN();
		if (WasEmpty)
			if (!!(r = gs_connid_queue_cond_notify_nonempty(Queue->mCondShared.get())))
				GS_GOTO_CLEAN();
		/* == setup on the queuer side == */
		Queuer->mConnidQueueMap[Request.mId] = Queue;
	}

clean:

	return r;
}

/** Threading: Thread-aware */
int gs_queuer_dequeue(
	struct GsQueuer *Queuer,
	gs_worker_id_t WorkerId,
	struct GsWorkerRequestData *oRequest)
{
	int r = 0;

	int WasDequeued = 0;
	struct GsWorkerRequestData Request = {};

	struct GsWorkerData3 *WorkerData = Queuer->mWorkers[WorkerId].get();

	std::unique_lock<std::mutex> Lock(WorkerData->mCondShared->mMutex);

	while (! WasDequeued) {
		size_t NewSize = 0;
		if (!!(r = gs_connid_queue_cond_wait_nolock(WorkerData->mCondShared.get(), &Lock)))
			GS_GOTO_CLEAN();
		if (!!(r = gs_worker_data_3_connid_queue_dequeue_try(WorkerData, oRequest, &WasDequeued, &NewSize)))
			GS_GOTO_CLEAN();
		if (WasDequeued && NewSize == 0)
			if (!!(r = gs_connid_queue_cond_nonempty_approx_dec(WorkerData->mCondShared.get())))
				GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_queuer_nonempty_approx_lowest(
	struct GsQueuer *Queuer,
	gs_worker_id_t *oWorkerIdLowestPrio)
{
	int r = 0;

	gs_worker_id_t WorkerIdLowestPrio = 0;
	uint32_t PrioLowest = UINT32_MAX;

	for (size_t i = 0; i < Queuer->mWorkers.size(); i++) {
		uint32_t PrioWorker = Queuer->mWorkers[i]->mCondShared->mConnidHandledNonemptyApprox;
		if (PrioWorker < PrioLowest) {
			PrioLowest = PrioWorker;
			WorkerIdLowestPrio = i;
		}
	}

	if (oWorkerIdLowestPrio)
		*oWorkerIdLowestPrio = WorkerIdLowestPrio;

clean:

	return r;
}

int gs_connid_queue_enqueue(
	struct GsConnidQueue *Queue,
	struct GsWorkerRequestData Request,
	int *oWasEmpty)
{
	int r = 0;

	int WasEmpty = Queue->mQueue.empty();

	Queue->mQueue.push_back(Request);

	if (oWasEmpty)
		*oWasEmpty = WasEmpty;

clean:

	return r;
}

int gs_connid_queue_dequeue_try(
	struct GsConnidQueue *Queue,
	struct GsWorkerRequestData *oRequest,
	int    *oWasDequeued,
	size_t *oNewSize)
{
	size_t Size = Queue->mQueue.size();
	if ((*oWasDequeued = (Size != 0))) {
		*oRequest = Queue->mQueue.front();
		Queue->mQueue.pop_front();
		Size--;
	}
	*oNewSize = Size;
	return 0;
}

int gs_connid_queue_cond_notify_nonempty(
	struct GsConnidQueueCond *QueueCond)
{
	int r = 0;

	// FIXME: would notify_one suffice?
	QueueCond->mCond.notify_all();

clean:

	return r;
}

/**
   NOTE/WARNING: the condition is NOT checked such as in the normal condition_variable usage pattern:
       while(!Pred) { Cond.wait() }
     thus spurious wakeups are possible. the condition check must thus be handled somewhere later.
*/
int gs_connid_queue_cond_wait_nolock(
	struct GsConnidQueueCond *QueueCond,
	std::unique_lock<std::mutex> *Lock)
{
	int r = 0;

	GS_ASSERT(Lock->owns_lock());

	QueueCond->mCond.wait(*Lock);

clean:

	return r;
}

int gs_connid_queue_cond_nonempty_approx_inc(
	struct GsConnidQueueCond *QueueCond)
{
	QueueCond->mConnidHandledNonemptyApprox.fetch_add(1);
	return 0;
}

int gs_connid_queue_cond_nonempty_approx_dec(
	struct GsConnidQueueCond *QueueCond)
{
	QueueCond->mConnidHandledNonemptyApprox.fetch_sub(1);
	return 0;
}

int gs_connid_queue_cond_nonempty_approx_set(
	struct GsConnidQueueCond *QueueCond,
	uint32_t Val)
{
	QueueCond->mConnidHandledNonemptyApprox = Val;
	return 0;
}

int gs_worker_data_3_add_handled(
	struct GsWorkerData3 *WorkerData,
	gs_connection_surrogate_id_t ConnId,
	const sp<GsConnidQueue> &Queue)
{
	WorkerData->mConnidHandled.push_back(Queue);
	return 0;
}

int gs_worker_data_3_connid_queue_dequeue_try(
	struct GsWorkerData3 *WorkerData,
	struct GsWorkerRequestData *oRequest,
	int *oWasDequeued,
	size_t *oNewSize)
{
	int r = 0;

	int WasDequeued = 0;
	size_t NewSize = 0;

	for (size_t i = 0; i < WorkerData->mConnidHandled.size(); ++i) {
		if (!!(r = gs_worker_data_3_round_robin_advance(WorkerData)))
			GS_GOTO_CLEAN();
		if (!!(r = gs_connid_queue_dequeue_try(
			WorkerData->mConnidHandled[WorkerData->mRoundRobinCnt].get(),
			oRequest,
			&WasDequeued,
			&NewSize)))
		{
			GS_GOTO_CLEAN();
		}
		// FIXME: if found to be empty / not dequeued often enough, maybe put it to an idle-queue
		if (WasDequeued)
			break;
	}

	if (oWasDequeued)
		*oWasDequeued = WasDequeued;

	if (oNewSize)
		*oNewSize = NewSize;

clean:

	return r;
}

int gs_worker_data_3_round_robin_advance(
	struct GsWorkerData3 *WorkerData)
{
	WorkerData->mRoundRobinCnt = WorkerData->mRoundRobinCnt + 1 < WorkerData->mConnidHandled.size()
		? WorkerData->mRoundRobinCnt + 1 : 0;
	return 0;
}

int gs_worker_data_3_nonempty_approx_refresh(
	struct GsWorkerData3 *WorkerData)
{
	int r = 0;

	uint32_t NumNonempty = 0;

	for (size_t i = 0; i < WorkerData->mConnidHandled.size(); ++i)
		if (! WorkerData->mConnidHandled[i]->mQueue.empty())
			NumNonempty++;

	if (!!(r = gs_connid_queue_cond_nonempty_approx_set(WorkerData->mCondShared.get(), NumNonempty)))
		GS_GOTO_CLEAN();

clean:

	return r;
}
