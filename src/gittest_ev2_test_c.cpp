#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>

#include <thread>
#include <chrono>

#include <git2.h>

#define EVENT2_VISIBILITY_STATIC_MSVC
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <gittest/misc.h>
#include <gittest/bypart_git.h>
#include <gittest/log.h>
#include <gittest/config.h>
#include <gittest/gittest.h>
#include <gittest/crank_clnt.h>

#include <gittest/gittest_ev2_test.h>

static int gs_ev_clnt_state_crank3_connected(
	struct bufferevent *Bev,
	struct GsEvCtx *CtxBase);
static int gs_ev_clnt_state_crank3_disconnected(
	struct bufferevent *Bev,
	struct GsEvCtx *Ctx,
	int DisconnectReason);
static int gs_ev_clnt_state_crank3(
	struct bufferevent *Bev,
	struct GsEvCtx *CtxBase,
	struct GsEvData *Packet);

int gs_ev_clnt_state_crank3_connected(
	struct bufferevent *Bev,
	struct GsEvCtx *CtxBase)
{
	int r = 0;

	struct GsEvCtxClnt *Ctx = (struct GsEvCtxClnt *) CtxBase;

	std::string Buffer;

	git_repository *RepositoryT = NULL;

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	GS_ASSERT(Ctx->base.mMagic == GS_EV_CTX_CLNT_MAGIC);

	if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_REPOSITORY)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(Ctx->mCommonVars.RepoMasterUpdatePathBuf, Ctx->mCommonVars.LenRepoMasterUpdatePath, &RepositoryT)))
		GS_GOTO_CLEAN();

	Ctx->mClntState->mRepositoryT = sp<git_repository *>(new git_repository *(RepositoryT));

	if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_TREE_HEAD)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_latest_commit_tree(gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), Buffer.data(), Buffer.size())))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_ev_clnt_state_crank3_disconnected(
	struct bufferevent *Bev,
	struct GsEvCtx *Ctx,
	int DisconnectReason)
{
	int r = 0;

	bufferevent_free(Bev);

	GS_LOG(I, S, "crank3 disconnected, err-trigger");

	r = 1;

	return r;
}

int gs_ev_clnt_state_crank3(
	struct bufferevent *Bev,
	struct GsEvCtx *CtxBase,
	struct GsEvData *Packet)
{
	int r = 0;

	struct GsEvCtxClnt *Ctx = (struct GsEvCtxClnt *) CtxBase;

	uint32_t Code = 0;

process_another_state_label:

	if (!!(r = clnt_state_code(Ctx->mClntState, &Code)))
		GS_GOTO_CLEAN();

	switch (Code) {
	case GS_CLNT_STATE_CODE_NEED_TREE_HEAD:
	{
		sp<git_oid> TreeHeadOid(new git_oid);

		std::string Buffer;
		uint32_t Offset = 0;

		git_oid CommitHeadOidT = {};
		git_oid TreeHeadOidT = {};

		GS_BYPART_DATA_VAR(String, BysizeBuffer);
		GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

		if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE))))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, TreeHeadOid->id, GIT_OID_RAWSZ)))
			GS_GOTO_CLEAN();

		if (!!(r = clnt_latest_commit_tree_oid(*Ctx->mClntState->mRepositoryT, Ctx->mCommonVars.RefNameMainBuf, &CommitHeadOidT, &TreeHeadOidT)))
			GS_GOTO_CLEAN();

		if (git_oid_cmp(&TreeHeadOidT, TreeHeadOid.get()) == 0) {
			char buf[GIT_OID_HEXSZ] = {};
			git_oid_fmt(buf, &CommitHeadOidT);
			GS_LOG(I, PF, "Have latest [%.*s]\n", (int)GIT_OID_HEXSZ, buf);

			// FIXME: delegate to cleansub (nocleansub)
			GS_ERR_NO_CLEAN(GS_ERRCODE_EXIT);
		}

		Ctx->mClntState->mTreeHeadOid = TreeHeadOid;

		if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_TREELIST)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_write_request_treelist(TreeHeadOid->id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeBuffer)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), Buffer.data(), Buffer.size())))
			GS_GOTO_CLEAN();
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_TREELIST:
	{
		sp<std::vector<git_oid> > Treelist(new std::vector<git_oid>);
		sp<std::vector<git_oid> > MissingTreelist(new std::vector<git_oid>);

		struct GsStrided MissingTreelistStrided = {};

		std::string Buffer;
		uint32_t Offset = 0;
		uint32_t LengthLimit = 0;

		GS_BYPART_DATA_VAR(String, BysizeBuffer);
		GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

		GS_BYPART_DATA_VAR(OidVector, BypartTreelist);
		GS_BYPART_DATA_INIT(OidVector, BypartTreelist, Treelist.get());

		if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREELIST))))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartTreelist, gs_bypart_cb_OidVector)))
			GS_GOTO_CLEAN();

		if (!!(r = clnt_missing_trees(*Ctx->mClntState->mRepositoryT, Treelist.get(), MissingTreelist.get())))
			GS_GOTO_CLEAN();

		Ctx->mClntState->mTreelist = Treelist;
		Ctx->mClntState->mMissingTreelist = MissingTreelist;

		if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_BLOBLIST)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_strided_for_oid_vec_cpp(MissingTreelist.get(), &MissingTreelistStrided)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_write_request_trees(MissingTreelistStrided, gs_bysize_cb_String, &BysizeBuffer)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), Buffer.data(), Buffer.size())))
			GS_GOTO_CLEAN();
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_BLOBLIST:
	{
		sp<std::vector<git_oid> > MissingBloblist(new std::vector<git_oid>);
		sp<GsPacketWithOffset> PacketTreeWO(new GsPacketWithOffset);

		PacketTreeWO->mPacket = new GsPacket();
		PacketTreeWO->mPacket->data = new uint8_t[Packet->dataLength];
		PacketTreeWO->mPacket->dataLength = Packet->dataLength;
		memmove(PacketTreeWO->mPacket->data, Packet->data, Packet->dataLength);

		struct GsStrided MissingBloblistStrided = {};

		std::string Buffer;
		uint32_t Offset = 0;
		uint32_t LengthLimit = 0;

		uint32_t BufferTreeLen = 0;

		GS_BYPART_DATA_VAR(String, BysizeBuffer);
		GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

		if (!!(r = aux_frame_ensure_frametype(PacketTreeWO->mPacket->data, PacketTreeWO->mPacket->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREES))))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_read_size_limit(PacketTreeWO->mPacket->data, PacketTreeWO->mPacket->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketTreeWO->mPacket->data, LengthLimit, Offset, &Offset,
			NULL, &PacketTreeWO->mOffsetSize, &PacketTreeWO->mOffsetObject)))
		{
			GS_GOTO_CLEAN();
		}

		if (!!(r = gs_packet_with_offset_get_veclen(PacketTreeWO.get(), &BufferTreeLen)))
			GS_GOTO_CLEAN();

		// FIXME: proper handling for this condition / malformed request or response
		//   presumably server did not send all the requested trees
		GS_ASSERT(BufferTreeLen == Ctx->mClntState->mMissingTreelist->size());

		if (!!(r = clnt_missing_blobs_bare(
			*Ctx->mClntState->mRepositoryT,
			PacketTreeWO->mPacket->data, LengthLimit, PacketTreeWO->mOffsetSize,
			PacketTreeWO->mPacket->data, LengthLimit, PacketTreeWO->mOffsetObject,
			BufferTreeLen,
			MissingBloblist.get())))
		{
			GS_GOTO_CLEAN();
		}

		Ctx->mClntState->mMissingBloblist = MissingBloblist;
		Ctx->mClntState->mTreePacketWithOffset = PacketTreeWO;

		if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_strided_for_oid_vec_cpp(Ctx->mClntState->mMissingBloblist.get(), &MissingBloblistStrided)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_write_request_blobs(MissingBloblistStrided, gs_bysize_cb_String, &BysizeBuffer)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), Buffer.data(), Buffer.size())))
			GS_GOTO_CLEAN();
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE:
	{
		sp<std::vector<git_oid> > WrittenBlob(new std::vector<git_oid>);
		sp<std::vector<git_oid> > WrittenTree(new std::vector<git_oid>);
		sp<GsPacketWithOffset> PacketBlobWO(new GsPacketWithOffset);

		// FIXME: leak
		PacketBlobWO->mPacket = new GsPacket();
		PacketBlobWO->mPacket->data = new uint8_t[Packet->dataLength];
		PacketBlobWO->mPacket->dataLength = Packet->dataLength;
		memmove(PacketBlobWO->mPacket->data, Packet->data, Packet->dataLength);

		std::string Buffer;
		uint32_t Offset = 0;
		uint32_t LengthLimit = 0;

		struct GsStrided MissingBloblistStrided = {};

		uint32_t BufferBlobLen = 0;
		uint32_t BufferTreeLen = 0;

		GS_BYPART_DATA_VAR(String, BysizeBuffer);
		GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

		if (!!(r = aux_frame_ensure_frametype(PacketBlobWO->mPacket->data, PacketBlobWO->mPacket->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_read_size_limit(PacketBlobWO->mPacket->data, PacketBlobWO->mPacket->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketBlobWO->mPacket->data, LengthLimit, Offset, &Offset,
			NULL, &PacketBlobWO->mOffsetSize, &PacketBlobWO->mOffsetObject)))
		{
			GS_GOTO_CLEAN();
		}

		if (!!(r = gs_packet_with_offset_get_veclen(PacketBlobWO.get(), &BufferBlobLen)))
			GS_GOTO_CLEAN();

		GS_ASSERT(Ctx->mClntState->mMissingBloblist && Ctx->mClntState->mMissingTreelist);
		GS_ASSERT(BufferBlobLen <= Ctx->mClntState->mMissingBloblist->size() - Ctx->mClntState->mWrittenBlob->size());

		if (!!(r = clnt_deserialize_blobs(
			*Ctx->mClntState->mRepositoryT,
			PacketBlobWO->mPacket->data, LengthLimit, PacketBlobWO->mOffsetSize,
			PacketBlobWO->mPacket->data, LengthLimit, PacketBlobWO->mOffsetObject,
			BufferBlobLen, WrittenBlob.get())))
		{
			GS_GOTO_CLEAN();
		}

		for (size_t i = 0; i < WrittenBlob->size(); i++)
			Ctx->mClntState->mWrittenBlob->push_back((*WrittenBlob)[i]);

		if (Ctx->mClntState->mWrittenBlob->size() < Ctx->mClntState->mMissingBloblist->size()) {
			/* not all blobs transferred yet - staying in this state, requesting more blobs */

			/* how many blobs per response the server is able to send is determined from number of blobs written during response handling (ie. here).
			   we limit ourselves to, in our next message, requesting twice as many as we've been sent here. */
			uint32_t NumAddedToWrittenThisTime = WrittenBlob->size();
			uint32_t NumNotYetInWritten = Ctx->mClntState->mMissingBloblist->size() - Ctx->mClntState->mWrittenBlob->size();
			uint32_t NumToRequest = GS_MIN(NumAddedToWrittenThisTime * 2, NumNotYetInWritten);

			std::vector<git_oid> BlobsToRequest;
			struct GsStrided BlobsToRequestStrided = {};

			for (size_t i = 0; i < NumToRequest; i++)
				BlobsToRequest.push_back((*Ctx->mClntState->mMissingBloblist)[Ctx->mClntState->mWrittenBlob->size() + i]);

			if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_strided_for_oid_vec_cpp(&BlobsToRequest, &BlobsToRequestStrided)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_request_blobs(BlobsToRequestStrided, gs_bysize_cb_String, &BysizeBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), Buffer.data(), Buffer.size())))
				GS_GOTO_CLEAN();
		}
		else {
			/* all blobs transferred - moving state forward */

			if (!!(r = gs_packet_with_offset_get_veclen(Ctx->mClntState->mTreePacketWithOffset.get(), &BufferTreeLen)))
				GS_GOTO_CLEAN();

			GS_ASSERT(BufferTreeLen == Ctx->mClntState->mMissingTreelist->size());

			// FIXME: using full size (PacketTree->dataLength) instead of LengthLimit of PacketTree (NOT of PacketBlob!)
			if (!!(r = clnt_deserialize_trees(
				*Ctx->mClntState->mRepositoryT,
				Ctx->mClntState->mTreePacketWithOffset->mPacket->data, Ctx->mClntState->mTreePacketWithOffset->mPacket->dataLength, Ctx->mClntState->mTreePacketWithOffset->mOffsetSize,
				Ctx->mClntState->mTreePacketWithOffset->mPacket->data, Ctx->mClntState->mTreePacketWithOffset->mPacket->dataLength, Ctx->mClntState->mTreePacketWithOffset->mOffsetObject,
				BufferTreeLen, WrittenTree.get())))
			{
				GS_GOTO_CLEAN();
			}

			/* unlike the others, this state uses reverse convention (initialized as default, cleared for transition) */
			Ctx->mClntState->mWrittenBlob.reset();
			Ctx->mClntState->mWrittenTree.reset();

			if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_UPDATED_REF)))
				GS_GOTO_CLEAN();

			/* no frame needs to be sent */
			goto process_another_state_label;
		}
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_UPDATED_REF:
	{
		sp<git_oid> UpdatedRefOid(new git_oid());

		git_oid CommitOid = {};

		if (!!(r = gs_buf_ensure_haszero(Ctx->mCommonVars.RefNameMainBuf, Ctx->mCommonVars.LenRefNameMain + 1)))
			GS_GOTO_CLEAN();

		if (!!(r = clnt_commit_ensure_dummy(*Ctx->mClntState->mRepositoryT, Ctx->mClntState->mTreeHeadOid.get(), &CommitOid)))
			GS_GOTO_CLEAN();

		if (!!(r = clnt_commit_setref(*Ctx->mClntState->mRepositoryT, Ctx->mCommonVars.RefNameMainBuf, &CommitOid)))
			GS_GOTO_CLEAN();

		Ctx->mClntState->mUpdatedRefOid = UpdatedRefOid;

		if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_NOTHING)))
			GS_GOTO_CLEAN();

		goto process_another_state_label;
	}
	break;

	case GS_CLNT_STATE_CODE_NEED_NOTHING:
	{
		GS_ERR_NO_CLEAN(GS_ERRCODE_EXIT);
	}
	break;

	default:
		GS_ASSERT(0);
	}

noclean:

clean:

	return r;
}

int gs_ev_ctx_clnt_destroy(struct GsEvCtxClnt *w)
{
	if (w)
		delete w;
	return 0;
}

int gs_ev2_test_clntmain(
	struct GsAuxConfigCommonVars CommonVars,
	struct GsEvCtxClnt **oCtx)
{
	int r = 0;

	struct GsEvCtxClnt *Ctx = new GsEvCtxClnt();

	Ctx->base.mMagic = GS_EV_CTX_CLNT_MAGIC;
	Ctx->base.mIsError = 0;
	Ctx->base.CbConnect = gs_ev_clnt_state_crank3_connected;
	Ctx->base.CbDisconnect = gs_ev_clnt_state_crank3_disconnected;
	Ctx->base.CbCrank = gs_ev_clnt_state_crank3;
	Ctx->mCommonVars = CommonVars;
	Ctx->mClntState = new ClntState();

	if (!!(r = clnt_state_make_default(Ctx->mClntState)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_ev2_connect(&Ctx->base,
		CommonVars.ServHostNameBuf, CommonVars.LenServHostName,
		CommonVars.ServPort)))
	{
		GS_ERR_NO_CLEAN(0);
	}

	GS_LOG(I, S, "exiting");

noclean:

	if (oCtx)
		*oCtx = Ctx;

clean:

	return r;
}

