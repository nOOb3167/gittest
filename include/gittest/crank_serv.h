#ifndef _GITTEST_CRANK_SERV_H_
#define _GITTEST_CRANK_SERV_H_

#include <gittest/net2.h>

/** @sa
       ::gs_extra_host_create_server_create
	   ::gs_extra_host_create_cb_destroy_host_t_enet_host_destroy
	   ::gs_extra_host_create_cb_destroy_t_delete
*/
struct GsExtraHostCreateServer
{
	struct GsExtraHostCreate base;

	uint32_t mServPort;
};

/** @sa
       ::gs_extra_worker_server_create
	   ::gs_extra_worker_cb_destroy_t_server
*/
struct GsExtraWorkerServer
{
	struct GsExtraWorker base;
};

/** @sa
       ::gs_store_ntwk_server_create
	   ::gs_store_ntwk_cb_destroy_t_server
*/
struct GsStoreNtwkServer
{
	struct GsStoreNtwk base;
};

/** @sa
       ::gs_store_worker_server_create
	   ::gs_store_worker_cb_destroy_t_server
*/
struct GsStoreWorkerServer
{
	struct GsStoreWorker base;

	const char *mRefNameMainBuf; size_t mLenRefNameMain;
	const char *mRefNameSelfUpdateBuf; size_t mLenRefNameSelfUpdate;
	const char *mRepoMainPathBuf; size_t mLenRepoMainPath;
	const char *mRepoSelfUpdatePathBuf; size_t mLenRepoSelfUpdatePath;
	const char *mSelfUpdateBlobNameBuf; size_t mLenSelfUpdateBlobName;
};

int gs_extra_host_create_server_create(
	uint32_t ServPort,
	struct GsExtraHostCreateServer **oExtraHostCreate);
int gs_extra_host_create_cb_create_t_server(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	size_t LenExtraWorker,
	struct GsExtraWorker **oExtraWorkerArr);

int gs_extra_worker_server_create(
	struct GsExtraWorker **oExtraWorker);
int gs_extra_worker_cb_destroy_t_server(struct GsExtraWorker *ExtraWorker);

int gs_store_ntwk_server_create(
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreNtwkServer **oStoreNtwk);
int gs_store_ntwk_cb_destroy_t_server(struct GsStoreNtwk *StoreNtwk);

int gs_store_worker_server_create(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	const char *SelfUpdateBlobNameBuf, size_t LenSelfUpdateBlobName,
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreWorkerServer **oStoreWorker);
int gs_store_worker_server_aux_state_service_request_blobs2(
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	GsPacket *Packet,
	uint32_t OffsetSize,
	git_repository *Repository,
	const GsFrameType &FrameTypeResponse);
int gs_store_worker_cb_crank_t_server(struct GsCrankData *CrankData);
int gs_store_worker_cb_destroy_t_server(struct GsStoreWorker *StoreWorker);

int gs_net_full_create_connection_server(
	uint32_t ServPort,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	const char *SelfUpdateBlobNameBuf, size_t LenSelfUpdateBlobName,
	struct GsFullConnection **oConnectionServer);

#endif /* _GITTEST_CRANK_SERV_H_ */
