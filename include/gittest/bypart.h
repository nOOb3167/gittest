#ifndef _GITTEST_BYPART_H_
#define _GITTEST_BYPART_H_

#include <stdint.h>

#ifdef __cplusplus
#include <vector>
#include <git2.h>
#endif /* __cplusplus */

#define GS_STRIDED_PIDX(S, IDX) ((S).mDataStart + (S).mDataOffset + (S).mEltStride * (IDX))

#define GS_BYPART_DATA_DECL(SUBNAME, MEMBERS) \
	struct GsBypartCbData ## SUBNAME { uint32_t Tripwire; MEMBERS }

#define GS_BYPART_DATA_VAR(SUBNAME, VARNAME) \
	GsBypartCbData ## SUBNAME VARNAME;       \
	(VARNAME).Tripwire = GS_BYPART_TRIPWIRE_ ## SUBNAME;

#define GS_BYPART_DATA_INIT(SUBNAME, VARNAME, ...) \
	GS_BYPART_DATA_INIT_ ## SUBNAME (VARNAME, __VA_ARGS__)

#define GS_BYPART_DATA_VAR_CTX_NONUCF(SUBNAME, VARNAME, CTXNAME)                 \
	GsBypartCbData ## SUBNAME * VARNAME = (GsBypartCbData ## SUBNAME *) CTXNAME; \
	{                                                                            \
		if ((VARNAME)->Tripwire != GS_BYPART_TRIPWIRE_ ## SUBNAME)               \
			{ r = 1; goto clean; }                                               \
	}

#define GS_BYPART_DATA_VAR_AUX_TRIPWIRE_CHECK_NONUCF(SUBNAME, PVAR) \
	if ((PVAR)->Tripwire != GS_BYPART_TRIPWIRE_ ## SUBNAME)         \
		{ r = 1; goto clean; }

typedef int(*gs_bypart_cb_t) (void *ctx, const char *d, int64_t l);
typedef gs_bypart_cb_t gd_byfull_cb_t;
typedef int(*gs_bysize_cb_t) (void *ctx, int64_t l, uint8_t **od);

/** manual-init struct
    value struct
*/
struct GsStrided {
	uint8_t *mDataStart;
	uint32_t mDataOffset;
	uint32_t mEltNum;
	uint32_t mEltSize;
	uint32_t mEltStride;
};

int gs_strided_for_oid_vec(
	const git_oid *OidVec,
	size_t OidVecNum,
	GsStrided *oStrided);

int gs_strided_for_struct_member(
	uint8_t *DataStart, uint32_t DataStartOffset, uint32_t OffsetOfMember,
	uint32_t EltNum, uint32_t EltSize, uint32_t EltStride,
	GsStrided *oStrided);

#ifdef __cplusplus

struct GsConnectionSurrogateId;
typedef uint64_t gs_connection_surrogate_id_t; /* FIXME: 'forward' typedef (canonical decl net2.h) */

GS_BYPART_DATA_DECL(GsConnectionSurrogateId, gs_connection_surrogate_id_t m0Id;);
#define GS_BYPART_TRIPWIRE_GsConnectionSurrogateId 0x68347232
#define GS_BYPART_DATA_INIT_GsConnectionSurrogateId(VARNAME, ID) (VARNAME).m0Id = ID;

/* GsBypartCbDataString */
GS_BYPART_DATA_DECL(String, std::string *m0Buffer;);
#define GS_BYPART_TRIPWIRE_String 0x23132359
#define GS_BYPART_DATA_INIT_String(VARNAME, PBUFFER) (VARNAME).m0Buffer = PBUFFER;
int gs_bypart_cb_String(void *ctx, const char *d, int64_t l);
int gs_bysize_cb_String(void *ctx, int64_t l, uint8_t **od);

/* GsBypartCbDataOidVector */
GS_BYPART_DATA_DECL(OidVector, std::vector<git_oid> *m0OidVec;);
#define GS_BYPART_TRIPWIRE_OidVector 0x23132358
#define GS_BYPART_DATA_INIT_OidVector(VARNAME, POIDVEC) (VARNAME).m0OidVec = POIDVEC;
int gs_bypart_cb_OidVector(void *ctx, const char *d, int64_t l);

int gs_strided_for_oid_vec_cpp(std::vector<git_oid> *OidVec, GsStrided *oStrided);

#endif /* __cplusplus */

#endif /* _GITTEST_BYPART_H_ */
