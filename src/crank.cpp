#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h>

#include <gittest/net.h>

#include <gittest/crank.h>

#include <../../gittest/src/net2.cpp>

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
	gs_packet_t Packet;
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
	sp<PacketUniqueWithOffset> PacketTreeWithOffset(new PacketUniqueWithOffset);

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
		GS_ASSERT(0);
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

int serv_state_service_request_blobs2(
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	GsPacket *Packet,
	uint32_t OffsetSize,
	git_repository *Repository,
	const GsFrameType &FrameTypeResponse)
{
	int r = 0;

	std::string ResponseBuffer;
	uint32_t Offset = OffsetSize;
	uint32_t LengthLimit = 0;
	std::vector<git_oid> BloblistRequested;
	std::string SizeBufferBlob;
	std::string ObjectBufferBlob;

	GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
	GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

	GS_BYPART_DATA_VAR(OidVector, BypartBloblistRequested);
	GS_BYPART_DATA_INIT(OidVector, BypartBloblistRequested, &BloblistRequested);

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartBloblistRequested, gs_bypart_cb_OidVector)))
		GS_GOTO_CLEAN();

	if (!!(r = serv_serialize_blobs(Repository, &BloblistRequested, &SizeBufferBlob, &ObjectBufferBlob)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_response_blobs(
		FrameTypeResponse, BloblistRequested.size(),
		(uint8_t *)SizeBufferBlob.data(), SizeBufferBlob.size(),
		(uint8_t *)ObjectBufferBlob.data(), ObjectBufferBlob.size(),
		gs_bysize_cb_String, &BysizeResponseBuffer)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int serv_state_crank2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsIntrTokenSurrogate *IntrToken,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath)
{
		int r = 0;

	git_repository *Repository = NULL;
	git_repository *RepositorySelfUpdate = NULL;

	if (!!(r = aux_repository_open(RepoMainPathBuf, &Repository)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoSelfUpdatePathBuf, &RepositorySelfUpdate)))
		GS_GOTO_CLEAN();

	while (true) {
		GsPacket *Packet = NULL;
		gs_connection_surrogate_id_t IdForSend = 0;

		GS_LOG(I, S, "waiting for request");

		if (!!(r = gs_worker_packet_dequeue(WorkerDataRecv, &Packet, &IdForSend)))
			GS_GOTO_CLEAN();

		uint32_t OffsetStart = 0;
		uint32_t OffsetSize = 0;

		GsFrameType FoundFrameType = {};

		if (!!(r = aux_frame_read_frametype(Packet->data, Packet->dataLength, OffsetStart, &OffsetSize, &FoundFrameType)))
			GS_GOTO_CLEAN();

		GS_LOG(I, PF, "servicing request [%.*s]", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);

		switch (FoundFrameType.mTypeNum)
		{
		case GS_FRAME_TYPE_REQUEST_LATEST_COMMIT_TREE:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid CommitHeadOid = {};
			git_oid TreeHeadOid = {};
			GS_OID_STR_VAR(TreeHeadOid);

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(Repository, RefNameMainBuf, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			GS_OID_STR_MAKE(TreeHeadOid);
			GS_LOG(I, PF, "latest commit tree [%s]", TreeHeadOidStr);

			if (!!(r = aux_frame_full_write_response_latest_commit_tree(TreeHeadOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		case GS_FRAME_TYPE_REQUEST_TREELIST:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid TreeOid = {};
			std::vector<git_oid> Treelist;
			GsStrided TreelistStrided = {};

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, TreeOid.id, GIT_OID_RAWSZ)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_oid_treelist(Repository, &TreeOid, &Treelist)))
				GS_GOTO_CLEAN();

			GS_LOG(I, PF, "listing trees [num=%d]", (int)Treelist.size());

			if (!!(r = gs_strided_for_oid_vec_cpp(&Treelist, &TreelistStrided)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_treelist(TreelistStrided, gs_bysize_cb_String, &BysizeResponseBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		case GS_FRAME_TYPE_REQUEST_TREES:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			uint32_t LengthLimit = 0;
			std::vector<git_oid> TreelistRequested;
			std::string SizeBufferTree;
			std::string ObjectBufferTree;

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			GS_BYPART_DATA_VAR(OidVector, BypartTreelistRequested);
			GS_BYPART_DATA_INIT(OidVector, BypartTreelistRequested, &TreelistRequested);

			if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartTreelistRequested, gs_bypart_cb_OidVector)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_serialize_trees(Repository, &TreelistRequested, &SizeBufferTree, &ObjectBufferTree)))
				GS_GOTO_CLEAN();

			GS_LOG(I, PF, "serializing trees [num=%d]", (int)TreelistRequested.size());

			if (!!(r = aux_frame_full_write_response_trees(
				TreelistRequested.size(),
				(uint8_t *)SizeBufferTree.data(), SizeBufferTree.size(),
				(uint8_t *)ObjectBufferTree.data(), ObjectBufferTree.size(),
				gs_bysize_cb_String, &BysizeResponseBuffer)))
			{
				GS_GOTO_CLEAN();
			}

			if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		case GS_FRAME_TYPE_REQUEST_BLOBS:
		{
			if (!!(r = serv_state_service_request_blobs2(
				WorkerDataSend,
				IdForSend,
				IntrToken,
				Packet,
				OffsetSize,
				Repository,
				GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_BLOBS_SELFUPDATE:
		{
			if (!!(r = serv_state_service_request_blobs2(
				WorkerDataSend,
				IdForSend,
				IntrToken,
				Packet,
				OffsetSize,
				RepositorySelfUpdate,
				GS_FRAME_TYPE_DECL(RESPONSE_BLOBS_SELFUPDATE))))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_LATEST_SELFUPDATE_BLOB:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid CommitHeadOid = {};
			git_oid TreeHeadOid = {};
			git_oid BlobSelfUpdateOid = {};

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(RepositorySelfUpdate, RefNameSelfUpdateBuf, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_oid_tree_blob_byname(RepositorySelfUpdate, &TreeHeadOid, GS_STR_PARENT_EXPECTED_SUFFIX, &BlobSelfUpdateOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_latest_selfupdate_blob(BlobSelfUpdateOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		default:
		{
			GS_LOG(E, PF, "[worker] unknown frametype received [%.*s]", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);
			if (1)
				GS_ERR_CLEAN(1);
		}
		break;
		}

	}

clean:
	if (RepositorySelfUpdate)
		git_repository_free(RepositorySelfUpdate);

	if (Repository)
		git_repository_free(Repository);

	return r;
}
