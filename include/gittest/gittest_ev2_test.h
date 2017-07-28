#ifndef _GITTEST_EV2_TEST_H_
#define _GITTEST_EV2_TEST_H_

#include <cstdint>

#include <event2/bufferevent.h>

#define GS_EV_TIMEOUT_SEC 30

#define GS_DISCONNECT_REASON_ERROR BEV_EVENT_ERROR
#define GS_DISCONNECT_REASON_EOF BEV_EVENT_EOF
#define GS_DISCONNECT_REASON_TIMEOUT BEV_EVENT_TIMEOUT

struct GsEvData
{
	uint8_t *data;
	size_t   dataLength;
};

struct GsEvCtx
{
	uint32_t mMagic;

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

int gs_ev_evbuffer_get_frame_try(
	struct evbuffer *Ev,
	const char **oDataOpt,
	size_t *oLenHdr,
	size_t *oLenDataOpt);
int gs_ev_evbuffer_write_frame(
	struct evbuffer *Ev,
	const char *Data,
	size_t LenData);

int gs_ev2_test_servmain(struct GsAuxConfigCommonVars CommonVars);

#endif /* _GITTEST_EV2_TEST_H_ */
