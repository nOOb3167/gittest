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
#include <gittest/crank_serv.h>

#include <gittest/gittest_ev2_test.h>

#define GS_EV_CTX_SERV_MAGIC 0x4E8BF2AD 

struct GsEvCtxServ
{
	uint32_t mMagic;
	struct GsAuxConfigCommonVars mCommonVars;

	git_repository *mRepository = NULL;
	git_repository *mRepositorySelfUpdate = NULL;
};

int gs_ev_serv_state_crank3_connected(
	struct bufferevent *Bev,
	struct GsEvCtxServ *Ctx)
{
	int r = 0;

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

	switch (FoundFrameType.mTypeNum) {
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
	GS_ASSERT(Ctx->mMagic == GS_EV_CTX_SERV_MAGIC);

	if (What & BEV_EVENT_CONNECTED) {
		if (!!(r = gs_ev_serv_state_crank3_connected(Bev, Ctx)))
			GS_GOTO_CLEAN();
	}
	if (What & BEV_EVENT_ERROR) {
		printf("%s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
		GS_ERR_CLEAN(1);
	}
	
	if (What & BEV_EVENT_ERROR || What & BEV_EVENT_EOF || What & BEV_EVENT_TIMEOUT) {
		if (!!(r = event_base_loopbreak(bufferevent_get_base(Bev))))
			GS_GOTO_CLEAN();
		GS_ERR_CLEAN(1);
	}

clean:
	if (!!r)
		assert(0);
}

static void bev_read_cb(struct bufferevent *Bev, void *CtxServ)
{
	int r = 0;
	struct GsEvCtxServ *Ctx = (struct GsEvCtxServ *) CtxServ;
	GS_ASSERT(Ctx->mMagic == GS_EV_CTX_SERV_MAGIC);
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

	GS_ASSERT(Ctx->mMagic == GS_EV_CTX_SERV_MAGIC);
	
	if (!(Bev = bufferevent_socket_new(Base, Fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS)))
		GS_ERR_CLEAN(1);

	bufferevent_setcb(Bev, bev_read_cb, NULL, bev_event_cb, Ctx);

	bufferevent_enable(Bev, EV_READ);

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

	Ctx->mMagic = GS_EV_CTX_SERV_MAGIC;
	Ctx->mCommonVars = CommonVars;

	if (!!(r = aux_repository_open(CommonVars.RepoMainPathBuf, &Ctx->mRepository)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(CommonVars.RepoSelfUpdatePathBuf, &Ctx->mRepositorySelfUpdate)))
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
