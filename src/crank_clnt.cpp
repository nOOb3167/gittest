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

int clnt_state_need_tree_head_work(struct GsCrankData *CrankData)
{
	int r = 0;

	sp<git_oid> TreeHeadOid(new git_oid);

	struct GsStoreWorkerClient *pStoreWorker = (struct GsStoreWorkerClient *) CrankData->mStoreWorker;
	struct GsExtraWorkerClient *pExtraWorker = (struct GsExtraWorkerClient *) CrankData->mExtraWorker;

	struct ClntState * const State = pStoreWorker->mClntState.get();

	std::string Buffer;
	GsPacket *Packet = NULL;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (pStoreWorker->base.magic != GS_STORE_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (pExtraWorker->base.magic != GS_EXTRA_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_full_write_request_latest_commit_tree(gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		CrankData->mWorkerDataSend,
		&CrankData->mStoreWorker->mIntrToken,
		pExtraWorker->mId,
		Buffer.data(), Buffer.size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects2(
		CrankData,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		&AffinityToken,
		&Packet,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, TreeHeadOid->id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_latest_commit_tree_oid(*State->mRepositoryT, pStoreWorker->mRefNameMainBuf, &CommitHeadOidT, &TreeHeadOidT)))
		GS_GOTO_CLEAN();

	if (git_oid_cmp(&TreeHeadOidT, TreeHeadOid.get()) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &CommitHeadOidT);
		GS_LOG(I, PF, "[clnt] Have latest [%.*s]\n", (int)GIT_OID_HEXSZ, buf);
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_TREELIST, a,
		{ a.mTreeHeadOid = TreeHeadOid; });

clean:
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	return r;
}

int clnt_state_need_treelist_work(struct GsCrankData *CrankData)
{
	int r = 0;

	sp<std::vector<git_oid> > Treelist(new std::vector<git_oid>);
	sp<std::vector<git_oid> > MissingTreelist(new std::vector<git_oid>);

	struct GsStoreWorkerClient *pStoreWorker = (struct GsStoreWorkerClient *) CrankData->mStoreWorker;
	struct GsExtraWorkerClient *pExtraWorker = (struct GsExtraWorkerClient *) CrankData->mExtraWorker;

	struct ClntState * const State = pStoreWorker->mClntState.get();

	std::string Buffer;
	GsPacket *Packet = NULL;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	GS_BYPART_DATA_VAR(OidVector, BypartTreelist);
	GS_BYPART_DATA_INIT(OidVector, BypartTreelist, Treelist.get());

	if (pStoreWorker->base.magic != GS_STORE_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (pExtraWorker->base.magic != GS_EXTRA_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_full_write_request_treelist(State->mTreeHeadOid->id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		CrankData->mWorkerDataSend,
		&CrankData->mStoreWorker->mIntrToken,
		pExtraWorker->mId,
		Buffer.data(), Buffer.size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects2(
		CrankData,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		&AffinityToken,
		&Packet,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREELIST))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartTreelist, gs_bypart_cb_OidVector)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_missing_trees(*State->mRepositoryT, Treelist.get(), MissingTreelist.get())))
		GS_GOTO_CLEAN();

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_BLOBLIST, a,
		{ a.mTreelist = Treelist;
		  a.mMissingTreelist = MissingTreelist; });

clean:
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	return r;
}

int clnt_state_need_bloblist_work(struct GsCrankData *CrankData)
{
	int r = 0;

	sp<std::vector<git_oid> > MissingBloblist(new std::vector<git_oid>);
	sp<GsPacketWithOffset> PacketTreeWO(new GsPacketWithOffset);

	struct GsStoreWorkerClient *pStoreWorker = (struct GsStoreWorkerClient *) CrankData->mStoreWorker;
	struct GsExtraWorkerClient *pExtraWorker = (struct GsExtraWorkerClient *) CrankData->mExtraWorker;

	struct ClntState * const State = pStoreWorker->mClntState.get();

	std::string Buffer;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	struct GsStrided MissingTreelistStrided = {};

	uint32_t BufferTreeLen = 0;

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (pStoreWorker->base.magic != GS_STORE_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (pExtraWorker->base.magic != GS_EXTRA_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (!!(r = gs_strided_for_oid_vec_cpp(State->mMissingTreelist.get(), &MissingTreelistStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_trees(MissingTreelistStrided, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		CrankData->mWorkerDataSend,
		&CrankData->mStoreWorker->mIntrToken,
		pExtraWorker->mId,
		Buffer.data(), Buffer.size())))
	{
		GS_GOTO_CLEAN();
	}

	/* NOTE: NOALLOC - PacketTree Lifetime start */

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects2(
		CrankData,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		&AffinityToken,
		&PacketTreeWO->mPacket,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_ensure_frametype(PacketTreeWO->mPacket->data, PacketTreeWO->mPacket->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREES))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(PacketTreeWO->mPacket->data, PacketTreeWO->mPacket->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketTree Offsets use start */

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketTreeWO->mPacket->data, LengthLimit, Offset, &Offset,
		NULL, &PacketTreeWO->mOffsetSize, &PacketTreeWO->mOffsetObject)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_packet_with_offset_get_veclen(PacketTreeWO.get(), &BufferTreeLen)))
		GS_GOTO_CLEAN();

	// FIXME: proper handling for this condition / malformed request or response
	//   presumably server did not send all the requested trees
	GS_ASSERT(BufferTreeLen == State->mMissingTreelist->size());

	if (!!(r = clnt_missing_blobs_bare(
		*State->mRepositoryT,
		PacketTreeWO->mPacket->data, LengthLimit, PacketTreeWO->mOffsetSize,
		PacketTreeWO->mPacket->data, LengthLimit, PacketTreeWO->mOffsetObject,
		BufferTreeLen,
		MissingBloblist.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE, a,
		{ a.mMissingBloblist = MissingBloblist;
		  a.mTreePacketWithOffset = PacketTreeWO; });

clean:
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	return r;
}

int clnt_state_need_written_blob_and_tree_work(struct GsCrankData *CrankData)
{
	int r = 0;

	sp<std::vector<git_oid> > WrittenBlob(new std::vector<git_oid>);
	sp<std::vector<git_oid> > WrittenTree(new std::vector<git_oid>);
	sp<GsPacketWithOffset> PacketBlobWO(new GsPacketWithOffset);

	struct GsStoreWorkerClient *pStoreWorker = (struct GsStoreWorkerClient *) CrankData->mStoreWorker;
	struct GsExtraWorkerClient *pExtraWorker = (struct GsExtraWorkerClient *) CrankData->mExtraWorker;

	struct ClntState * const State = pStoreWorker->mClntState.get();

	std::string Buffer;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	struct GsStrided MissingBloblistStrided = {};

	uint32_t BufferBlobLen = 0;
	uint32_t BufferTreeLen = 0;

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (pStoreWorker->base.magic != GS_STORE_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (pExtraWorker->base.magic != GS_EXTRA_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (!!(r = gs_strided_for_oid_vec_cpp(State->mMissingBloblist.get(), &MissingBloblistStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_blobs(MissingBloblistStrided, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		CrankData->mWorkerDataSend,
		&CrankData->mStoreWorker->mIntrToken,
		pExtraWorker->mId,
		Buffer.data(), Buffer.size())))
	{
		GS_GOTO_CLEAN();
	}

	/* NOTE: NOALLOC - PacketBlob Lifetime start */

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects2(
		CrankData,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		&AffinityToken,
		&PacketBlobWO->mPacket,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_ensure_frametype(PacketBlobWO->mPacket->data, PacketBlobWO->mPacket->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(PacketBlobWO->mPacket->data, PacketBlobWO->mPacket->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketBlob Offsets use start */

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketBlobWO->mPacket->data, LengthLimit, Offset, &Offset,
		NULL, &PacketBlobWO->mOffsetSize, &PacketBlobWO->mOffsetObject)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_packet_with_offset_get_veclen(PacketBlobWO.get(), &BufferBlobLen)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_packet_with_offset_get_veclen(State->mTreePacketWithOffset.get(), &BufferTreeLen)))
		GS_GOTO_CLEAN();

	// FIXME: proper handling for this condition / malformed request or response
	//   presumably server did not send all the requested blobs and trees
	GS_ASSERT(BufferBlobLen == State->mMissingBloblist->size());
	GS_ASSERT(BufferTreeLen == State->mMissingTreelist->size());

	if (!!(r = clnt_deserialize_blobs(
		*State->mRepositoryT,
		PacketBlobWO->mPacket->data, LengthLimit, PacketBlobWO->mOffsetSize,
		PacketBlobWO->mPacket->data, LengthLimit, PacketBlobWO->mOffsetObject,
		BufferBlobLen, WrittenBlob.get())))
	{
		GS_GOTO_CLEAN();
	}

	// FIXME: using full size (PacketTree->dataLength) instead of LengthLimit of PacketTree (NOT of PacketBlob!)
	if (!!(r = clnt_deserialize_trees(
		*State->mRepositoryT,
		State->mTreePacketWithOffset->mPacket->data, State->mTreePacketWithOffset->mPacket->dataLength, State->mTreePacketWithOffset->mOffsetSize,
		State->mTreePacketWithOffset->mPacket->data, State->mTreePacketWithOffset->mPacket->dataLength, State->mTreePacketWithOffset->mOffsetObject,
		BufferTreeLen, WrittenTree.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_UPDATED_REF, a,
		{ a.mWrittenBlob = WrittenBlob;
		  a.mWrittenTree = WrittenTree; });

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

int clnt_state_crank2(struct GsCrankData *CrankData)
{
	int r = 0;

	struct GsStoreWorkerClient *pStoreWorker = (GsStoreWorkerClient *) CrankData->mStoreWorker;

	struct ClntState *State = pStoreWorker->mClntState.get();

	uint32_t Code = 0;

	if (pStoreWorker->base.magic != GS_STORE_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (!!(r = clnt_state_code(State, &Code)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "servicing [code=[%s]]", gs_clnt_state_code_to_name(Code));

	switch (Code) {

	case GS_CLNT_STATE_CODE_NEED_REPOSITORY:
	{
		if (!!(r = clnt_state_need_repository_setup2(
			State,
			pStoreWorker->mRepoMainPathBuf, pStoreWorker->mLenRepoMainPath)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_TREE_HEAD:
	{
		if (!!(r = clnt_state_need_tree_head_work(CrankData)))
			GS_GOTO_CLEAN();
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_TREELIST:
	{
		if (!!(r = clnt_state_need_treelist_work(CrankData)))
			GS_GOTO_CLEAN();
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_BLOBLIST:
	{
		if (!!(r = clnt_state_need_bloblist_work(CrankData)))
			GS_GOTO_CLEAN();
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE:
	{
		if (!!(r = clnt_state_need_written_blob_and_tree_work(CrankData)))
			GS_GOTO_CLEAN();
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_UPDATED_REF:
	{
		if (!!(r = clnt_state_need_updated_ref_setup2(
			pStoreWorker,
			State)))
		{
			GS_GOTO_CLEAN();
		}
	}

	case GS_CLNT_STATE_CODE_NEED_NOTHING:
	{
		int r2 = gs_helper_api_worker_exit(CrankData->mWorkerDataSend);
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

	if (!!(r = gs_extra_host_create_init(
		GS_EXTRA_HOST_CREATE_CLIENT_MAGIC,
		gs_extra_host_create_cb_create_t_client,
		gs_extra_host_create_cb_destroy_host_t_enet_host_destroy,
		gs_extra_host_create_cb_destroy_t_delete,
		&ExtraHostCreate->base)))
	{
		GS_GOTO_CLEAN();
	}

	ExtraHostCreate->mServPort = ServPort;
	ExtraHostCreate->mServHostNameBuf = ServHostNameBuf;
	ExtraHostCreate->mLenServHostName = LenServHostName;

	if (oExtraHostCreate)
		*oExtraHostCreate = ExtraHostCreate;

clean:
	if (!!r) {
		GS_DELETE(&ExtraHostCreate, GsExtraHostCreateClient);
	}

	return r;
}

int gs_store_ntwk_client_create(
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreNtwkClient **oStoreNtwk)
{
	int r = 0;

	struct GsStoreNtwkClient *StoreNtwk = new GsStoreNtwkClient();

	if (!!(r = gs_store_ntwk_init(
		GS_STORE_NTWK_CLIENT_MAGIC,
		gs_store_ntwk_cb_destroy_t_client,
		ConnectionCommon,
		&StoreNtwk->base)))
	{
		GS_GOTO_CLEAN();
	}
	
	if (oStoreNtwk)
		*oStoreNtwk = StoreNtwk;

clean:
	if (!!r) {
		GS_DELETE(&StoreNtwk, GsStoreNtwkClient);
	}

	return r;
}

int gs_store_ntwk_cb_destroy_t_client(struct GsStoreNtwk *StoreNtwk)
{
	struct GsStoreNtwkClient *pThis = (struct GsStoreNtwkClient *) StoreNtwk;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_STORE_NTWK_CLIENT_MAGIC);

	GS_DELETE_F(&pThis->base.mConnectionSurrogateMap, gs_connection_surrogate_map_destroy);

	GS_DELETE(&StoreNtwk, GsStoreNtwk);

	return 0;
}

int gs_store_worker_client_create(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreWorkerClient **oStoreWorker)
{
	int r = 0;

	struct GsStoreWorkerClient *StoreWorker = new GsStoreWorkerClient();

	uint32_t NumWorkers = 0;

	sp<ClntState> State(new ClntState());

	if (!!(r = gs_ctrl_con_get_num_workers(ConnectionCommon->mCtrlCon, &NumWorkers)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_state_make_default(State.get())))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_worker_init(
		GS_STORE_WORKER_CLIENT_MAGIC,
		gs_store_worker_cb_crank_t_client,
		gs_store_worker_cb_destroy_t_client,
		NumWorkers,
		ConnectionCommon,
		&StoreWorker->base)))
	{
		GS_GOTO_CLEAN();
	}
	
	StoreWorker->mRefNameMainBuf = RefNameMainBuf;
	StoreWorker->mLenRefNameMain = LenRefNameMain;
	StoreWorker->mRepoMainPathBuf = RepoMainPathBuf;
	StoreWorker->mLenRepoMainPath = LenRepoMainPath;
	StoreWorker->mClntState = State;

	if (oStoreWorker)
		*oStoreWorker = StoreWorker;

clean:
	if (!!r) {
		GS_DELETE(&StoreWorker, GsStoreWorkerClient);
	}

	return r;
}

int gs_store_worker_cb_destroy_t_client(struct GsStoreWorker *StoreWorker)
{
	struct GsStoreWorkerClient *pThis = (struct GsStoreWorkerClient *) StoreWorker;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_STORE_WORKER_CLIENT_MAGIC);

	GS_DELETE(&StoreWorker, GsStoreWorker);

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
	struct GsFullConnectionCommonData *ConnectionCommon = NULL;

	struct GsExtraHostCreateClient *ExtraHostCreate = NULL;
	struct GsStoreNtwkClient       *StoreNtwk       = NULL;
	struct GsStoreWorkerClient     *StoreWorker     = NULL;

	if (!!(r = gs_full_connection_common_data_create(GS_MAGIC_NUM_WORKER_THREADS, &ConnectionCommon)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_extra_host_create_client_create(ServPort, ServHostNameBuf, LenServHostName, &ExtraHostCreate)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_ntwk_client_create(ConnectionCommon, &StoreNtwk)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_worker_client_create(
		RefNameMainBuf, LenRefNameMain,
		RepoMainPathBuf, LenRepoMainPath,
		ConnectionCommon,
		&StoreWorker)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_net_full_create_connection(
		ServPort,
		GS_BASE_ARGOWN(&ExtraHostCreate),
		GS_BASE_ARGOWN(&StoreNtwk),
		GS_BASE_ARGOWN(&StoreWorker),
		GS_ARGOWN(&ConnectionCommon),
		&ConnectionClient,
		"clnt")))
	{
		GS_GOTO_CLEAN();
	}

	if (oConnectionClient)
		*oConnectionClient = ConnectionClient;

clean:
	if (!!r) {
		GS_DELETE_F(&ConnectionClient, gs_full_connection_destroy);
		GS_DELETE_BASE_VF(&StoreWorker, cb_destroy_t);
		GS_DELETE_BASE_VF(&StoreNtwk, cb_destroy_t);
		GS_DELETE_BASE_VF(&ExtraHostCreate, cb_destroy_t);
		GS_DELETE_F(&ConnectionCommon, gs_full_connection_common_data_destroy);
	}

	return r;
}

int gs_store_worker_cb_crank_t_client(struct GsCrankData *CrankData)
{
	int r = 0;

	while (true) {
		if (!!(r = clnt_state_crank2(CrankData)))
			GS_ERR_CLEAN(r);
	}

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

	struct GsHostSurrogate Host = {};

	gs_connection_surrogate_id_t AssignedId = 0;

	if (pThis->base.magic != GS_EXTRA_HOST_CREATE_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	/* create host */

	// FIXME: 128 peerCount, 1 channelLimit
	if (!!(r = gs_host_surrogate_setup_host_nobind(128, &Host)))
		GS_GOTO_CLEAN();

	/* create and register connection */

	if (!!(r = gs_host_surrogate_connect_wait_blocking_register(
		&Host,
		pThis->mServPort,
		pThis->mServHostNameBuf, pThis->mLenServHostName,
		ioConnectionSurrogateMap,
		&AssignedId)))
	{
		GS_GOTO_CLEAN();
	}

	/* output */

	for (uint32_t i = 0; i < LenExtraWorker; i++)
		if (!!(r = gs_extra_worker_client_create(&oExtraWorkerArr[i], AssignedId)))
			GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		*ioHostSurrogate = Host;

clean:

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

	GS_DELETE(&ExtraWorker, GsExtraWorker);

	return 0;
}
