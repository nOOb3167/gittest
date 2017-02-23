#include <cstdint>
#include <cstring>

#include <gittest/cbuf.h>

int cbuf_setup(uint64_t sz, cbuf *oc) {
	char *d = new char[sz];
	memset(d, '\0', sz);

	cbuf c = {};
	c.d = d;
	c.sz = sz;
	c.s = 0;
	c.e = 0;

	if (oc)
		*oc = c;

	return 0;
}

void cbuf_reset(cbuf *c) {
	if (c->d) {
		delete c->d;
		c->d = NULL;
		c->sz = 0;
		c->s = 0;
		c->e = 0;
	}
}

int64_t cbuf_mod(int64_t a, int64_t m) {
	return (a % m) < 0 ? (a % m) + m : (a % m);
}

int64_t cbuf_len(cbuf *c) {
	return cbuf_mod(c->e - c->s, c->sz);
}

int64_t cbuf_available(cbuf *c) {
	return c->sz - 1 - cbuf_len(c);
}

int cbuf_push_back(cbuf *c, char *d, int64_t l) {
	if (cbuf_len(c) + l >= c->sz)
		return 1;
	int64_t lfst = GS_MIN(c->sz - c->e, l);
	memcpy(c->d + c->e, d, lfst);
	memcpy(c->d + 0, d + lfst, l - lfst);
	c->e = cbuf_mod(c->e + l, c->sz);
	return 0;
}

int cbuf_push_back_discarding(cbuf *c, char *d, int64_t l) {
	if (l >= c->sz)
		return 1;
	int64_t discard = GS_MAX(l - cbuf_available(c), 0);
	if (!!cbuf_pop_front_only(c, discard))
		return 1;
	if (cbuf_available(c) < l)
		return 1;
	if (!!cbuf_push_back(c, d, l))
		return 1;
	return 0;
}

int cbuf_pop_front(cbuf *c, char *d, int64_t l) {
	if (cbuf_len(c) - l < 0)
		return 1;
	int64_t lfst = GS_MIN(c->sz - c->s, l);
	memcpy(d, c->d + c->s, lfst);
	memcpy(d + lfst, c->d + 0, l - lfst);
	c->s = cbuf_mod(c->s + l, c->sz);
	return 0;
}

int cbuf_pop_front_only(cbuf *c, int64_t l) {
	if (cbuf_len(c) - l < 0)
		return 1;
	c->s = cbuf_mod(c->s + l, c->sz);
	return 0;
}
