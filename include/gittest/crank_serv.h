#ifndef _GITTEST_CRANK_SERV_H_
#define _GITTEST_CRANK_SERV_H_

#include <gittest/net2.h>

struct GsExtraHostCreateServer
{
	struct GsExtraHostCreate base;

	uint32_t mServPort;
};

struct GsExtraWorkerServer
{
	struct GsExtraWorker base;
};

struct GsStoreNtwkServer
{
	struct GsStoreNtwk base;
};

struct GsStoreWorkerServer
{
	struct GsStoreWorker base;

	const char *mRefNameMainBuf; size_t mLenRefNameMain;
	const char *mRefNameSelfUpdateBuf; size_t mLenRefNameSelfUpdate;
	const char *mRepoMainPathBuf; size_t mLenRepoMainPath;
	const char *mRepoSelfUpdatePathBuf; size_t mLenRepoSelfUpdatePath;

	struct GsIntrTokenSurrogate mIntrToken;
};

int serv_state_service_request_blobs2(
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	GsPacket *Packet,
	uint32_t OffsetSize,
	git_repository *Repository,
	const GsFrameType &FrameTypeResponse);

int serv_state_crank2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsIntrTokenSurrogate *IntrToken,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath);

int gs_net_full_create_connection_server(
	uint32_t ServPort,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	struct GsFullConnection **oConnectionServer);

int gs_store_worker_cb_crank_t_server(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker *ExtraWorker);

int gs_extra_host_create_cb_create_t_server(
	GsExtraHostCreate *ExtraHostCreate,
	GsHostSurrogate *ioHostSurrogate,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsExtraWorker **oExtraWorker);

int gs_extra_worker_cb_create_t_server(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id);
int gs_extra_worker_cb_destroy_t_server(struct GsExtraWorker *ExtraWorker);

#endif /* _GITTEST_CRANK_SERV_H_ */
