#ifndef _FRAME_H_
#define _FRAME_H_

#include <cstdint>

#include <string>
#include <vector>

#include <git2.h>

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
#define GS_FRAME_TYPE_REQUEST_BLOB_SELFUPDATE 9
#define GS_FRAME_TYPE_RESPONSE_BLOB_SELFUPDATE 10

#define GS_FRAME_TYPE_DECL2(name) GS_FRAME_TYPE_ ## name
#define GS_FRAME_TYPE_DECL(name) { # name, GS_FRAME_TYPE_DECL2(name) }

struct GsFrameType {
	char mTypeName[GS_FRAME_HEADER_STR_LEN];
	uint32_t mTypeNum;
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
	git_oid *oOid);
int aux_frame_write_oid(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint8_t *Oid, uint32_t OidSize);
int aux_frame_read_oid_vec(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	std::vector<git_oid> *oOidVec);
int aux_frame_write_oid_vec(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	git_oid *OidVec, uint32_t OidNum, uint32_t OidSize);

int aux_frame_full_aux_write_empty(
	std::string *oBuffer,
	GsFrameType *FrameType);
int aux_frame_full_aux_write_oid(
	std::string *oBuffer,
	GsFrameType *FrameType, uint8_t *Oid, uint32_t OidSize);
int aux_frame_full_aux_write_oid_vec(
	std::string *oBuffer,
	GsFrameType *FrameType, git_oid *OidVec, uint32_t OidNum, uint32_t OidSize);
int aux_frame_full_aux_read_paired_vec_noalloc(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t *oPairedVecLen, uint32_t *oOffsetSizeBuffer, uint32_t *oOffsetObjectBuffer);
int aux_frame_full_aux_write_paired_vec(
	std::string *oBuffer,
	GsFrameType *FrameType, uint32_t PairedVecLen, std::string *SizeBufferTree, std::string *ObjectBufferTree);

int aux_frame_full_write_serv_aux_interrupt_requested(
	std::string *oBuffer);
int aux_frame_full_write_request_latest_commit_tree(
	std::string *oBuffer);
int aux_frame_full_write_response_latest_commit_tree(
	std::string *oBuffer,
	uint8_t *Oid, uint32_t OidSize);
int aux_frame_full_write_request_treelist(
	std::string *oBuffer,
	uint8_t *Oid, uint32_t OidSize);
int aux_frame_full_write_response_treelist(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec);
int aux_frame_full_write_request_trees(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec);
int aux_frame_full_write_response_trees(
	std::string *oBuffer,
	uint32_t PairedVecLen, std::string *SizeBufferTree, std::string *ObjectBufferTree);
int aux_frame_full_write_request_blobs(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec);
int aux_frame_full_write_response_blobs(
	std::string *oBuffer,
	uint32_t PairedVecLen, std::string *SizeBufferBlob, std::string *ObjectBufferBlob);
int aux_frame_full_write_request_blob_selfupdate(
	std::string *oBuffer);
int aux_frame_full_write_response_blob_selfupdate(
	std::string *oBuffer,
	uint8_t *Oid, uint32_t OidSize);

extern GsFrameType GsFrameTypes[];

#endif /* _FRAME_H_ */
