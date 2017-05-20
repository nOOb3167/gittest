#ifndef _GITTEST_NET2_H_
#define _GITTEST_NET2_H_

#include <stddef.h>
#include <stdint.h>

#include <thread>
#include <vector>
#include <deque>

#include <gittest/frame.h>
#include <gittest/net2_fwd.h>
#include <gittest/net2_surrogate.h>
#include <gittest/net2_request.h>
#include <gittest/net2_crankdata.h>
#include <gittest/net2_affinity.h>
#include <gittest/net2_aux.h>

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

/* actually defined in the cpp file */
struct ENetIntrNtwk;

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

int gs_ntwk_reconnect_expend(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct ClntStateReconnect *ioStateReconnect,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	struct GsHostSurrogate *ioHostSurrogate);
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
