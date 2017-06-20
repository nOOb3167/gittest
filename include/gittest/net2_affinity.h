#ifndef _NET2_AFFINITY_H_
#define _NET2_AFFINITY_H_

#include <stdint.h>

#include <mutex>
#include <vector>
#include <list>
#include <set>

#include <gittest/net2_fwd.h>
#include <gittest/net2_surrogate.h>
#include <gittest/net2_request.h>

#define GS_AFFINITY_IN_PROGRESS_NONE -1

typedef ::std::map<gs_connection_surrogate_id_t, gs_worker_id_t> gs_affinity_map_t;
typedef ::std::vector<std::set<gs_connection_surrogate_id_t> >   gs_affinity_reverse_t;     /* worker -> set<conid> */
typedef ::std::vector<gs_connection_surrogate_id_t>              gs_affinity_in_progress_t; /* worker -> inprogress */

/** manual-init struct
	value struct
*/
struct GsPrioData
{
	uint32_t mPrio;
	gs_worker_id_t mWorkerId;
};

struct GsPrioDataComparator
{
	bool operator() (const struct GsPrioData &a, const struct GsPrioData &b) const;
};

typedef ::std::multiset<GsPrioData, GsPrioDataComparator> gs_prio_set_t;
typedef ::std::vector<gs_prio_set_t::iterator> gs_prio_vec_t;                               /* worker -> prioIterator */

/** the lock for mAffinityInProgress field N is GsWorkerDataVec N
    @sa
	   ::gs_affinity_queue_create
	   ::gs_affinity_queue_destroy
	   ::gs_affinity_queue_worker_acquire_ready
	   ::gs_affinity_queue_worker_completed_all_requests
	   ::gs_affinity_queue_request_dequeue_and_acquire
	   ::gs_affinity_queue_prio_increment_nolock
	   ::gs_affinity_queue_prio_zero_nolock
*/
struct GsAffinityQueue
{
	std::mutex mMutexData;
	gs_affinity_map_t mAffinityMap;
	gs_affinity_reverse_t mAffinityReverse;
	gs_affinity_in_progress_t mAffinityInProgress; /**< special locking semantics */
	gs_prio_set_t mPrioSet;
	gs_prio_vec_t mPrioVec;
};

/** nodestroy
    should be initializable by '= {}' assignment
    @sa
	   ::gs_affinity_token_acquire_raw_nolock
	   ::gs_affinity_token_release
*/
struct GsAffinityToken
{
	uint32_t mIsAcquired;
	gs_worker_id_t mExpectedWorker;         /**< notowned */
	struct GsAffinityQueue *mAffinityQueue; /**< notowned */
	struct GsWorkerData *mWorkerData;        /**< notowned */
	struct GsWorkerRequestData mValRequest; /**< notowned */
};

int gs_affinity_queue_create(
	size_t NumWorkers,
	struct GsAffinityQueue **oAffinityQueue);
int gs_affinity_queue_destroy(struct GsAffinityQueue *AffinityQueue);
int gs_affinity_queue_worker_acquire_ready_and_enqueue(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerDataVec *WorkerDataVec,
	struct GsWorkerRequestData *valRequestData,
	gs_connection_surrogate_id_t ConnectionId);
int gs_affinity_queue_worker_completed_all_requests_somelock(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t WorkerId,
	std::unique_lock<std::mutex> *LockAffinityQueue);
int gs_affinity_queue_request_dequeue_and_acquire(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t WorkerId,
	uint32_t TimeoutMs,
	struct GsWorkerRequestData *oValRequest,
	struct GsAffinityToken *ioAffinityToken);
int gs_affinity_queue_prio_zero_nolock(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t WorkerId,
	std::unique_lock<std::mutex> *Lock);
int gs_affinity_queue_prio_increment_nolock(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t WorkerId,
	std::unique_lock<std::mutex> *Lock);
int gs_affinity_queue_prio_decrement_nolock(
	struct GsAffinityQueue *AffinityQueue,
	gs_worker_id_t WorkerId,
	std::unique_lock<std::mutex> *Lock);
int gs_affinity_queue_prio_acquire_lowest_and_increment_nolock(
	struct GsAffinityQueue *AffinityQueue,
	std::unique_lock<std::mutex> *Lock,
	gs_worker_id_t *oWorkerLowestPrioId);
int gs_affinity_queue_helper_worker_double_lock(
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t DstWorkerId,
	gs_worker_id_t SrcWorkerId,
	std::unique_lock<std::mutex> ioDoubleLock[2]);

int gs_affinity_token_acquire_raw_nolock(
	struct GsAffinityToken *ioAffinityToken,
	gs_worker_id_t WorkerId,
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerData *WorkerData,
	struct GsWorkerRequestData valRequest);
int gs_affinity_token_release(
	struct GsAffinityToken *ioAffinityToken);

#endif /* _NET2_AFFINITY_H_ */
