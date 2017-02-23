#ifndef _GITTEST_CBUF_H_
#define _GITTEST_CBUF_H_

#include <cstdint>

#define GS_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define GS_MIN(x, y) (((x) < (y)) ? (x) : (y))

/* NOTE: a circular buffer of size sz, using the [s,e) convention holds only sz-1 bytes, not sz */

struct cbuf {
	char *d;
	int64_t sz;
	int64_t s;
	int64_t e;
};

int  cbuf_setup(uint64_t sz, cbuf *oc);
void cbuf_reset(cbuf *c);
int64_t cbuf_mod(int64_t a, int64_t m);
int64_t cbuf_len(cbuf *c);
int64_t cbuf_available(cbuf *c);
int cbuf_push_back(cbuf *c, char *d, int64_t l);
int cbuf_push_back_discarding(cbuf *c, char *d, int64_t l);
int cbuf_pop_front(cbuf *c, char *d, int64_t l);
int cbuf_pop_front_only(cbuf *c, int64_t l);

#endif /* _GITTEST_CBUF_H_ */
