#include <cassert>
#include <cstdlib>
#include <cstdio>

#define EVENT2_VISIBILITY_STATIC_MSVC
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#define GS_EV_CTX_CLNT_MAGIC 0x4E8BF2AD 

struct GsEvCtxClnt
{
	uint32_t mMagic;
	struct event_base *mBase;
};

void aux_LE_to_uint32(uint32_t *oA, const char *buf, size_t bufsize)
{
	assert(sizeof(uint32_t) == 4 && bufsize == 4);
	uint32_t w = 0;
	w |= (buf[0] & 0xFF) << 0;
	w |= (buf[1] & 0xFF) << 8;
	w |= (buf[2] & 0xFF) << 16;
	w |= (buf[3] & 0xFF) << 24;
	*oA = w;
}

int gs_ev_evbuffer_get_frame_try(
	struct evbuffer *Ev,
	const char **oDataOpt,
	size_t *oLenHdr,
	size_t *oLenDataOpt)
{
	const char *DataOpt = NULL;
	size_t LenHdr = 0;
	size_t LenData = 0;

	size_t LenMagic = 5;
	const char *DataH = (const char*) evbuffer_pullup(Ev, LenMagic + sizeof(uint32_t));
	if (DataH) {
		uint32_t FrameDataLen = 0;
		aux_LE_to_uint32(&FrameDataLen, DataH + LenMagic, sizeof(uint32_t));
		const char *DataF = (const char *) evbuffer_pullup(Ev, LenMagic + sizeof(uint32_t) + FrameDataLen);
		if (DataF) {
			DataOpt = DataF;
			LenHdr = LenMagic + sizeof(uint32_t);
			LenData = FrameDataLen;
		}
	}

	if (oDataOpt)
		*oDataOpt = DataOpt;
	if (oLenHdr)
		*oLenHdr = LenHdr;
	if (oLenDataOpt)
		*oLenDataOpt = LenData;

	return 0;
}

void bev_event_cb(struct bufferevent *Bev, short What, void *Ctx)
{
	int r = 0;
	struct GsEvCtxClnt *CtxClnt = (struct GsEvCtxClnt *) Ctx;

	if (CtxClnt->mMagic != GS_EV_CTX_CLNT_MAGIC)
		assert(0);

	printf("HelloE %x\n", What);
	if (What & BEV_EVENT_ERROR) {
		printf("%s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
	}
	
	if (What & BEV_EVENT_ERROR || What & BEV_EVENT_EOF || What & BEV_EVENT_TIMEOUT) {
		if (!!(r = event_base_loopbreak(CtxClnt->mBase)))
			assert(0);
	}
}

void bev_read_cb(struct bufferevent *Bev, void *Ctx)
{
	printf("HelloR\n");
	int r = 0;
	const char *Data = NULL;
	size_t LenHdr = 0;
	size_t LenData = 0;
	struct evbuffer *Ev = bufferevent_get_input(Bev);
	if (!!(r = gs_ev_evbuffer_get_frame_try(Ev, &Data, &LenHdr, &LenData)))
		assert(0);
	if (Data) {
		if (!!(r = evbuffer_drain(Ev, LenHdr)))
			assert(0);
	}
}

int main(int argc, char **argv)
{
	int r = 0;

	struct event_base *Base = NULL;
	struct bufferevent *Bev = NULL;
	struct event *Tev = NULL;

	struct GsEvCtxClnt *CtxClnt = new GsEvCtxClnt();

#ifdef _WIN32
	WSADATA wsa_data;
	if (!!(r = WSAStartup(0x0201, &wsa_data)))
		assert(0);
#endif

	if (!(Base = event_base_new()))
		assert(0);

	if (!(Bev = bufferevent_socket_new(Base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS)))
		assert(0);

	if (!!(r = bufferevent_enable(Bev, EV_READ)))
		assert(0);

	CtxClnt->mMagic = GS_EV_CTX_CLNT_MAGIC;
	CtxClnt->mBase = Base;

	bufferevent_setcb(Bev, bev_read_cb, NULL, bev_event_cb, CtxClnt);

	if (!!(r = bufferevent_socket_connect_hostname(Bev, NULL, AF_INET, "localhost", 3756)))
		assert(0);

	if (!!(r = bufferevent_write(Bev, "HelloWorld", sizeof "HelloWorld")))
		assert(0);

	if (!!(r = event_base_loop(Base, EVLOOP_NO_EXIT_ON_EMPTY)))
		assert(0);

	printf("exiting\n");

	return EXIT_SUCCESS;
}
