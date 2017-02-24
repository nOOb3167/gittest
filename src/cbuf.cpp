#include <cstdint>
#include <cstring>

#include <gittest/cbuf.h>

#define GS_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define GS_MIN(x, y) (((x) < (y)) ? (x) : (y))

int cbuf_setup(uint64_t sz, cbuf *oc) {
	if (oc->d)
		return 1;

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

void cbuf_clear(cbuf *c) {
	c->s = 0;
	c->e = 0;
	memset(c->d, '\0', c->sz);
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

int cbuf_push_back(cbuf *c, const char *d, int64_t l) {
	if (cbuf_len(c) + l >= c->sz)
		return 1;
	int64_t lfst = GS_MIN(c->sz - c->e, l);
	memcpy(c->d + c->e, d, lfst);
	memcpy(c->d + 0, d + lfst, l - lfst);
	c->e = cbuf_mod(c->e + l, c->sz);
	return 0;
}

int cbuf_push_back_discarding_trunc(cbuf *c, const char *d, int64_t l) {
	/* truncate if too long for the buffer */
	if (l >= c->sz)
		l = c->sz;
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
	int64_t lsnd = l - lfst;
	memcpy(d, c->d + c->s, lfst);
	memcpy(d + lfst, c->d + 0, lsnd);
	c->s = cbuf_mod(c->s + l, c->sz);
	return 0;
}

int cbuf_pop_front_only(cbuf *c, int64_t l) {
	if (cbuf_len(c) - l < 0)
		return 1;
	c->s = cbuf_mod(c->s + l, c->sz);
	return 0;
}

int cbuf_read_full_bypart(cbuf *c, void *ctx, gs_bypart_cb_t cb) {
	int64_t l = cbuf_len(c);
	int64_t lfst = GS_MIN(c->sz - c->s, l);
	int64_t lsnd = l - lfst;
	if (! (lfst > 0))
		return 0;
	if (!!cb(ctx, c->d + c->s, lfst))
		return 1;
	if (! (lsnd > 0))
		return 0;
	if (!!cb(ctx, c->d + 0, lsnd))
		return 1;
	return 0;
}

#ifdef __cplusplus

int cbuf_setup_cpp(uint64_t sz, ::std::shared_ptr<cbuf> *oc) {
	::std::shared_ptr<cbuf> pc(new cbuf);
	cbuf c = {};
	*pc = c;
	if (!!cbuf_setup(sz, pc.get()))
		return 1;
	*oc = pc;
	return 0;
}

int cbuf_read_full_bypart_cpp(cbuf *c, gs_bypart_cb_cpp_t cb) {
	if (!!(cbuf_read_full_bypart(c, &cb, cbuf_read_full_by_part_cpp_cb_)))
		return 1;
	return 0;
}

int cbuf_read_full_by_part_cpp_cb_(void *ctx, const char *d, int64_t l) {
	gs_bypart_cb_cpp_t *cb = (gs_bypart_cb_cpp_t *) ctx;
	return (*cb)(d, l);
}

#endif /* __cplusplus */
