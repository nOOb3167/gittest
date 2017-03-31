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
int clnt_state_crank2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath);

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
