#ifndef _GITTEST_NET2_H_
#define _GITTEST_NET2_H_

#include <stddef.h>
#include <stdint.h>
#include <enet/enet.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <list>

#include <enet/enet.h>

#include <gittest/frame.h>

#include <gittest/net2_fwd.h>
#include <gittest/net2_surrogate.h>
#include <gittest/net2_request.h>

#define GS_MAGIC_NUM_WORKER_THREADS 1

#define GS_SERV_AUX_VERYHIGH_TIMEOUT_U32_MS 0xFFFFFFFF
#define GS_SERV_AUX_ARBITRARY_TIMEOUT_MS 5000
#define GS_CONNECT_NUMRETRY   5
#define GS_CONNECT_TIMEOUT_MS 1000
#define GS_CONNECT_NUMRECONNECT 5
#define GS_RECEIVE_TIMEOUT_MS 500000

#define GS_TIMEOUT_1SEC 1000

#define GS_EXTRA_HOST_CREATE_CLIENT_MAGIC 0x501C325E
#define GS_EXTRA_WORKER_CLIENT_MAGIC      0x501C325F
#define GS_STORE_NTWK_CLIENT_MAGIC        0x501C3260
#define GS_STORE_WORKER_CLIENT_MAGIC      0x501C3261

#define GS_EXTRA_HOST_CREATE_SERVER_MAGIC 0x502C325E
#define GS_EXTRA_WORKER_SERVER_MAGIC      0x502C325F
#define GS_STORE_NTWK_SERVER_MAGIC        0x502C3260
#define GS_STORE_WORKER_SERVER_MAGIC      0x502C3261

#define GS_EXTRA_HOST_CREATE_SELFUPDATE_BASIC_MAGIC 0x503C325E
#define GS_EXTRA_WORKER_SELFUPDATE_BASIC_MAGIC      0x503C325F
#define GS_STORE_NTWK_SELFUPDATE_BASIC_MAGIC        0x503C3260
#define GS_STORE_WORKER_SELFUPDATE_BASIC_MAGIC      0x503C3261

#define GS_AFFINITY_IN_PROGRESS_NONE -1

struct GsExtraWorker;
struct GsWorkerData;

typedef ::std::map<gs_connection_surrogate_id_t, gs_worker_id_t> gs_affinity_map_t;
typedef ::std::list<gs_worker_id_t> gs_affinity_list_t;
typedef ::std::vector<gs_connection_surrogate_id_t> gs_affinity_in_progress_t;

/** manual-init struct
	value struct
*/
struct GsPrioData {
	uint32_t mPrio;
	gs_worker_id_t mWorkerId;
};

struct GsPrioDataComparator {
	bool operator() (const struct GsPrioData &a, const struct GsPrioData &b) const {
		return a.mPrio < b.mPrio;
	}
};

typedef ::std::multiset<GsPrioData, GsPrioDataComparator> gs_prio_set_t;
typedef ::std::vector<gs_prio_set_t::iterator> gs_prio_vec_t;

/** Wrapper for a count.
	Semantically the number of reconnections remaining.

    @sa
	   ::clnt_state_reconnect_make_default
	   ::clnt_state_reconnect_have_remaining
	   ::clnt_state_reconnect_expend
*/
struct ClntStateReconnect {
	uint32_t NumReconnections;
	uint32_t NumReconnectionsLeft;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

/** manual-init struct
    value struct

	@sa
	   ::gs_aux_aux_aux_cb_last_chance_t
*/
struct ENetIntrNtwk {
	struct ENetIntr base;

	struct GsWorkerData *WorkerDataSend;
};

/** @sa
       ::gs_packet_create
*/
struct GsPacket {
	struct GsPacketSurrogate mPacket;

	uint8_t *data;
	size_t   dataLength;
};

/** manual-init struct
    value struct

	@sa
	   ::gs_packet_with_offset_get_veclen
*/
struct GsPacketWithOffset {
	struct GsPacket *mPacket;
	uint32_t  mOffsetSize;
	uint32_t  mOffsetObject;
};

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
struct GsAffinityQueue {
	std::mutex mMutexData;
	gs_affinity_map_t mAffinityMap;
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

/** @sa
       ::gs_ctrl_con_create
	   ::gs_ctrl_con_destroy
	   ::gs_ctrl_con_signal_exited
	   ::gs_ctrl_con_wait_exited
	   ::gs_ctrl_con_get_num_workers
*/
struct GsCtrlCon
{
	uint32_t mNumNtwks;
	uint32_t mNumWorkers;
	uint32_t mExitedSignalLeft;
	sp<std::mutex> mCtrlConMutex;
	sp<std::condition_variable> mCtrlConCondExited;
};

/** @sa
       GsExtraHostCreateClient
	   GsExtraHostCreateServer
	   GsExtraHostCreateSelfUpdateBasic
*/
struct GsExtraHostCreate
{
	uint32_t magic;

	int(*cb_create_batch_t)(
		struct GsExtraHostCreate *ExtraHostCreate,
		struct GsHostSurrogate *ioHostSurrogate,
		struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
		size_t LenExtraWorker,
		struct GsExtraWorker **oExtraWorkerArr);
	int(*cb_destroy_host_t)(
		struct GsExtraHostCreate *ExtraHostCreate,
		struct GsHostSurrogate *ioHostSurrogate);
	int(*cb_destroy_t)(struct GsExtraHostCreate *ExtraHostCreate);
};

/** @sa
       GsExtraWorkerClient
	   GsExtraWorkerServer
	   GsExtraWorkerSelfUpdateBasic
	   ::gs_extra_worker_replace
	   ::gs_extra_worker_pp_base_cast
*/
struct GsExtraWorker
{
	uint32_t magic;

	int(*cb_destroy_t)(struct GsExtraWorker *ExtraWorker);
};

/** @sa
       GsStoreNtwkClient
	   GsStoreNtwkServer
	   GsStoreNtwkSelfUpdateBasic
	   ::gs_store_ntwk_init
*/
struct GsStoreNtwk
{
	uint32_t magic;

	int(*cb_destroy_t)(struct GsStoreNtwk *StoreNtwk);

	struct GsIntrTokenSurrogate mIntrToken; /**< notowned */
	struct GsCtrlCon *mCtrlCon;             /**< notowned */
	struct GsAffinityQueue *mAffinityQueue; /**< notowned */

	struct ClntStateReconnect mStateReconnect; /**< owned (nodestroy) */
	struct GsConnectionSurrogateMap *mConnectionSurrogateMap; /**< owned */
};

/** @sa
       GsStoreWorkerClient
	   GsStoreWorkerServer
	   GsStoreWorkerSelfUpdateBasic
	   ::gs_store_worker_init
*/
struct GsStoreWorker
{
	uint32_t magic;

	int(*cb_crank_t)(struct GsCrankData *CrankData);
	int(*cb_destroy_t)(struct GsStoreWorker *StoreWorker);

	struct GsIntrTokenSurrogate mIntrToken; /**< notowned */
	struct GsCtrlCon *mCtrlCon;             /**< notowned */
	struct GsAffinityQueue *mAffinityQueue; /**< notowned */

	uint32_t mNumWorkers;
};

/** @sa
       ::gs_crank_data_create
	   ::gs_crank_data_destroy
*/
struct GsCrankData
{
	struct GsWorkerDataVec *mWorkerDataVecRecv;
	struct GsWorkerData *mWorkerDataSend;
	struct GsStoreWorker *mStoreWorker;
	gs_worker_id_t mWorkerId;
	struct GsExtraWorker *mExtraWorker; /**< mutable */
};

/** @sa
       ::gs_full_connection_common_data_create
	   ::gs_full_connection_common_data_destroy
*/
struct GsFullConnectionCommonData
{
	struct GsIntrTokenSurrogate mIntrToken; /**< owned, stealable */
	struct GsCtrlCon *mCtrlCon;             /**< owned, stealable */
	struct GsAffinityQueue *mAffinityQueue; /**< owned, stealable */
};

/**

    WARNING: custom / special destruction protocol
	- first: wait until mCtrlCon is signalled
	           (the launched threads must signal before exiting)
	- second: destroy the GsFullConnection
	Signaling mCtrlCon before exiting from a launched thread races
	against waiting on mCtrlCon and destroying GsFullConnection.
	This is a problem because a thread is required to be
	deached or joined before destruction.
	The chosen resolution is ensure the threads are detached before
	being destroyed.
	This is accomplished containing threads in a shared pointer
	with a custom thread-detaching deleter.

    @sa
	   ::gs_ctrl_con_wait_exited
       ::gs_sp_thread_detaching_deleter
	   ::std::thread::detach
	   ::gs_full_connection_create
	   ::gs_full_connection_destroy
*/
struct GsFullConnection
{
	sp<std::thread> ThreadNtwk;
	std::vector<sp<std::thread> > mThreadWorker;         /**< owned */
	struct GsWorkerDataVec *mWorkerDataVecRecv;          /**< owned */
	struct GsWorkerData *mWorkerDataSend;                /**< owned */
	struct GsExtraHostCreate *mExtraHostCreate;          /**< owned */
	struct GsStoreNtwk       *mStoreNtwk;                /**< owned */
	struct GsStoreWorker     *mStoreWorker;              /**< owned */
	struct GsCtrlCon *mCtrlCon;                          /**< owned */
	struct GsAffinityQueue *mAffinityQueue;              /**< owned */
};

int gs_helper_api_worker_exit(struct GsWorkerData *WorkerDataSend);
int gs_helper_api_worker_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsWorkerRequestData *oValRequestReconnectRecv);
int gs_helper_api_ntwk_exit(struct GsWorkerDataVec *WorkerDataVecRecv);
int gs_helper_api_ntwk_reconnect(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_helper_api_ntwk_extra_host_create_and_notify(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);

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

int clnt_state_reconnect_make_default(struct ClntStateReconnect *oStateReconnect);
bool clnt_state_reconnect_have_remaining(struct ClntStateReconnect *StateReconnect);
int clnt_state_reconnect_expend(struct ClntStateReconnect *ioStateReconnect);

int gs_packet_create(
	struct GsPacket **oPacket,
	struct GsPacketSurrogate *valPacketSurrogate);
int gs_packet_with_offset_get_veclen(
	struct GsPacketWithOffset *PacketWithOffset,
	uint32_t *oVecLen);

int gs_ctrl_con_create(
	uint32_t NumNtwks,
	uint32_t NumWorkers,
	struct GsCtrlCon **oCtrlCon);
int gs_ctrl_con_destroy(struct GsCtrlCon *CtrlCon);
int gs_ctrl_con_signal_exited(struct GsCtrlCon *CtrlCon);
int gs_ctrl_con_wait_exited(struct GsCtrlCon *CtrlCon);
int gs_ctrl_con_get_num_workers(struct GsCtrlCon *CtrlCon, uint32_t *oNumWorkers);

int gs_extra_worker_replace(
	struct GsExtraWorker **ioExtraWorker,
	struct GsExtraWorker *Replacement);

int gs_store_ntwk_init(
	uint32_t Magic,
	int(*CbDestroy)(struct GsStoreNtwk *StoreNtwk),
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreNtwk *ioStoreNtwk);

int gs_store_worker_init(
	uint32_t Magic,
	int(*CbCrank)(struct GsCrankData *CrankData),
	int(*CbDestroy)(struct GsStoreWorker *StoreWorker),
	uint32_t mNumWorkers,
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreWorker *ioStoreWorker);

int gs_extra_host_create_cb_destroy_host_t_enet_host_destroy(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate);
int gs_extra_host_create_cb_destroy_t_delete(struct GsExtraHostCreate *ExtraHostCreate);

int gs_ntwk_reconnect_expend(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct ClntStateReconnect *ioStateReconnect,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	struct GsHostSurrogate *ioHostSurrogate);
int gs_aux_aux_aux_cb_last_chance_t(
	struct ENetIntr *Intr,
	struct ENetIntrToken *IntrToken);
int gs_ntwk_host_service_worker_disconnect(
	struct GsHostSurrogate *ReferenceHostSurrogate,
	struct GsWorkerRequestData *RequestSend,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_ntwk_host_service_worker_packet(
	struct GsHostSurrogate *ReferenceHostSurrogate,
	struct GsWorkerRequestData *RequestSend,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_ntwk_host_service_sends(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_ntwk_host_service_event(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsAffinityQueue *AffinityQueue,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	int errService,
	struct GsEventSurrogate *Event);
int gs_ntwk_host_service(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk  *StoreNtwk,
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
void gs_ntwk_thread_func(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk *StoreNtwk,
	struct GsExtraHostCreate *ExtraHostCreate,
	const char *ExtraThreadName);

int gs_worker_exit(
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker);
int gs_worker_dequeue_handling_double_notify(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerRequestData *outValRequest);
int gs_worker_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsExtraWorker **oExtraWorker);
void gs_worker_thread_func(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	gs_worker_id_t WorkerId,
	const char *ExtraThreadName);

int gs_net_full_create_connection(
	uint32_t ServPort,
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsStoreNtwk       *StoreNtwk,
	struct GsStoreWorker     *StoreWorker,
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsFullConnection **oConnection,
	const char *ExtraThreadName);

int gs_crank_data_create(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	gs_worker_id_t WorkerId,
	struct GsExtraWorker *ExtraWorker,
	struct GsCrankData **oCrankData);
int gs_crank_data_destroy(struct GsCrankData *CrankData);

int gs_full_connection_common_data_create(
	uint32_t NumWorkers,
	struct GsFullConnectionCommonData **oConnectionCommon);
int gs_full_connection_common_data_destroy(struct GsFullConnectionCommonData *ioData);

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
	struct GsFullConnection **oConnection);
int gs_full_connection_destroy(struct GsFullConnection *Connection);

#endif /* _GITTEST_NET2_H_ */
