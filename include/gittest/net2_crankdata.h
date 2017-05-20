#ifndef _NET2_CRANKDATA_H_
#define _NET2_CRANKDATA_H_

#include <gittest/net2_fwd.h>
#include <gittest/net2_surrogate.h>

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

int clnt_state_reconnect_make_default(struct ClntStateReconnect *oStateReconnect);
bool clnt_state_reconnect_have_remaining(struct ClntStateReconnect *StateReconnect);
int clnt_state_reconnect_expend(struct ClntStateReconnect *ioStateReconnect);

int gs_extra_host_create_cb_destroy_host_t_enet_host_destroy(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate);
int gs_extra_host_create_cb_destroy_t_delete(struct GsExtraHostCreate *ExtraHostCreate);

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

int gs_crank_data_create(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	gs_worker_id_t WorkerId,
	struct GsExtraWorker *ExtraWorker,
	struct GsCrankData **oCrankData);
int gs_crank_data_destroy(struct GsCrankData *CrankData);

#endif /* _NET2_CRANKDATA_H_ */
