#ifndef _GITTEST_EV2_TEST_H_
#define _GITTEST_EV2_TEST_H_

#include <cstdint>

struct GsEvData
{
	uint8_t *data;
	size_t   dataLength;
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
