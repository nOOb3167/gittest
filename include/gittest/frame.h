#ifndef _GITTEST_FRAME_H_
#define _GITTEST_FRAME_H_

#include <cstdint>

#include <string>
#include <vector>

#include <git2.h>

#include <gittest/misc.h>
#include <gittest/cbuf.h>

#define GS_FRAME_HEADER_STR_LEN 40
#define GS_FRAME_HEADER_NUM_LEN 4
#define GS_FRAME_HEADER_LEN (GS_FRAME_HEADER_STR_LEN + GS_FRAME_HEADER_NUM_LEN)
#define GS_FRAME_SIZE_LEN 4

#define GS_PAYLOAD_OID_LEN 20

#define GS_FRAME_TYPE_SERV_AUX_INTERRUPT_REQUESTED 0
#define GS_FRAME_TYPE_REQUEST_LATEST_COMMIT_TREE 1
#define GS_FRAME_TYPE_RESPONSE_LATEST_COMMIT_TREE 2
#define GS_FRAME_TYPE_REQUEST_TREELIST 3
#define GS_FRAME_TYPE_RESPONSE_TREELIST 4
#define GS_FRAME_TYPE_REQUEST_TREES 5
#define GS_FRAME_TYPE_RESPONSE_TREES 6
#define GS_FRAME_TYPE_REQUEST_BLOBS 7
#define GS_FRAME_TYPE_RESPONSE_BLOBS 8
#define GS_FRAME_TYPE_REQUEST_BLOBS_SELFUPDATE 9
#define GS_FRAME_TYPE_RESPONSE_BLOBS_SELFUPDATE 10
#define GS_FRAME_TYPE_REQUEST_LATEST_SELFUPDATE_BLOB 11
#define GS_FRAME_TYPE_RESPONSE_LATEST_SELFUPDATE_BLOB 12

#define GS_FRAME_TYPE_DECL2(name) GS_FRAME_TYPE_ ## name
#define GS_FRAME_TYPE_DECL(name) { # name, GS_FRAME_TYPE_DECL2(name) }

#define GS_FRAME_TYPE_DEFINE_FRAME_TYPE_ARRAY(VARNAME)       \
	GsFrameType (VARNAME)[] = {                              \
		GS_FRAME_TYPE_DECL(SERV_AUX_INTERRUPT_REQUESTED),    \
		GS_FRAME_TYPE_DECL(REQUEST_LATEST_COMMIT_TREE),      \
		GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE),     \
		GS_FRAME_TYPE_DECL(REQUEST_TREELIST),                \
		GS_FRAME_TYPE_DECL(RESPONSE_TREELIST),               \
		GS_FRAME_TYPE_DECL(REQUEST_TREES),                   \
		GS_FRAME_TYPE_DECL(RESPONSE_TREES),                  \
		GS_FRAME_TYPE_DECL(REQUEST_BLOBS),                   \
		GS_FRAME_TYPE_DECL(RESPONSE_BLOBS),                  \
		GS_FRAME_TYPE_DECL(REQUEST_BLOBS_SELFUPDATE),        \
		GS_FRAME_TYPE_DECL(RESPONSE_BLOBS_SELFUPDATE),       \
		GS_FRAME_TYPE_DECL(REQUEST_LATEST_SELFUPDATE_BLOB),  \
		GS_FRAME_TYPE_DECL(RESPONSE_LATEST_SELFUPDATE_BLOB), \
	};

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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct GsStrided {
	uint8_t *mDataStart;
	uint32_t mDataOffset;
	uint32_t mEltNum;
	uint32_t mEltSize;
	uint32_t mEltStride;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

struct GsFrameType {
	char mTypeName[GS_FRAME_HEADER_STR_LEN];
	uint32_t mTypeNum;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

bool aux_frametype_equals(const GsFrameType &a, const GsFrameType &b);

int aux_frame_enough_space(uint32_t TotalLength, uint32_t Offset, uint32_t WantedSpace);

int aux_frame_read_buf(
	uint8_t *DataStart, uint32_t DataLength, uint32_t DataOffset, uint32_t *DataOffsetNew,
	uint8_t *BufStart, uint32_t BufLength, uint32_t BufOffset, uint32_t NumToRead);
int aux_frame_write_buf(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew, uint8_t *Buf, uint32_t BufLen);

int aux_frame_read_size(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t SizeOfSize, uint32_t *oSize, uint32_t *oDataLengthLimit);
int aux_frame_write_size(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t SizeOfSize, uint32_t Size);
int aux_frame_read_size_ensure(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew, uint32_t MSize);
int aux_frame_read_size_limit(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t SizeOfSize, uint32_t *oDataLengthLimit);

int aux_frame_read_frametype(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	GsFrameType *oFrameType);
int aux_frame_write_frametype(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	GsFrameType *FrameType);
int aux_frame_ensure_frametype(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	const GsFrameType &FrameType);

int aux_frame_read_oid(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint8_t *oOid, uint32_t OidSize);
int aux_frame_write_oid(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint8_t *Oid, uint32_t OidSize);
int aux_frame_read_oid_vec(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	void *ctx, gs_bypart_cb_t cb);
int aux_frame_write_oid_vec(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	const GsStrided OidVec);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

/* GsBypartCbDataString */
GS_BYPART_DATA_DECL(String, std::string *m0Buffer;);
#define GS_BYPART_TRIPWIRE_String 0x23132359
#define GS_BYPART_DATA_INIT_String(VARNAME, PBUFFER) (VARNAME).m0Buffer = PBUFFER;
int gs_bypart_cb_String(void *ctx, const char *d, int64_t l);
int gs_bysize_cb_String(void *ctx, int64_t l, uint8_t **od);

int aux_frame_full_aux_write_empty(
	GsFrameType *FrameType,
	gs_bysize_cb_t cb, void *ctx);
int aux_frame_full_aux_write_oid(
	GsFrameType *FrameType, uint8_t *Oid, uint32_t OidSize,
	gs_bysize_cb_t cb, void *ctx);
int aux_frame_full_aux_write_oid_vec(
	std::string *oBuffer, GsFrameType *FrameType,
	const GsStrided OidVec);
int aux_frame_full_aux_read_paired_vec_noalloc(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t *oPairedVecLen, uint32_t *oOffsetSizeBuffer, uint32_t *oOffsetObjectBuffer);
int aux_frame_full_aux_write_paired_vec(
	std::string *oBuffer,
	GsFrameType *FrameType, uint32_t PairedVecLen, std::string *SizeBufferTree, std::string *ObjectBufferTree);

int aux_frame_full_write_serv_aux_interrupt_requested(
	gs_bysize_cb_t cb, void *ctx);
int aux_frame_full_write_request_latest_commit_tree(
	gs_bysize_cb_t cb, void *ctx);
int aux_frame_full_write_response_latest_commit_tree(
	uint8_t *Oid, uint32_t OidSize,
	gs_bysize_cb_t cb, void *ctx);
int aux_frame_full_write_request_treelist(
	uint8_t *Oid, uint32_t OidSize,
	gs_bysize_cb_t cb, void *ctx);
int aux_frame_full_write_response_trees(
	std::string *oBuffer,
	uint32_t PairedVecLen, std::string *SizeBufferTree, std::string *ObjectBufferTree);
int aux_frame_full_write_response_blobs(
	std::string *oBuffer, const GsFrameType &FrameType,
	uint32_t PairedVecLen, std::string *SizeBufferBlob, std::string *ObjectBufferBlob);
int aux_frame_full_write_request_latest_selfupdate_blob(
	gs_bysize_cb_t cb, void *ctx);
int aux_frame_full_write_response_latest_selfupdate_blob(
	uint8_t *Oid, uint32_t OidSize,
	gs_bysize_cb_t cb, void *ctx);

int gs_strided_for_oid_vec_cpp(std::vector<git_oid> *OidVec, GsStrided *oStrided);

int aux_frame_read_oid_vec_cpp(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	std::vector<git_oid> *oOidVec);
int aux_frame_write_oid_vec_cpp(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	std::vector<git_oid> *OidVec);

int aux_frame_full_write_response_treelist_cpp(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec);
int aux_frame_full_write_request_trees_cpp(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec);
int aux_frame_full_write_request_blobs_cpp(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec);
int aux_frame_full_write_request_blobs_selfupdate_cpp(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec);

#endif /* __cplusplus */

#endif /* _GITTEST_FRAME_H_ */
