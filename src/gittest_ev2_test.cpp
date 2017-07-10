#include <cassert>
#include <cstdlib>
#include <cstdio>

#include <thread>
#include <chrono>

#define EVENT2_VISIBILITY_STATIC_MSVC
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <gittest/misc.h>
#include <gittest/log.h>
#include <gittest/config.h>
#include <gittest/gittest.h>
#include <gittest/crank_clnt.h>

#include <gittest/gittest_ev2_test.h>

#define GS_EV_CTX_CLNT_MAGIC 0x4E8BF2AD 

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

struct GsEvCtxClnt
{
	uint32_t mMagic;
	struct GsAuxConfigCommonVars mCommonVars;
	struct ClntState *mClntState;
};

int gs_ev_clnt_state_crank3_connected(
	struct bufferevent *Bev,
	struct GsEvCtxClnt *Ctx)
{
	int r = 0;

	std::string Buffer;

	git_repository *RepositoryT = NULL;

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_REPOSITORY)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(Ctx->mCommonVars.RepoMainPathBuf, &RepositoryT)))
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

int gs_ev_clnt_state_crank3(
	struct bufferevent *Bev,
	struct GsEvCtxClnt *Ctx,
	struct GsEvData *Packet)
{
	int r = 0;

	uint32_t Code = 0;

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
		}

		Ctx->mClntState->mTreeHeadOid = TreeHeadOid;

		if (!!(r = clnt_state_code_ensure(Ctx->mClntState, GS_CLNT_STATE_CODE_NEED_TREELIST)))
			GS_GOTO_CLEAN();

		if (!!(r = aux_frame_full_write_request_latest_commit_tree(gs_bysize_cb_String, &BysizeBuffer)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_ev_evbuffer_write_frame(bufferevent_get_output(Bev), Buffer.data(), Buffer.size())))
			GS_GOTO_CLEAN();
	}
	break;
	default:
		GS_ASSERT(0);
	}

clean:

	return r;
}

int gs_ev_evbuffer_get_frame_try(
	struct evbuffer *Ev,
	const char **oDataOpt,
	size_t *oLenHdr,
	size_t *oLenDataOpt)
{
	int r = 0;

	const char Magic[] = "FRAME";
	size_t LenMagic = sizeof Magic - 1;

	const char *DataOpt = NULL;
	size_t LenHdr = 0;
	size_t LenData = 0;

	const char *DataH = (const char*) evbuffer_pullup(Ev, LenMagic + sizeof(uint32_t));
	if (DataH) {
		uint32_t FrameDataLen = 0;
		if (memcmp(Magic, DataH, LenMagic) != 0)
			GS_ERR_CLEAN(1);
		aux_LE_to_uint32(&FrameDataLen, DataH + LenMagic, sizeof(uint32_t));
		const char *DataF = (const char *) evbuffer_pullup(Ev, LenMagic + sizeof(uint32_t) + FrameDataLen);
		if (DataF) {
			DataOpt = DataF + LenMagic + sizeof(uint32_t);
			LenHdr = LenMagic + sizeof(uint32_t);
			LenData = FrameDataLen;
		}
	}

clean:

	if (oDataOpt)
		*oDataOpt = DataOpt;
	if (oLenHdr)
		*oLenHdr = LenHdr;
	if (oLenDataOpt)
		*oLenDataOpt = LenData;

	return r;
}

int gs_ev_evbuffer_write_frame(
	struct evbuffer *Ev,
	const char *Data,
	size_t LenData)
{
	int r = 0;

	const char Magic[] = "FRAME";
	size_t LenMagic = sizeof Magic - 1;

	char cFrameDataLen[sizeof(uint32_t)];

	aux_uint32_to_LE(LenData, cFrameDataLen, sizeof cFrameDataLen);

	if (!!(r = evbuffer_expand(Ev, LenMagic + sizeof cFrameDataLen + LenData)))
		GS_GOTO_CLEAN();

	if (!!(r = evbuffer_add(Ev, Magic, LenMagic)))
		GS_GOTO_CLEAN();

	if (!!(r = evbuffer_add(Ev, cFrameDataLen, sizeof cFrameDataLen)))
		GS_GOTO_CLEAN();

	if (!!(r = evbuffer_add(Ev, Data, LenData)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

static void bev_event_cb(struct bufferevent *Bev, short What, void *CtxClnt)
{
	int r = 0;
	struct GsEvCtxClnt *Ctx = (struct GsEvCtxClnt *) CtxClnt;
	GS_ASSERT(Ctx->mMagic == GS_EV_CTX_CLNT_MAGIC);

	if (What & BEV_EVENT_CONNECTED) {
		if (!!(r = gs_ev_clnt_state_crank3_connected(Bev, Ctx)))
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

static void bev_read_cb(struct bufferevent *Bev, void *CtxClnt)
{
	int r = 0;
	struct GsEvCtxClnt *Ctx = (struct GsEvCtxClnt *) CtxClnt;
	GS_ASSERT(Ctx->mMagic == GS_EV_CTX_CLNT_MAGIC);
	const char *Data = NULL;
	size_t LenHdr, LenData;
	if (!!(r = gs_ev_evbuffer_get_frame_try(bufferevent_get_input(Bev), &Data, &LenHdr, &LenData)))
		assert(0);
	if (Data) {
		struct GsEvData Packet = { (uint8_t *) Data, LenData };
		if (!!(r = gs_ev_clnt_state_crank3(Bev, Ctx, &Packet)))
			GS_GOTO_CLEAN();
		if (!!(r = evbuffer_drain(bufferevent_get_input(Bev), LenHdr + LenData)))
			GS_GOTO_CLEAN();
	}

clean:
	if (!!r)
		assert(0);
}

int gs_ev2_test_clntmain(struct GsAuxConfigCommonVars CommonVars)
{
	int r = 0;

	log_guard_t Log(GS_LOG_GET("selfup"));

	struct event_base *Base = NULL;
	struct bufferevent *Bev = NULL;
	struct event *Tev = NULL;

	struct GsEvCtxClnt *Ctx = new GsEvCtxClnt();

	if (!(Base = event_base_new()))
		assert(0);

	if (!(Bev = bufferevent_socket_new(Base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS)))
		assert(0);

	bufferevent_setcb(Bev, bev_read_cb, NULL, bev_event_cb, Ctx);

	if (!!(r = bufferevent_enable(Bev, EV_READ)))
		assert(0);

	Ctx->mMagic = GS_EV_CTX_CLNT_MAGIC;
	Ctx->mCommonVars = CommonVars;
	Ctx->mClntState = new ClntState();

	if (!!(r = clnt_state_make_default(Ctx->mClntState)))
		assert(0);

	if (!!(r = bufferevent_socket_connect_hostname(Bev, NULL, AF_INET, CommonVars.ServHostNameBuf, CommonVars.ServPort)))
		assert(0);

	if (!!(r = event_base_loop(Base, EVLOOP_NO_EXIT_ON_EMPTY)))
		assert(0);

	printf("exitingC\n");

clean:

	return r;
}

int main(int argc, char **argv)
{
	int r = 0;

	confmap_t KeyVal;
	struct GsAuxConfigCommonVars CommonVars = {};

	std::thread ThreadServ;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	/* NOTE: enet_initialize takes care of calling WSAStartup (needed for LibEvent) */
	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_crash_handler_setup()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_create_common_logs()))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_read_default_everything(&KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_get_common_vars(KeyVal, &CommonVars)))
		GS_GOTO_CLEAN();

	ThreadServ.swap(std::thread(gs_ev2_test_servmain, CommonVars));

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	if (!!(r = gs_ev2_test_clntmain(CommonVars)))
		GS_GOTO_CLEAN();

clean:
	if (!!r)
		GS_ASSERT(0);

	return EXIT_SUCCESS;
}
