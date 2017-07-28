#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <sstream>

#include <git2.h>

#define EVENT2_VISIBILITY_STATIC_MSVC
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <gittest/misc.h>
#include <gittest/log.h>
#include <gittest/config.h>
#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h>
#include <gittest/crank_serv.h>

#include <gittest/gittest_ev2_test.h>

#define GS_EV_CTX_SERV_MAGIC 0x4E8BF2AD

struct GsEvCtxServ
{
	struct GsEvCtx base;
	struct GsAuxConfigCommonVars mCommonVars;

	git_repository *mRepository = NULL;
	git_repository *mRepositorySelfUpdate = NULL;
};

int gs_ev2_serv_state_service_request_blobs2(
	struct bufferevent *Bev,
	struct GsEvCtxServ *Ctx,
	struct GsEvData *Packet,
	uint32_t OffsetSize,
	git_repository *Repository,
	const struct GsFrameType FrameTypeResponse)
{
	int r = 0;

	std::string ResponseBuffer;
	uint32_t Offset = OffsetSize;
	uint32_t LengthLimit = 0;
	std::vector<git_oid> BloblistRequested;
	std::string SizeBufferBlob;
	std::string ObjectBufferBlob;

	size_t ServBlobSoftSizeLimit = Ctx->mCommonVars.ServBlobSoftSizeLimit;
	size_t NumUntilSizeLimit = 0;

	GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
	GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

	GS_BYPART_DATA_VAR(OidVector, BypartBloblistRequested);
	GS_BYPART_DATA_INIT(OidVector, BypartBloblistRequested, &BloblistRequested);

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartBloblistRequested, gs_bypart_cb_OidVector)))
		GS_GOTO_CLEAN();

	/* the client may be requesting many (and large) blobs
	   so we have a size limit (for transmission in one response) */

	if (!!(r = aux_objects_until_sizelimit(Repository, BloblistRequested.data(), BloblistRequested.size(), ServBlobSoftSizeLimit, &NumUntilSizeLimit)))
		GS_GOTO_CLEAN();

	/* if none made it under the size limit, make sure we send at least one, so that progress is made */

	if (NumUntilSizeLimit == 0)
		NumUntilSizeLimit = GS_CLAMP(BloblistRequested.size(), 0, 1);

	BloblistRequested.resize(NumUntilSizeLimit);

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

	if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), ResponseBuffer.data(), ResponseBuffer.size())))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_ev_serv_state_crank3_connected(
	struct bufferevent *Bev,
	struct GsEvCtxServ *Ctx)
{
	int r = 0;

	return r;
}

int gs_ev_serv_state_crank3_disconnected(
	struct bufferevent *Bev,
	struct GsEvCtxServ *Ctx,
	int DisconnectReason)
{
	int r = 0;

	bufferevent_free(Bev);

	return r;
}

int gs_ev_serv_state_crank3(
	struct bufferevent *Bev,
	struct GsEvCtxServ *Ctx,
	struct GsEvData *Packet)
{
	int r = 0;

	uint32_t OffsetStart = 0;
	uint32_t OffsetSize = 0;

	GsFrameType FoundFrameType = {};

	if (!!(r = aux_frame_read_frametype(Packet->data, Packet->dataLength, OffsetStart, &OffsetSize, &FoundFrameType)))
		GS_GOTO_CLEAN();

	switch (FoundFrameType.mTypeNum)
	{

	case GS_FRAME_TYPE_REQUEST_LATEST_COMMIT_TREE:
	{
		std::string ResponseBuffer;
		uint32_t Offset = OffsetSize;
		git_oid CommitHeadOid = {};
		git_oid TreeHeadOid = {};

		GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
		GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

		if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
			GS_GOTO_CLEAN();

		if (!!(r = serv_latest_commit_tree_oid(Ctx->mRepository, Ctx->mCommonVars.RefNameMainBuf, &CommitHeadOid, &TreeHeadOid)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_write_response_latest_commit_tree(TreeHeadOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), ResponseBuffer.data(), ResponseBuffer.size())))
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

		if (!!(r = serv_oid_treelist(Ctx->mRepository, &TreeOid, &Treelist)))
			GS_GOTO_CLEAN();

		GS_LOG(I, PF, "listing trees [num=%d]", (int)Treelist.size());

		if (!!(r = gs_strided_for_oid_vec_cpp(&Treelist, &TreelistStrided)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_write_response_treelist(TreelistStrided, gs_bysize_cb_String, &BysizeResponseBuffer)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), ResponseBuffer.data(), ResponseBuffer.size())))
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

		if (!!(r = serv_serialize_trees(Ctx->mRepository, &TreelistRequested, &SizeBufferTree, &ObjectBufferTree)))
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

		if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), ResponseBuffer.data(), ResponseBuffer.size())))
			GS_GOTO_CLEAN();
	}
	break;

	case GS_FRAME_TYPE_REQUEST_BLOBS:
	{
		if (!!(r = gs_ev2_serv_state_service_request_blobs2(
			Bev,
			Ctx,
			Packet,
			OffsetSize,
			Ctx->mRepository,
			GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
		{
			GS_GOTO_CLEAN();
		}
	}
	break;

	case GS_FRAME_TYPE_REQUEST_BLOBS_SELFUPDATE:
	{
		if (!!(r = gs_ev2_serv_state_service_request_blobs2(
			Bev,
			Ctx,
			Packet,
			OffsetSize,
			Ctx->mRepositorySelfUpdate,
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

		if (!!(r = serv_latest_commit_tree_oid(Ctx->mRepositorySelfUpdate, Ctx->mCommonVars.RefNameSelfUpdateBuf, &CommitHeadOid, &TreeHeadOid)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_oid_tree_blob_byname(Ctx->mRepositorySelfUpdate, &TreeHeadOid, Ctx->mCommonVars.SelfUpdateBlobNameBuf, &BlobSelfUpdateOid)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_write_response_latest_selfupdate_blob(BlobSelfUpdateOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), ResponseBuffer.data(), ResponseBuffer.size())))
			GS_GOTO_CLEAN();
	}
	break;

	default:
		GS_ASSERT(0);
	}

clean:

	return r;
}

static void bev_event_cb(struct bufferevent *Bev, short What, void *CtxServ)
{
	int r = 0;

	struct GsEvCtxServ *Ctx = (struct GsEvCtxServ *) CtxServ;

	int DisconnectReason = 0;

	GS_ASSERT(Ctx->base.mMagic == GS_EV_CTX_SERV_MAGIC);

	if (What & BEV_EVENT_CONNECTED) {
		if (!!(r = gs_ev_serv_state_crank3_connected(Bev, Ctx)))
			GS_GOTO_CLEAN();
	}
	else {
		if (What & BEV_EVENT_EOF)
			DisconnectReason = GS_DISCONNECT_REASON_EOF;
		else if (What & BEV_EVENT_TIMEOUT)
			DisconnectReason = GS_DISCONNECT_REASON_TIMEOUT;
		else if (What & BEV_EVENT_ERROR)
			DisconnectReason = GS_DISCONNECT_REASON_ERROR;

		if (!!(r = gs_ev_serv_state_crank3_disconnected(Bev, Ctx, DisconnectReason)))
			GS_GOTO_CLEAN();

		printf("[beverr=[%s]]\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));

		// FIXME: possibly loopbreak on fatal error / BEV_EVENT_ERROR
		// if (!!(r = event_base_loopbreak(bufferevent_get_base(Bev))))
		//	GS_GOTO_CLEAN();
	}
	
clean:
	if (!!r)
		assert(0);
}

static void bev_read_cb(struct bufferevent *Bev, void *CtxServ)
{
	int r = 0;
	struct GsEvCtxServ *Ctx = (struct GsEvCtxServ *) CtxServ;
	GS_ASSERT(Ctx->base.mMagic == GS_EV_CTX_SERV_MAGIC);
	const char *Data = NULL;
	size_t LenHdr, LenData;
	if (!!(r = gs_ev_evbuffer_get_frame_try(bufferevent_get_input(Bev), &Data, &LenHdr, &LenData)))
		assert(0);
	if (Data) {
		struct GsEvData Packet = { (uint8_t *) Data, LenData };
		if (!!(r = gs_ev_serv_state_crank3(Bev, Ctx, &Packet)))
			GS_GOTO_CLEAN();
		if (!!(r = evbuffer_drain(bufferevent_get_input(Bev), LenHdr + LenData)))
			GS_GOTO_CLEAN();
	}

clean:
	if (!!r)
		assert(0);
}

static void evc_listener_cb(struct evconnlistener *Listener, evutil_socket_t Fd, struct sockaddr *Addr, int AddrLen, void *CtxServ)
{
	int r = 0;

	struct GsEvCtxServ *Ctx = (struct GsEvCtxServ *) CtxServ;

	struct event_base *Base = evconnlistener_get_base(Listener);
	struct bufferevent *Bev = NULL;
	struct timeval Timeout = {};
	Timeout.tv_sec = GS_EV_TIMEOUT_SEC;
	Timeout.tv_usec = 0;

	GS_ASSERT(Ctx->base.mMagic == GS_EV_CTX_SERV_MAGIC);
	
	if (!(Bev = bufferevent_socket_new(Base, Fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS)))
		GS_ERR_CLEAN(1);

	bufferevent_setcb(Bev, bev_read_cb, NULL, bev_event_cb, Ctx);

	if (!!(r = bufferevent_set_timeouts(Bev, &Timeout, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = bufferevent_enable(Bev, EV_READ)))
		GS_GOTO_CLEAN();

clean:
	if (!!r)
		GS_ASSERT(0);
}

static void evc_error_cb(struct evconnlistener *Listener, void *CtxServ)
{
	struct event_base *Base = evconnlistener_get_base(Listener);

	GS_LOG(E, S, "Listener failure");

	if (!!(event_base_loopbreak(Base)))
		GS_ASSERT(0);
}

int gs_ev2_test_servmain(struct GsAuxConfigCommonVars CommonVars)
{
	int r = 0;

	log_guard_t Log(GS_LOG_GET("serv"));

	struct addrinfo Hints = {};
	struct addrinfo *ServInfo = NULL;
	struct sockaddr *ServAddr = NULL;

	struct event_base *Base = NULL;
	struct evconnlistener *Listener = NULL;

	struct GsEvCtxServ *Ctx = new GsEvCtxServ();

	std::stringstream ss;
	ss << CommonVars.ServPort;
	std::string cServPort = ss.str();

	Ctx->base.mMagic = GS_EV_CTX_SERV_MAGIC;
	Ctx->mCommonVars = CommonVars;

	if (!!(r = aux_repository_open(CommonVars.RepoMainPathBuf, CommonVars.LenRepoMainPath, &Ctx->mRepository)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(CommonVars.RepoSelfUpdatePathBuf, CommonVars.LenRepoSelfUpdatePath, &Ctx->mRepositorySelfUpdate)))
		GS_GOTO_CLEAN();

	if (!(Base = event_base_new()))
		GS_ERR_CLEAN(1);

	Hints.ai_flags = AI_PASSIVE; /* for NULL nodename in getaddrinfo */
	Hints.ai_family = AF_INET;
	Hints.ai_socktype = SOCK_STREAM;

	if (!!(r = getaddrinfo(NULL, cServPort.c_str(), &Hints, &ServInfo)))
		GS_GOTO_CLEAN();

	if (!(Listener = evconnlistener_new_bind(
		Base,
		evc_listener_cb,
		Ctx,
		LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_REUSEABLE,
		-1,
		ServInfo->ai_addr,
		ServInfo->ai_addrlen)))
	{
		GS_ERR_CLEAN(1);
	}
	evconnlistener_set_error_cb(Listener, evc_error_cb);

	if (!!(r = event_base_loop(Base, EVLOOP_NO_EXIT_ON_EMPTY)))
		GS_GOTO_CLEAN();

	printf("exitingS\n");

clean:
	freeaddrinfo(ServInfo);
	
	return r;
}
