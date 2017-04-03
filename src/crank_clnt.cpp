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
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State,
	const char *RefNameMainBuf, size_t LenRefNameMain)
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
		IdForSend,
		IntrToken,
		RefNameMainBuf, LenRefNameMain,
		RepositoryT,
		TreeHeadOid.get())))
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
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	git_repository *RepositoryT,
	git_oid *oTreeHeadOid)
{
	int r = 0;

	std::string Buffer;
	GsPacket *Packet = NULL;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = aux_frame_full_write_request_latest_commit_tree(gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_dequeue(WorkerDataRecv, &Packet, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, oTreeHeadOid->id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_latest_commit_tree_oid(RepositoryT, RefNameMainBuf, &CommitHeadOidT, &TreeHeadOidT)))
		GS_GOTO_CLEAN();

	if (git_oid_cmp(&TreeHeadOidT, oTreeHeadOid) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &CommitHeadOidT);
		GS_LOG(I, PF, "[clnt] Have latest [%.*s]\n", (int)GIT_OID_HEXSZ, buf);
	}

clean:

	return r;
}

int clnt_state_need_treelist_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State)
{
	int r = 0;

	sp<std::vector<git_oid> > Treelist(new std::vector<git_oid>);
	sp<std::vector<git_oid> > MissingTreelist(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<git_oid> &TreeHeadOid = State->mTreeHeadOid;

	if (!!(r = clnt_state_need_treelist_noown2(
		WorkerDataRecv,
		WorkerDataSend,
		IdForSend,
		IntrToken,
		RepositoryT,
		TreeHeadOid.get(),
		Treelist.get(),
		MissingTreelist.get())))
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
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	git_repository *RepositoryT,
	git_oid *TreeHeadOid,
	std::vector<git_oid> *oTreelist,
	std::vector<git_oid> *oMissingTreelist)
{
	int r = 0;

	std::string Buffer;
	GsPacket *Packet = NULL;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	GS_BYPART_DATA_VAR(OidVector, BypartTreelist);
	GS_BYPART_DATA_INIT(OidVector, BypartTreelist, oTreelist);

	if (!!(r = aux_frame_full_write_request_treelist(TreeHeadOid->id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_dequeue(WorkerDataRecv, &Packet, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREELIST))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartTreelist, gs_bypart_cb_OidVector)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_missing_trees(RepositoryT, oTreelist, oMissingTreelist)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_need_bloblist_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State)
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
		IdForSend,
		IntrToken,
		RepositoryT,
		MissingTreelist.get(),
		MissingBloblist.get(),
		&PacketTree,
		&OffsetSizeBufferTree,
		&OffsetObjectBufferTree)))
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
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	git_repository *RepositoryT,
	std::vector<git_oid> *MissingTreelist,
	std::vector<git_oid> *oMissingBloblist,
	struct GsPacket **oPacketTree,
	uint32_t *oOffsetSizeBufferTree,
	uint32_t *oOffsetObjectBufferTree)
{
	int r = 0;

	std::string Buffer;
	uint32_t Offset = 0;
	uint32_t LengthLimit = 0;

	GsPacket *PacketTree = NULL;

	GsStrided MissingTreelistStrided = {};

	uint32_t BufferTreeLen = 0;

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = gs_strided_for_oid_vec_cpp(MissingTreelist, &MissingTreelistStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_trees(MissingTreelistStrided, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketTree Lifetime start */

	if (!!(r = gs_worker_packet_dequeue(WorkerDataRecv, &PacketTree, NULL)))
		GS_GOTO_CLEAN();

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

	return r;
}

int clnt_state_need_written_blob_and_tree_setup2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State)
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
		IdForSend,
		IntrToken,
		RepositoryT,
		MissingTreelist.get(),
		MissingBloblist.get(),
		PacketTree,
		OffsetSizeBufferTree,
		OffsetObjectBufferTree,
		WrittenBlob.get(),
		WrittenTree.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State, GS_CLNT_STATE_CODE_NEED_NOTHING, a,
		{ a.mWrittenBlob = WrittenBlob;
		  a.mWrittenTree = WrittenTree; });

clean:

	return r;
}

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
	std::vector<git_oid> *oWrittenTree)
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

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = gs_strided_for_oid_vec_cpp(MissingBloblist, &MissingBloblistStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_blobs(MissingBloblistStrided, gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketBlob Lifetime start */

	if (!!(r = gs_worker_packet_dequeue(WorkerDataRecv, &PacketBlob, NULL)))
		GS_GOTO_CLEAN();

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

int clnt_state_crank2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	ClntState *State,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath)
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
			RepoMainPathBuf, LenRepoMainPath)))
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
			IdForSend,
			IntrToken,
			State,
			RefNameMainBuf, LenRefNameMain)))
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
			IdForSend,
			IntrToken,
			State)))
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
			IdForSend,
			IntrToken,
			State)))
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
			IdForSend,
			IntrToken,
			State)))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_NOTHING:
	{
		GS_ERR_CLEAN(GS_ERRCODE_EXIT);
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

int gs_net_full_create_connection_client(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	sp<GsFullConnection> *oConnectionClient)
{
	int r = 0;

	sp<GsFullConnection> ConnectionClient;

	ENetIntrTokenCreateFlags *IntrTokenFlags = NULL;
	GsIntrTokenSurrogate      IntrTokenSurrogate = {};

	sp<ClntState> State(new ClntState());

	GsCtrlCon               *CtrlCon = NULL;

	GsExtraHostCreateClient *ExtraHostCreate = new GsExtraHostCreateClient();
	GsStoreNtwkClient       *StoreNtwk       = new GsStoreNtwkClient();
	GsStoreWorkerClient     *StoreWorker     = new GsStoreWorkerClient();

	sp<GsExtraHostCreate> pExtraHostCreate(&ExtraHostCreate->base);
	sp<GsStoreNtwk>       pStoreNtwk(&StoreNtwk->base);
	sp<GsStoreWorker>     pStoreWorker(&StoreWorker->base);

	if (!(IntrTokenFlags = enet_intr_token_create_flags_create(ENET_INTR_DATA_TYPE_NONE)))
		GS_GOTO_CLEAN();

	if (!(IntrTokenSurrogate.mIntrToken = enet_intr_token_create(IntrTokenFlags)))
		GS_ERR_CLEAN(1);

	if (!!(r = clnt_state_make_default(State.get())))
		GS_GOTO_CLEAN();

	if (!!(r = gs_ctrl_con_create(&CtrlCon)))
		GS_GOTO_CLEAN();

	ExtraHostCreate->base.magic = GS_EXTRA_HOST_CREATE_CLIENT_MAGIC;
	ExtraHostCreate->base.cb_create_t = gs_extra_host_create_cb_create_t_client;
	ExtraHostCreate->mServPort = ServPort;
	ExtraHostCreate->mServHostNameBuf = ServHostNameBuf;
	ExtraHostCreate->mLenServHostName = LenServHostName;

	StoreNtwk->base.magic = GS_STORE_NTWK_CLIENT_MAGIC;
	StoreNtwk->base.mIntrTokenSurrogate = IntrTokenSurrogate;

	StoreWorker->base.magic = GS_STORE_WORKER_CLIENT_MAGIC;
	StoreWorker->base.cb_crank_t = gs_store_worker_cb_crank_t_client;
	StoreWorker->base.mCtrlCon = CtrlCon;
	StoreWorker->mRefNameMainBuf = RefNameMainBuf;
	StoreWorker->mLenRefNameMain = LenRefNameMain;
	StoreWorker->mRepoMainPathBuf = RepoMainPathBuf;
	StoreWorker->mLenRepoMainPath = LenRepoMainPath;
	StoreWorker->mIntrToken = IntrTokenSurrogate;
	StoreWorker->mClntState = State;

	if (!!(r = gs_net_full_create_connection(
		ServPort,
		pExtraHostCreate,
		pStoreNtwk,
		pStoreWorker,
		&ConnectionClient)))
	{
		GS_GOTO_CLEAN();
	}

	GS_SP_SET_RAW_NULLING(ConnectionClient->mCtrlCon, CtrlCon, GsCtrlCon);

	if (oConnectionClient)
		*oConnectionClient = ConnectionClient;

clean:

	return r;
}

int gs_store_worker_cb_crank_t_client(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker *ExtraWorker)
{
	int r = 0;

	GsStoreWorkerClient *pStoreWorker = (GsStoreWorkerClient *) StoreWorker;
	GsExtraWorkerClient *pExtraWorker = (GsExtraWorkerClient *) ExtraWorker;

	if (pStoreWorker->base.magic != GS_STORE_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	if (pExtraWorker->base.magic != GS_EXTRA_WORKER_CLIENT_MAGIC)
		GS_ERR_CLEAN(1);

	while (true) {

		if (!!(r = clnt_state_crank2(
			WorkerDataRecv,
			WorkerDataSend,
			pExtraWorker->mId,
			&pStoreWorker->mIntrToken,
			pStoreWorker->mClntState.get(),
			pStoreWorker->mRefNameMainBuf, pStoreWorker->mLenRefNameMain,
			pStoreWorker->mRepoMainPathBuf, pStoreWorker->mLenRepoMainPath)))
		{
			GS_ERR_NO_CLEAN(r);
		}

	}

noclean:

clean:

	return r;
}

int gs_extra_host_create_cb_create_t_client(
	GsExtraHostCreate *ExtraHostCreate,
	GsHostSurrogate *ioHostSurrogate,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsExtraWorker **oExtraWorker)
{
	int r = 0;

	struct GsExtraHostCreateClient *pThis = (struct GsExtraHostCreateClient *) ExtraHostCreate;

	ENetIntrHostCreateFlags FlagsHost = {};
	ENetHost *host = NULL;
	ENetAddress addr = {};
	ENetPeer *peer = NULL;
	ENetEvent event = {};

	GsConnectionSurrogate ConnectionSurrogate = {};

	gs_connection_surrogate_id_t AssignedId = 0;

	GsExtraWorker *ExtraWorker = NULL;

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
	ConnectionSurrogate.mIsPrincipalClientConnection = false;

	// FIXME: pretty ugly initialization of GsConnectionSurrogate
	if (!!(r = gs_aux_aux_aux_connection_register_transfer_ownership(
		ConnectionSurrogate,
		ioConnectionSurrogateMap,
		&AssignedId)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_extra_worker_cb_create_t_client(&ExtraWorker, AssignedId)))
		GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		ioHostSurrogate->mHost = host;

	if (oExtraWorker)
		*oExtraWorker = ExtraWorker;

clean:

	return r;
}

int gs_extra_worker_cb_create_t_client(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id)
{
	struct GsExtraWorkerClient * pThis = new GsExtraWorkerClient();

	pThis->base.magic = GS_EXTRA_WORKER_CLIENT_MAGIC;

	pThis->base.cb_create_t = gs_extra_worker_cb_create_t_client;
	pThis->base.cb_destroy_t = gs_extra_worker_cb_destroy_t_client;

	pThis->mId = Id;

	if (oExtraWorker)
		*oExtraWorker = &pThis->base;

	return 0;
}

int gs_extra_worker_cb_destroy_t_client(struct GsExtraWorker *ExtraWorker)
{
	if (ExtraWorker->magic != GS_EXTRA_WORKER_CLIENT_MAGIC)
		return -1;

	delete ExtraWorker;

	return 0;
}
