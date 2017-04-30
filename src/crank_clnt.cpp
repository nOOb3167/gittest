#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h>
#include <gittest/net2.h>

#include <gittest/crank_clnt.h>

const char * gs_clnt_state_code_to_name(uint32_t Code) {
	/* NOTE: special error-handling */
	int r = 0;

	GS_CLNT_STATE_CODE_DEFINE_ARRAY(ClntStateCodeArray);
	GS_CLNT_STATE_CODE_CHECK_ARRAY_NONUCF(ClntStateCodeArray);

	if (! (Code < LenClntStateCodeArray))
		GS_ERR_CLEAN(1);

clean:
	if (!!r)
		GS_ASSERT(0);

	return ClntStateCodeArray[Code].mCodeName;
}

int clnt_state_make_default(ClntState *oState) {
	ClntState State;
	if (oState)
		*oState = State;
	return 0;
}

int clnt_state_cpy(ClntState *dst, const ClntState *src) {
	*dst = *src;
	return 0;
}

int clnt_state_code(ClntState *State, uint32_t *oCode) {
	int r = 0;
	
	int Code = 0;

	if (! State->mRepositoryT)
		{ Code = GS_CLNT_STATE_CODE_NEED_REPOSITORY; goto need_repository; }
	if (! State->mTreeHeadOid)
		{ Code = GS_CLNT_STATE_CODE_NEED_TREE_HEAD; goto need_tree_head; }
	if (! State->mTreelist || ! State->mMissingTreelist)
		{ Code = GS_CLNT_STATE_CODE_NEED_TREELIST; goto need_treelist; }
	if (! State->mMissingBloblist || ! State->mTreePacketWithOffset)
		{ Code = GS_CLNT_STATE_CODE_NEED_BLOBLIST; goto need_bloblist; }
	if (! State->mWrittenBlob || ! State->mWrittenTree)
		{ Code = GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE; goto need_written_blob_and_tree; }
	if (! State->mUpdatedRefOid)
		{ Code = GS_CLNT_STATE_CODE_NEED_UPDATED_REF; goto need_updated_ref; }
	if (true)
		{ Code = GS_CLNT_STATE_CODE_NEED_NOTHING; goto need_nothing; }

need_repository:
	if (State->mTreeHeadOid)
		GS_ERR_CLEAN(1);
need_tree_head:
	if (State->mTreelist || State->mMissingTreelist)
		GS_ERR_CLEAN(1);
need_treelist:
	if (State->mMissingBloblist || State->mTreePacketWithOffset)
		GS_ERR_CLEAN(1);
need_bloblist:
	if (State->mWrittenBlob || State->mWrittenTree)
		GS_ERR_CLEAN(1);
need_written_blob_and_tree:
	if (State->mUpdatedRefOid)
		GS_ERR_CLEAN(1);
need_updated_ref:
need_nothing:

	if (oCode)
		*oCode = Code;

clean:

	return r;
}

int clnt_state_code_ensure(ClntState *State, uint32_t WantedCode) {
	int r = 0;

	uint32_t FoundCode = 0;

	if (!!(r = clnt_state_code(State, &FoundCode)))
		GS_GOTO_CLEAN();

	if (WantedCode != FoundCode)
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int clnt_state_need_repository_setup2(
	ClntState *State,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath)
{
	int r = 0;

	sp<git_repository *> RepositoryT(new git_repository *(NULL));

	if (!!(r = clnt_state_need_repository_noown2(
		RepoMainOpenPathBuf, LenRepoMainOpenPath,
		RepositoryT.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_TREE_HEAD, a,
		{ a.mRepositoryT = RepositoryT; });

clean:
	if (!!r) {
		if (RepositoryT && *RepositoryT)
			git_repository_free(*RepositoryT);
	}

	return r;
}

int clnt_state_need_repository_noown2(
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	git_repository **oRepositoryT)
{
	int r = 0;

	if (!!(r = aux_repository_open(RepoMainOpenPathBuf, oRepositoryT)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_need_tree_head_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	ClntState *State,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	sp<git_oid> TreeHeadOid(new git_oid);

	git_repository * const RepositoryT = *State->mRepositoryT;

	std::string Buffer;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	if (!!(r = clnt_state_need_tree_head_noown2(
		WorkerDataRecv,
		WorkerDataSend,
		StoreWorker,
		WorkerId,
		RepositoryT,
		TreeHeadOid.get(),
		ioExtraWorker)))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_TREELIST, a,
		{ a.mTreeHeadOid = TreeHeadOid; });

clean:

	return r;
}

int clnt_state_need_tree_head_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	git_repository *RepositoryT,
	git_oid *oTreeHeadOid,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	std::string Buffer;
	GsPacket *Packet = NULL;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = aux_frame_full_write_request_latest_commit_tree(gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		WorkerDataSend,
		&StoreWorker->base.mIntrToken,
		(*ioExtraWorker)->mId,
		Buffer.data(), Buffer.size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects(
		WorkerDataRecv,
		WorkerDataSend,
		WorkerId,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		StoreWorker->base.mAffinityQueue,
		&AffinityToken,
		&Packet,
		NULL,
		GS_EXTRA_WORKER_PP_BASE_CAST(ioExtraWorker, CLIENT))))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, oTreeHeadOid->id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_latest_commit_tree_oid(RepositoryT, StoreWorker->mRefNameMainBuf, &CommitHeadOidT, &TreeHeadOidT)))
		GS_GOTO_CLEAN();

	if (git_oid_cmp(&TreeHeadOidT, oTreeHeadOid) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &CommitHeadOidT);
		GS_LOG(I, PF, "[clnt] Have latest [%.*s]\n", (int)GIT_OID_HEXSZ, buf);
	}

clean:
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	return r;
}

int clnt_state_need_treelist_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	ClntState *State,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	sp<std::vector<git_oid> > Treelist(new std::vector<git_oid>);
	sp<std::vector<git_oid> > MissingTreelist(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<git_oid> &TreeHeadOid = State->mTreeHeadOid;

	if (!!(r = clnt_state_need_treelist_noown2(
		WorkerDataRecv,
		WorkerDataSend,
		StoreWorker,
		WorkerId,
		RepositoryT,
		TreeHeadOid.get(),
		Treelist.get(),
		MissingTreelist.get(),
		ioExtraWorker)))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_BLOBLIST, a,
		{ a.mTreelist = Treelist;
		  a.mMissingTreelist = MissingTreelist; });

clean:

	return r;
}

int clnt_state_need_treelist_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	git_repository *RepositoryT,
	git_oid *TreeHeadOid,
	std::vector<git_oid> *oTreelist,
	std::vector<git_oid> *oMissingTreelist,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	std::string Buffer;
	GsPacket *Packet = NULL;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	GS_BYPART_DATA_VAR(OidVector, BypartTreelist);
	GS_BYPART_DATA_INIT(OidVector, BypartTreelist, oTreelist);

	if (!!(r = aux_frame_full_write_request_treelist(TreeHeadOid->id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		WorkerDataSend,
		&StoreWorker->base.mIntrToken,
		(*ioExtraWorker)->mId,
		Buffer.data(), Buffer.size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects(
		WorkerDataRecv,
		WorkerDataSend,
		WorkerId,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		StoreWorker->base.mAffinityQueue,
		&AffinityToken,
		&Packet,
		NULL,
		GS_EXTRA_WORKER_PP_BASE_CAST(ioExtraWorker, CLIENT))))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREELIST))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartTreelist, gs_bypart_cb_OidVector)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_missing_trees(RepositoryT, oTreelist, oMissingTreelist)))
		GS_GOTO_CLEAN();

clean:
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	return r;
}

int clnt_state_need_bloblist_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	ClntState *State,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	sp<std::vector<git_oid> > MissingBloblist(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<std::vector<git_oid> > &MissingTreelist = State->mMissingTreelist;

	GsPacket *PacketTree = NULL;

	uint32_t OffsetSizeBufferTree;
	uint32_t OffsetObjectBufferTree;

	sp<GsPacketWithOffset> TmpTreePacketWithOffset(new GsPacketWithOffset);

	if (!!(r = clnt_state_need_bloblist_noown2(
		WorkerDataRecv,
		WorkerDataSend,
		StoreWorker,
		WorkerId,
		RepositoryT,
		MissingTreelist.get(),
		MissingBloblist.get(),
		&PacketTree,
		&OffsetSizeBufferTree,
		&OffsetObjectBufferTree,
		ioExtraWorker)))
	{
		GS_GOTO_CLEAN();
	}

	TmpTreePacketWithOffset->mPacket = PacketTree;
	TmpTreePacketWithOffset->mOffsetSize = OffsetSizeBufferTree;
	TmpTreePacketWithOffset->mOffsetObject = OffsetObjectBufferTree;

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE, a,
		{ a.mMissingBloblist = MissingBloblist;
		  a.mTreePacketWithOffset = TmpTreePacketWithOffset; });

clean:

	return r;
}

int clnt_state_need_bloblist_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	git_repository *RepositoryT,
	std::vector<git_oid> *MissingTreelist,
	std::vector<git_oid> *oMissingBloblist,
	struct GsPacket **oPacketTree,
	uint32_t *oOffsetSizeBufferTree,
	uint32_t *oOffsetObjectBufferTree,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	std::string Buffer;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	GsPacket *PacketTree = NULL;

	GsStrided MissingTreelistStrided = {};

	uint32_t BufferTreeLen = 0;

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = gs_strided_for_oid_vec_cpp(MissingTreelist, &MissingTreelistStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_trees(MissingTreelistStrided, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		WorkerDataSend,
		&StoreWorker->base.mIntrToken,
		(*ioExtraWorker)->mId,
		Buffer.data(), Buffer.size())))
	{
		GS_GOTO_CLEAN();
	}

	/* NOTE: NOALLOC - PacketTree Lifetime start */

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects(
		WorkerDataRecv,
		WorkerDataSend,
		WorkerId,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		StoreWorker->base.mAffinityQueue,
		&AffinityToken,
		&PacketTree,
		NULL,
		GS_EXTRA_WORKER_PP_BASE_CAST(ioExtraWorker, CLIENT))))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_ensure_frametype(PacketTree->data, PacketTree->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREES))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(PacketTree->data, PacketTree->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketTree Offsets use start */

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketTree->data, LengthLimit, Offset, &Offset,
		&BufferTreeLen, oOffsetSizeBufferTree, oOffsetObjectBufferTree)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_missing_blobs_bare(
		RepositoryT,
		PacketTree->data, LengthLimit, *oOffsetSizeBufferTree,
		PacketTree->data, LengthLimit, *oOffsetObjectBufferTree, MissingTreelist->size(), oMissingBloblist)))
	{
		GS_GOTO_CLEAN();
	}

	if (oPacketTree)
		*oPacketTree = PacketTree;

clean:
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	return r;
}

int clnt_state_need_written_blob_and_tree_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	ClntState *State,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	sp<std::vector<git_oid> > WrittenBlob(new std::vector<git_oid>);
	sp<std::vector<git_oid> > WrittenTree(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<std::vector<git_oid> > &MissingTreelist = State->mMissingTreelist;
	const sp<std::vector<git_oid> > &MissingBloblist = State->mMissingBloblist;
	const sp<GsPacketWithOffset> &PacketTreeWithOffset = State->mTreePacketWithOffset;
	GsPacket * &PacketTree = PacketTreeWithOffset->mPacket;
	const uint32_t &OffsetSizeBufferTree = PacketTreeWithOffset->mOffsetSize;
	const uint32_t &OffsetObjectBufferTree = PacketTreeWithOffset->mOffsetObject;

	if (!!(r = clnt_state_need_written_blob_and_tree_noown2(
		WorkerDataRecv,
		WorkerDataSend,
		StoreWorker,
		WorkerId,
		RepositoryT,
		MissingTreelist.get(),
		MissingBloblist.get(),
		PacketTree,
		OffsetSizeBufferTree,
		OffsetObjectBufferTree,
		WrittenBlob.get(),
		WrittenTree.get(),
		ioExtraWorker)))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_UPDATED_REF, a,
		{ a.mWrittenBlob = WrittenBlob;
		  a.mWrittenTree = WrittenTree; });

clean:

	return r;
}

int clnt_state_need_written_blob_and_tree_noown2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	git_repository *RepositoryT,
	std::vector<git_oid> *MissingTreelist,
	std::vector<git_oid> *MissingBloblist,
	struct GsPacket * PacketTree,
	uint32_t OffsetSizeBufferTree,
	uint32_t OffsetObjectBufferTree,
	std::vector<git_oid> *oWrittenBlob,
	std::vector<git_oid> *oWrittenTree,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	std::string Buffer;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	GsPacket *PacketBlob = NULL;

	GsStrided MissingBloblistStrided = {};

	uint32_t BufferBlobLen;
	uint32_t OffsetSizeBufferBlob;
	uint32_t OffsetObjectBufferBlob;

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = gs_strided_for_oid_vec_cpp(MissingBloblist, &MissingBloblistStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_blobs(MissingBloblistStrided, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		WorkerDataSend,
		&StoreWorker->base.mIntrToken,
		(*ioExtraWorker)->mId,
		Buffer.data(), Buffer.size())))
	{
		GS_GOTO_CLEAN();
	}

	/* NOTE: NOALLOC - PacketBlob Lifetime start */

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects(
		WorkerDataRecv,
		WorkerDataSend,
		WorkerId,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		StoreWorker->base.mAffinityQueue,
		&AffinityToken,
		&PacketBlob,
		NULL,
		GS_EXTRA_WORKER_PP_BASE_CAST(ioExtraWorker, CLIENT))))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_ensure_frametype(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketBlob Offsets use start */

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketBlob->data, LengthLimit, Offset, &Offset,
		&BufferBlobLen, &OffsetSizeBufferBlob, &OffsetObjectBufferBlob)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_deserialize_blobs(
		RepositoryT,
		PacketBlob->data, LengthLimit, OffsetSizeBufferBlob,
		PacketBlob->data, LengthLimit, OffsetObjectBufferBlob,
		MissingBloblist->size(), oWrittenBlob)))
	{
		GS_GOTO_CLEAN();
	}

	// FIXME: using full size (PacketTree->dataLength) instead of LengthLimit of PacketTree (NOT of PacketBlob!)
	if (!!(r = clnt_deserialize_trees(
		RepositoryT,
		PacketTree->data, PacketTree->dataLength, OffsetSizeBufferTree,
		PacketTree->data, PacketTree->dataLength, OffsetObjectBufferTree,
		MissingTreelist->size(), oWrittenTree)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int clnt_state_need_updated_ref_setup2(
	struct GsStoreWorkerClient *StoreWorker,
	ClntState *State)
{
	int r = 0;

	sp<git_oid> UpdatedRefOid(new git_oid());

	git_repository * const RepositoryT = *State->mRepositoryT;
	git_oid * const TreeHeadOid = State->mTreeHeadOid.get();

	if (!!(r = clnt_state_need_updated_ref_noown2(
		StoreWorker,
		RepositoryT,
		TreeHeadOid,
		UpdatedRefOid.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_NOTHING, a,
	{ a.mUpdatedRefOid = UpdatedRefOid; });

clean:

	return r;
}

int clnt_state_need_updated_ref_noown2(
	struct GsStoreWorkerClient *StoreWorker,
	git_repository *RepositoryT,
	git_oid *TreeHeadOid,
	git_oid *oUpdatedRefOid)
{
	int r = 0;

	git_oid CommitOid = {};

	if (!!(r = gs_buf_ensure_haszero(StoreWorker->mRefNameMainBuf, StoreWorker->mLenRefNameMain + 1)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_commit_ensure_dummy(RepositoryT, TreeHeadOid, &CommitOid)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_commit_setref(RepositoryT, StoreWorker->mRefNameMainBuf, &CommitOid)))
		GS_GOTO_CLEAN();

	if (oUpdatedRefOid)
		git_oid_cpy(oUpdatedRefOid, &CommitOid);

clean:

	return r;
}

int clnt_state_crank2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorkerClient *StoreWorker,
	gs_worker_id_t WorkerId,
	ClntState *State,
	struct GsExtraWorkerClient **ioExtraWorker)
{
	int r = 0;

	uint32_t Code = 0;

	if (!!(r = clnt_state_code(State, &Code)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "servicing [code=[%s]]", gs_clnt_state_code_to_name(Code));

	switch (Code) {

	case GS_CLNT_STATE_CODE_NEED_REPOSITORY:
	{
		if (!!(r = clnt_state_need_repository_setup2(
			State,
			StoreWorker->mRepoMainPathBuf, StoreWorker->mLenRepoMainPath)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_TREE_HEAD:
	{
		if (!!(r = clnt_state_need_tree_head_setup2(
			WorkerDataRecv,
			WorkerDataSend,
			StoreWorker,
			WorkerId,
			State,
			ioExtraWorker)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_TREELIST:
	{
		if (!!(r = clnt_state_need_treelist_setup2(
			WorkerDataRecv,
			WorkerDataSend,
			StoreWorker,
			WorkerId,
			State,
			ioExtraWorker)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_BLOBLIST:
	{
		if (!!(r = clnt_state_need_bloblist_setup2(
			WorkerDataRecv,
			WorkerDataSend,
			StoreWorker,
			WorkerId,
			State,
			ioExtraWorker)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE:
	{
		if (!!(r = clnt_state_need_written_blob_and_tree_setup2(
			WorkerDataRecv,
			WorkerDataSend,
			StoreWorker,
			WorkerId,
			State,
			ioExtraWorker)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_UPDATED_REF:
	{
		if (!!(r = clnt_state_need_updated_ref_setup2(
			StoreWorker,
			State)))
		{
			GS_GOTO_CLEAN();
		}
	}

	case GS_CLNT_STATE_CODE_NEED_NOTHING:
	{
		int r2 = gs_helper_api_worker_exit(WorkerDataSend);
		GS_ERR_CLEAN(r2);
	}
	break;

	default:
	{
		GS_ASSERT(0);
	}
	break;

	}

clean:

	return r;
}

int gs_extra_host_create_client_create(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	struct GsExtraHostCreateClient **oExtraHostCreate)
{
	int r = 0;

	struct GsExtraHostCreateClient *ExtraHostCreate = new GsExtraHostCreateClient();

	ExtraHostCreate->base.magic = GS_EXTRA_HOST_CREATE_CLIENT_MAGIC;
	ExtraHostCreate->base.cb_create_batch_t = gs_extra_host_create_cb_create_t_client;
	ExtraHostCreate->base.cb_destroy_host_t = gs_extra_host_create_cb_destroy_host_t_enet_host_destroy;
	ExtraHostCreate->base.cb_destroy_t = gs_extra_host_create_cb_destroy_t_delete;

	ExtraHostCreate->mServPort = ServPort;
	ExtraHostCreate->mServHostNameBuf = ServHostNameBuf;
	ExtraHostCreate->mLenServHostName = LenServHostName;

	if (oExtraHostCreate)
		*oExtraHostCreate = ExtraHostCreate;

clean:
	if (!!r) {
		GS_DELETE(&ExtraHostCreate);
	}

	return r;
}

int gs_store_ntwk_client_create(
	struct GsIntrTokenSurrogate valIntrTokenSurrogate,
	struct GsCtrlCon *CtrlCon,
	struct GsAffinityQueue *AffinityQueue,
	struct GsStoreNtwkClient **oStoreNtwk)
{
	int r = 0;

	struct GsStoreNtwkClient *StoreNtwk = new GsStoreNtwkClient();

	StoreNtwk->base.magic = GS_STORE_NTWK_CLIENT_MAGIC;
	StoreNtwk->base.cb_destroy_t = gs_store_ntwk_cb_destroy_t_client;
	StoreNtwk->base.mIntrToken = valIntrTokenSurrogate;
	StoreNtwk->base.mCtrlCon = CtrlCon;
	StoreNtwk->base.mAffinityQueue = AffinityQueue;
	
	if (!!(r = clnt_state_reconnect_make_default(&StoreNtwk->base.mStateReconnect)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_connection_surrogate_map_create(&StoreNtwk->base.mConnectionSurrogateMap)))
		GS_GOTO_CLEAN();

	if (oStoreNtwk)
		*oStoreNtwk = StoreNtwk;

clean:
	if (!!r) {
		GS_DELETE(&StoreNtwk);
	}

	return r;
}

int gs_store_ntwk_cb_destroy_t_client(struct GsStoreNtwk *StoreNtwk)
{
	struct GsStoreNtwkClient *pThis = (struct GsStoreNtwkClient *) StoreNtwk;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_STORE_NTWK_CLIENT_MAGIC);

	GS_DELETE_F(pThis->base.mConnectionSurrogateMap, gs_connection_surrogate_map_destroy);

	GS_DELETE(&StoreNtwk);

	return 0;
}

int gs_store_worker_client_create(
	struct GsIntrTokenSurrogate valIntrTokenSurrogate,
	struct GsCtrlCon *CtrlCon,
	struct GsAffinityQueue *AffinityQueue,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	struct GsStoreWorkerClient **oStoreWorker)
{
	int r = 0;

	struct GsStoreWorkerClient *StoreWorker = new GsStoreWorkerClient();

	uint32_t NumWorkers = 0;

	sp<ClntState> State(new ClntState());

	if (!!(r = gs_ctrl_con_get_num_workers(CtrlCon, &NumWorkers)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_state_make_default(State.get())))
		GS_GOTO_CLEAN();

	StoreWorker->base.magic = GS_STORE_WORKER_CLIENT_MAGIC;
	StoreWorker->base.cb_crank_t = gs_store_worker_cb_crank_t_client;
	StoreWorker->base.cb_destroy_t = gs_store_worker_cb_destroy_t_client;
	StoreWorker->base.mIntrToken = valIntrTokenSurrogate;
	StoreWorker->base.mCtrlCon = CtrlCon;
	StoreWorker->base.mAffinityQueue = AffinityQueue;
	StoreWorker->base.mNumWorkers = NumWorkers;
	
	StoreWorker->mRefNameMainBuf = RefNameMainBuf;
	StoreWorker->mLenRefNameMain = LenRefNameMain;
	StoreWorker->mRepoMainPathBuf = RepoMainPathBuf;
	StoreWorker->mLenRepoMainPath = LenRepoMainPath;
	StoreWorker->mClntState = State;

	if (oStoreWorker)
		*oStoreWorker = StoreWorker;

clean:
	if (!!r) {
		GS_DELETE(&StoreWorker);
	}

	return r;
}

int gs_store_worker_cb_destroy_t_client(struct GsStoreWorker *StoreWorker)
{
	struct GsStoreWorkerClient *pThis = (struct GsStoreWorkerClient *) StoreWorker;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_STORE_WORKER_CLIENT_MAGIC);

	GS_DELETE(&StoreWorker);

	return 0;
}

int gs_net_full_create_connection_client(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	struct GsFullConnection **oConnectionClient)
{
	int r = 0;

	struct GsFullConnection *ConnectionClient = NULL;

	ENetIntrTokenCreateFlags *IntrTokenFlags = NULL;
	GsIntrTokenSurrogate      IntrToken = {};

	GsCtrlCon               *CtrlCon = NULL;

	GsAffinityQueue *AffinityQueue = NULL;

	GsExtraHostCreateClient *ExtraHostCreate = NULL;
	GsStoreNtwkClient       *StoreNtwk       = NULL;
	GsStoreWorkerClient     *StoreWorker     = NULL;

	if (!(IntrTokenFlags = enet_intr_token_create_flags_create(ENET_INTR_DATA_TYPE_NONE)))
		GS_GOTO_CLEAN();

	if (!(IntrToken.mIntrToken = enet_intr_token_create(IntrTokenFlags)))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_ctrl_con_create(1, GS_MAGIC_NUM_WORKER_THREADS, &CtrlCon)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_affinity_queue_create(GS_MAGIC_NUM_WORKER_THREADS, &AffinityQueue)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_extra_host_create_client_create(
		ServPort,
		ServHostNameBuf, LenServHostName,
		&ExtraHostCreate)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_store_ntwk_client_create(
		IntrToken,
		CtrlCon,
		AffinityQueue,
		&StoreNtwk)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_store_worker_client_create(
		IntrToken,
		CtrlCon,
		AffinityQueue,
		RefNameMainBuf, LenRefNameMain,
		RepoMainPathBuf, LenRepoMainPath,
		&StoreWorker)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_net_full_create_connection(
		ServPort,
		GS_ARGOWN(&CtrlCon, struct GsCtrlCon),
		GS_ARGOWN(&AffinityQueue, struct GsAffinityQueue),
		GS_ARGOWN(&ExtraHostCreate, struct GsExtraHostCreate),
		GS_ARGOWN(&StoreNtwk, struct GsStoreNtwk),
		GS_ARGOWN(&StoreWorker, struct GsStoreWorker),
		&ConnectionClient,
		"clnt")))
	{
		GS_GOTO_CLEAN();
	}

	if (oConnectionClient)
		*oConnectionClient = ConnectionClient;

clean:
	if (!!r) {
		GS_DELETE_VF(&StoreWorker->base, cb_destroy_t);
		GS_DELETE_VF(&StoreNtwk->base, cb_destroy_t);
		GS_DELETE_VF(&ExtraHostCreate->base, cb_destroy_t);
		GS_DELETE_F(AffinityQueue, gs_affinity_queue_destroy);
		GS_DELETE_F(CtrlCon, gs_ctrl_con_destroy);
		GS_DELETE_F(ConnectionClient, gs_full_connection_destroy);
	}

	return r;
}

int gs_store_worker_cb_crank_t_client(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker **ioExtraWorker,
	gs_worker_id_t WorkerId)
{
	int r = 0;

	GsStoreWorkerClient *pStoreWorker = (GsStoreWorkerClient *) StoreWorker;
	GsExtraWorkerClient **ppIoExtraWorker = (GsExtraWorkerClient **) ioExtraWorker;

	if (pStoreWorker->base.magic != GS_STORE_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if ((*ppIoExtraWorker)->base.magic != GS_EXTRA_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	while (true) {

		if (!!(r = clnt_state_crank2(
			WorkerDataRecv,
			WorkerDataSend,
			pStoreWorker,
			WorkerId,
			pStoreWorker->mClntState.get(),
			ppIoExtraWorker)))
		{
			GS_ERR_NO_CLEAN(r);
		}

	}

noclean:

clean:

	return r;
}

int gs_extra_host_create_cb_create_t_client(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	size_t LenExtraWorker,
	struct GsExtraWorker **oExtraWorkerArr)
{
	int r = 0;

	struct GsExtraHostCreateClient *pThis = (struct GsExtraHostCreateClient *) ExtraHostCreate;

	ENetIntrHostCreateFlags FlagsHost = {};
	ENetHost *host = NULL;
	ENetAddress addr = {};
	ENetPeer *peer = NULL;
	ENetEvent event = {};

	GsConnectionSurrogate ConnectionSurrogate = {};

	struct GsBypartCbDataGsConnectionSurrogateId *ctxstruct = new GsBypartCbDataGsConnectionSurrogateId();

	gs_connection_surrogate_id_t AssignedId = 0;

	int errService = 0;

	if (pThis->base.magic != GS_EXTRA_HOST_CREATE_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	/* create host */

	// FIXME: 128 peerCount, 1 channelLimit
	if (!(host = enet_host_create_interruptible(NULL, 128, 1, 0, 0, &FlagsHost)))
		GS_ERR_CLEAN(1);

	/* connect host */

	if (!!(r = enet_address_set_host(&addr, pThis->mServHostNameBuf)))
		GS_ERR_CLEAN(1);
	addr.port = pThis->mServPort;

	if (!(peer = enet_host_connect(host, &addr, 1, 0)))
		GS_ERR_CLEAN(1);

	/* connect host - wait for completion */

	while (0 <= (errService = enet_host_service(host, &event, GS_TIMEOUT_1SEC))) {
		if (errService > 0 && event.peer == peer && event.type == ENET_EVENT_TYPE_CONNECT)
			break;
	}

	/* a connection event must have been setup above */
	if (errService < 0 ||
		event.type != ENET_EVENT_TYPE_CONNECT ||
		event.peer == NULL)
	{
		GS_ERR_CLEAN(1);
	}

	/* register connection and prepare extra worker data */

	ConnectionSurrogate.mHost = host;
	ConnectionSurrogate.mPeer = peer;
	ConnectionSurrogate.mIsPrincipalClientConnection = true;

	if (!!(r = gs_aux_aux_aux_connection_register_transfer_ownership(
		ConnectionSurrogate,
		GS_ARGOWN(&ctxstruct, GsBypartCbDataGsConnectionSurrogateId),
		ioConnectionSurrogateMap,
		&AssignedId)))
	{
		GS_GOTO_CLEAN();
	}

	for (uint32_t i = 0; i < LenExtraWorker; i++)
		if (!!(r = gs_extra_worker_client_create(&oExtraWorkerArr[i], AssignedId)))
			GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		ioHostSurrogate->mHost = host;

clean:
	if (!!r) {
		GS_DELETE(&ctxstruct);
	}

	return r;
}

int gs_extra_worker_client_create(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id)
{
	struct GsExtraWorkerClient * pThis = new GsExtraWorkerClient();

	pThis->base.magic = GS_EXTRA_WORKER_CLIENT_MAGIC;
	pThis->base.cb_destroy_t = gs_extra_worker_cb_destroy_t_client;

	pThis->mId = Id;

	if (oExtraWorker)
		*oExtraWorker = &pThis->base;

	return 0;
}

int gs_extra_worker_cb_destroy_t_client(struct GsExtraWorker *ExtraWorker)
{
	struct GsExtraWorkerClient *pThis = (struct GsExtraWorkerClient *) ExtraWorker;

	if (!pThis)
		return 0;

	GS_ASSERT(ExtraWorker->magic == GS_EXTRA_WORKER_CLIENT_MAGIC);

	GS_DELETE(&ExtraWorker);

	return 0;
}
