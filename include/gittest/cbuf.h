#ifndef _GITTEST_CBUF_H_
#define _GITTEST_CBUF_H_

#include <cstdint>

#ifdef __cplusplus
#include <memory>
#include <functional>
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* NOTE: a circular buffer of size sz, using the [s,e) convention holds only sz-1 bytes, not sz */

struct cbuf {
	char *d;
	int64_t sz;
	int64_t s;
	int64_t e;
};

typedef int(*gs_bypart_cb_t) (void *ctx, const char *d, int64_t l);
typedef gs_bypart_cb_t gd_byfull_cb_t;

int  cbuf_setup(uint64_t sz, cbuf *oc);
void cbuf_reset(cbuf *c);
void cbuf_clear(cbuf *c);
int64_t cbuf_mod(int64_t a, int64_t m);
int64_t cbuf_len(cbuf *c);
int64_t cbuf_available(cbuf *c);
int cbuf_push_back(cbuf *c, const char *d, int64_t l);
int cbuf_push_back_discarding_trunc(cbuf *c, const char *d, int64_t l);
int cbuf_pop_front(cbuf *c, char *d, int64_t l);
int cbuf_pop_front_only(cbuf *c, int64_t l);
int cbuf_read_full_bypart(cbuf *c, void *ctx, gs_bypart_cb_t cb);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

typedef ::std::function<int(const char *d, int64_t l)> gs_bypart_cb_cpp_t;

int cbuf_setup_cpp(uint64_t sz, ::std::shared_ptr<cbuf> *oc);
int cbuf_read_full_bypart_cpp(cbuf *c, gs_bypart_cb_cpp_t cb);
int cbuf_read_full_by_part_cpp_cb_(void *ctx, const char *d, int64_t l);

#endif /* __cplusplus */

#endif /* _GITTEST_CBUF_H_ */
