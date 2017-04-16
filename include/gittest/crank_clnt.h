#ifndef _GITTEST_CRANK_CLNT_H_
#define _GITTEST_CRANK_CLNT_H_

#include <gittest/net2.h>

#define GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(PTR_VARNAME_CLNTSTATE, CODE, VARNAME_TMPSTATE, STATEMENTBLOCK) \
	{ ClntState VARNAME_TMPSTATE;                                                                       \
      if (!!clnt_state_cpy(& (VARNAME_TMPSTATE), (PTR_VARNAME_CLNTSTATE)))                              \
        GS_ERR_CLEAN(9998);                                                                             \
	  { STATEMENTBLOCK }                                                                                \
	  if (!!clnt_state_code_ensure(& (VARNAME_TMPSTATE), (CODE)))                                       \
	    GS_ERR_CLEAN(9999);                                                                             \
	  if (!!clnt_state_cpy((PTR_VARNAME_CLNTSTATE), & (VARNAME_TMPSTATE)))                              \
	    GS_ERR_CLEAN(9998); }

#define GS_CLNT_STATE_CODE_DECL2(name) GS_CLNT_STATE_CODE_ ## name
#define GS_CLNT_STATE_CODE_DECL(name) { # name , GS_CLNT_STATE_CODE_DECL2(name) }

#define GS_CLNT_STATE_CODE_DEFINE_ARRAY(VARNAME)             \
	GsClntStateCodeEntry VARNAME[] = {                       \
		GS_CLNT_STATE_CODE_DECL(NEED_REPOSITORY),            \
		GS_CLNT_STATE_CODE_DECL(NEED_TREE_HEAD),             \
		GS_CLNT_STATE_CODE_DECL(NEED_TREELIST),              \
		GS_CLNT_STATE_CODE_DECL(NEED_BLOBLIST),              \
		GS_CLNT_STATE_CODE_DECL(NEED_WRITTEN_BLOB_AND_TREE), \
		GS_CLNT_STATE_CODE_DECL(NEED_UPDATED_REF),           \
		GS_CLNT_STATE_CODE_DECL(NEED_NOTHING),               \
	    };                                                       \
	size_t Len ## VARNAME = sizeof (VARNAME) / sizeof *(VARNAME);

#define GS_CLNT_STATE_CODE_CHECK_ARRAY_NONUCF(VARNAME) \
	for (size_t i = 0; i < Len ## VARNAME; i++) \
		if ((VARNAME)[i].mCodeNum != i) \
			GS_ERR_CLEAN_L(1, E, S, "state code array non-contiguous");

/** @sa
       ::GS_CLNT_STATE_CODE_DEFINE_ARRAY
	   ::clnt_state_code
	   ::clnt_state_crank2
*/
enum gs_clnt_state_code_t {
	GS_CLNT_STATE_CODE_NEED_REPOSITORY = 0,
	GS_CLNT_STATE_CODE_NEED_TREE_HEAD = 1,
	GS_CLNT_STATE_CODE_NEED_TREELIST = 2,
	GS_CLNT_STATE_CODE_NEED_BLOBLIST = 3,
	GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE = 4,
	GS_CLNT_STATE_CODE_NEED_UPDATED_REF = 5,
	GS_CLNT_STATE_CODE_NEED_NOTHING = 6,
	GS_CLNT_STATE_CODE_MAX_ENUM = 0x7FFFFFFF,
};

struct GsClntStateCodeEntry {
	const char *mCodeName;
	uint32_t    mCodeNum;
};

struct ClntState {
	sp<git_repository *> mRepositoryT;

	sp<git_oid> mTreeHeadOid;

	sp<std::vector<git_oid> > mTreelist;
	sp<std::vector<git_oid> > mMissingTreelist;

	sp<std::vector<git_oid> >  mMissingBloblist;
	sp<GsPacketWithOffset> mTreePacketWithOffset;

	sp<std::vector<git_oid> > mWrittenBlob;
	sp<std::vector<git_oid> > mWrittenTree;

	sp<git_oid> mUpdatedRefOid;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

/** @sa
       ::gs_extra_host_create_client_create
	   ::gs_extra_host_create_cb_destroy_host_t_enet_host_destroy
	   ::gs_extra_host_create_cb_destroy_t_delete
*/

struct GsExtraHostCreateClient
{
	struct GsExtraHostCreate base;

	uint32_t mServPort;
	const char *mServHostNameBuf; size_t mLenServHostName;
};

/** @sa
       ::gs_extra_worker_client_create
	   ::gs_extra_worker_cb_destroy_t_client
*/
struct GsExtraWorkerClient
{
	struct GsExtraWorker base;

	gs_connection_surrogate_id_t mId;
};

/** @sa
       ::gs_store_ntwk_client_create
	   ::gs_store_ntwk_cb_destroy_t_client
*/
struct GsStoreNtwkClient
{
	struct GsStoreNtwk base;
};

/** @sa
       ::gs_store_worker_client_create
	   ::gs_store_worker_cb_destroy_t_client
*/
struct GsStoreWorkerClient
{
	struct GsStoreWorker base;

	const char *mRefNameMainBuf; size_t mLenRefNameMain;
	const char *mRepoMainPathBuf; size_t mLenRepoMainPath;

	sp<ClntState> mClntState;
};

const char * gs_clnt_state_code_to_name(uint32_t Code);

int clnt_state_make_default(ClntState *oState);
int clnt_state_cpy(ClntState *dst, const ClntState *src);
int clnt_state_code(ClntState *State, uint32_t *oCode);
int clnt_state_code_ensure(ClntState *State, uint32_t WantedCode);

int clnt_state_need_repository_setup2(
	ClntState *State,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath);
int clnt_state_need_repository_noown2(
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	git_repository **oRepositoryT);
int clnt_state_need_tree_head_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State,
	const char *RefNameMainBuf, size_t LenRefNameMain);
int clnt_state_need_tree_head_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	git_repository *RepositoryT,
	git_oid *oTreeHeadOid);
int clnt_state_need_treelist_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State);
int clnt_state_need_treelist_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	git_repository *RepositoryT,
	git_oid *TreeHeadOid,
	std::vector<git_oid> *oTreelist,
	std::vector<git_oid> *oMissingTreelist);
int clnt_state_need_treelist_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State);
int clnt_state_need_treelist_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	git_repository *RepositoryT,
	git_oid *TreeHeadOid,
	std::vector<git_oid> *oTreelist,
	std::vector<git_oid> *oMissingTreelist);
int clnt_state_need_bloblist_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State);
int clnt_state_need_bloblist_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	git_repository *RepositoryT,
	std::vector<git_oid> *MissingTreelist,
	std::vector<git_oid> *oMissingBloblist,
	struct GsPacket **oPacketTree,
	uint32_t *oOffsetSizeBufferTree,
	uint32_t *oOffsetObjectBufferTree);
int clnt_state_need_written_blob_and_tree_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State);
int clnt_state_need_written_blob_and_tree_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	git_repository *RepositoryT,
	std::vector<git_oid> *MissingTreelist,
	std::vector<git_oid> *MissingBloblist,
	struct GsPacket * PacketTree,
	uint32_t OffsetSizeBufferTree,
	uint32_t OffsetObjectBufferTree,
	std::vector<git_oid> *oWrittenBlob,
	std::vector<git_oid> *oWrittenTree);
int clnt_state_need_updated_ref_setup2(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	ClntState *State);
int clnt_state_need_updated_ref_noown2(
	git_repository *RepositoryT,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	git_oid *TreeHeadOid,
	git_oid *oUpdatedRefOid);
int clnt_state_crank2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath);

int gs_extra_host_create_client_create(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	struct GsExtraHostCreateClient **oExtraHostCreate);
int gs_store_ntwk_client_create(
	struct GsIntrTokenSurrogate valIntrTokenSurrogate,
	struct GsCtrlCon *CtrlCon,
	struct GsStoreNtwkClient **oStoreNtwk);
int gs_store_ntwk_cb_destroy_t_client(struct GsStoreNtwk *StoreNtwk);
int gs_store_worker_client_create(
	struct GsIntrTokenSurrogate valIntrTokenSurrogate,
	struct GsCtrlCon *CtrlCon,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	struct GsStoreWorkerClient **oStoreWorker);
int gs_store_worker_cb_destroy_t_client(struct GsStoreWorker *StoreWorker);

int gs_net_full_create_connection_client(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	struct GsFullConnection **oConnectionClient);

int gs_store_worker_cb_crank_t_client(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker *ExtraWorker);

int gs_extra_host_create_cb_create_t_client(
	GsExtraHostCreate *ExtraHostCreate,
	GsHostSurrogate *ioHostSurrogate,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsExtraWorker **oExtraWorker);

int gs_extra_worker_client_create(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id);
int gs_extra_worker_cb_destroy_t_client(struct GsExtraWorker *ExtraWorker);

#endif /* _GITTEST_CRANK_CLNT_H_ */
