#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <stdint.h>

#include <gittest/misc.h>
#include <gittest/net2_fwd.h>
#include <gittest/net2.h>

#include <gittest/net2_crankdata.h>

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
	GS_DELETE(&ExtraHostCreate, GsExtraHostCreate);
	return 0;
}

int gs_extra_worker_replace(
	struct GsExtraWorker **ioExtraWorker,
	struct GsExtraWorker *Replacement)
{
	if (ioExtraWorker)
		GS_DELETE_VF(ioExtraWorker, cb_destroy_t);
	*ioExtraWorker = Replacement;
	return 0;
}

int gs_extra_worker_cb_destroy_t_delete(struct GsExtraWorker *ExtraWorker)
{
	GS_DELETE(&ExtraWorker, GsExtraWorker);
	return 0;
}

int gs_store_ntwk_init(
	uint32_t Magic,
	int(*CbDestroy)(struct GsStoreNtwk *StoreNtwk),
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreNtwk *ioStoreNtwk)
{
	int r = 0;

	ioStoreNtwk->magic = Magic;
	ioStoreNtwk->cb_destroy_t = CbDestroy;
	ioStoreNtwk->mIntrToken = ConnectionCommon->mIntrToken;
	ioStoreNtwk->mCtrlCon = ConnectionCommon->mCtrlCon;
	ioStoreNtwk->mAffinityQueue = ConnectionCommon->mAffinityQueue;

	if (!!(r = clnt_state_reconnect_make_default(&ioStoreNtwk->mStateReconnect)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_connection_surrogate_map_create(&ioStoreNtwk->mConnectionSurrogateMap)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_store_ntwk_cb_destroy_t_delete(struct GsStoreNtwk *StoreNtwk)
{
	GS_DELETE(&StoreNtwk, GsStoreNtwk);
	return 0;
}

int gs_store_worker_init(
	uint32_t Magic,
	int(*CbCrank)(struct GsCrankData *CrankData),
	int(*CbDestroy)(struct GsStoreWorker *StoreWorker),
	uint32_t mNumWorkers,
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreWorker *ioStoreWorker)
{
	ioStoreWorker->magic = Magic;
	ioStoreWorker->cb_crank_t = CbCrank;
	ioStoreWorker->cb_destroy_t = CbDestroy;
	ioStoreWorker->mIntrToken = ConnectionCommon->mIntrToken;
	ioStoreWorker->mCtrlCon = ConnectionCommon->mCtrlCon;
	ioStoreWorker->mAffinityQueue = ConnectionCommon->mAffinityQueue;
	ioStoreWorker->mNumWorkers = mNumWorkers;
	return 0;
}

int gs_store_worker_cb_destroy_t_delete(struct GsStoreWorker *StoreWorker)
{
	GS_DELETE(&StoreWorker, GsStoreWorker);
	return 0;
}

int gs_crank_data_create(
	struct GsWorkerDataVec *WorkerDataVecRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	gs_worker_id_t WorkerId,
	struct GsExtraWorker *ExtraWorker,
	struct GsCrankData **oCrankData)
{
	struct GsCrankData *CrankData = new GsCrankData();

	CrankData->mWorkerDataVecRecv = WorkerDataVecRecv;
	CrankData->mWorkerDataSend = WorkerDataSend;
	CrankData->mStoreWorker = StoreWorker;
	CrankData->mWorkerId = WorkerId;
	CrankData->mExtraWorker = ExtraWorker;

	if (oCrankData)
		*oCrankData = CrankData;

	return 0;
}

int gs_crank_data_destroy(struct GsCrankData *CrankData)
{
	GS_DELETE(&CrankData, GsCrankData);
	return 0;
}
