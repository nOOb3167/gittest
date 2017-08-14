#ifndef _GITTEST_EV2_TEST_H_
#define _GITTEST_EV2_TEST_H_

#include <cstdint>

#define EVENT2_VISIBILITY_STATIC_MSVC
#include <event2/bufferevent.h>

#include <gittest/config.h>
#include <gittest/crank_clnt.h>

#define GS_EV_TIMEOUT_SEC (30 * 20)

#define GS_DISCONNECT_REASON_ERROR BEV_EVENT_ERROR
#define GS_DISCONNECT_REASON_EOF BEV_EVENT_EOF
#define GS_DISCONNECT_REASON_TIMEOUT BEV_EVENT_TIMEOUT

#define GS_EV_CTX_CLNT_MAGIC 0x4E8BF2AD
#define GS_EV_CTX_SERV_MAGIC 0x4E8BF2AE
#define GS_EV_CTX_SELFUPDATE_MAGIC 0x4E8BF2AF

struct GsEvData
{
	uint8_t *data;
	size_t   dataLength;
};

struct GsEvCtx
{
	uint32_t mMagic;

	int mIsError;

	int (*CbConnect)(
		struct bufferevent *Bev,
		struct GsEvCtx *CtxBase);
	int (*CbDisconnect)(
		struct bufferevent *Bev,
		struct GsEvCtx *CtxBase,
		int DisconnectReason);
	int (*CbCrank)(
		struct bufferevent *Bev,
		struct GsEvCtx *CtxBase,
		struct GsEvData *Packet);
};

struct GsEvCtxClnt
{
	struct GsEvCtx base;
	struct GsAuxConfigCommonVars mCommonVars;
	struct ClntState *mClntState;
};

enum gs_selfupdate_state_code_t {
	GS_SELFUPDATE_STATE_CODE_NEED_REPOSITORY = 0,
	GS_SELFUPDATE_STATE_CODE_NEED_BLOB_HEAD = 1,
	GS_SELFUPDATE_STATE_CODE_NEED_BLOB = 2,
	GS_SELFUPDATE_STATE_CODE_NEED_NOTHING = 3,
	GS_SELFUPDATE_STATE_CODE_MAX_ENUM = 0x7FFFFFFF,
};

struct GsSelfUpdateState
{
	sp<git_repository *> mRepositoryT;
	sp<git_repository *> mRepositoryMemory;
	sp<git_oid>          mBlobHeadOid;
	sp<std::string>      mBufferUpdate;
};

struct GsEvCtxSelfUpdate
{
	struct GsEvCtx base;
	struct GsAuxConfigCommonVars mCommonVars;
	const char *mCurExeBuf; size_t mLenCurExe;
	struct GsSelfUpdateState *mState;
};

int gs_ev_ctx_clnt_destroy(struct GsEvCtxClnt *w);
int gs_ev_ctx_selfupdate_destroy(struct GsEvCtxSelfUpdate *w);

int gs_selfupdate_state_code(
	struct GsSelfUpdateState *State,
	uint32_t *oCode);
int gs_selfupdate_state_code_ensure(
	struct GsSelfUpdateState *State,
	uint32_t WantedCode);

int gs_ev2_test_clntmain(
	struct GsAuxConfigCommonVars CommonVars,
	struct GsEvCtxClnt **oCtx);
int gs_ev2_test_servmain(struct GsAuxConfigCommonVars CommonVars);
int gs_ev2_test_selfupdatemain(
	struct GsAuxConfigCommonVars CommonVars,
	const char *CurExeBuf, size_t LenCurExe,
	struct GsEvCtxSelfUpdate **oCtx);

/* common */

int gs_ev_evbuffer_get_frame_try(
	struct evbuffer *Ev,
	const char **oDataOpt,
	size_t *oLenHdr,
	size_t *oLenDataOpt);
int gs_ev_evbuffer_write_frame(
	struct evbuffer *Ev,
	const char *Data,
	size_t LenData);

void bev_event_cb(struct bufferevent *Bev, short What, void *CtxBaseV);
void bev_read_cb(struct bufferevent *Bev, void *CtxBaseV);

void evc_listener_cb(struct evconnlistener *Listener, evutil_socket_t Fd, struct sockaddr *Addr, int AddrLen, void *CtxBaseV);
void evc_error_cb(struct evconnlistener *Listener, void *CtxBaseV);

int gs_ev2_listen(
	struct GsEvCtx *CtxBase,
	uint32_t ServPortU32);
int gs_ev2_connect(
struct GsEvCtx *CtxBase,
	const char *ConnectHostNameBuf, size_t LenConnectHostName,
	uint32_t ConnectPort);
#endif /* _GITTEST_EV2_TEST_H_ */
