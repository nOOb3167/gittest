#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif _MSC_VER

#include <cassert>
#include <cstdint>
#include <cstring>

#include <gittest/gittest.h>
#include <gittest/net.h>
#include <gittest/cbuf.h>

#include <gittest/frame.h>

bool aux_frametype_equals(const GsFrameType &a, const GsFrameType &b) {
	assert(sizeof a.mTypeName == GS_FRAME_HEADER_STR_LEN);
	bool eqstr = memcmp(a.mTypeName, b.mTypeName, GS_FRAME_HEADER_STR_LEN) == 0;
	bool eqnum = a.mTypeNum == b.mTypeNum;
	/* XOR basically */
	if ((eqstr || eqnum) && (!eqstr || !eqnum))
		assert(0);
	return eqstr && eqnum;
}

int aux_frame_enough_space(uint32_t TotalLength, uint32_t Offset, uint32_t WantedSpace) {
	int r = 0;

	if (! ((TotalLength >= Offset) && (TotalLength - Offset) >= WantedSpace))
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int aux_frame_read_buf(
	uint8_t *DataStart, uint32_t DataLength, uint32_t DataOffset, uint32_t *DataOffsetNew,
	uint8_t *BufStart, uint32_t BufLength, uint32_t BufOffset, uint32_t NumToRead)
{
	int r = 0;

	if (!!(r = aux_frame_enough_space(DataLength, DataOffset, NumToRead)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_enough_space(BufLength, BufOffset, NumToRead)))
		GS_GOTO_CLEAN();

	memcpy(BufStart + BufOffset, DataStart + DataOffset, NumToRead);

	if (DataOffsetNew)
		*DataOffsetNew = DataOffset + NumToRead;

clean:

	return r;
}

int aux_frame_write_buf(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew, uint8_t *Buf, uint32_t BufLen) {
	int r = 0;

	if (!!(r = aux_frame_enough_space(DataLength, Offset, BufLen)))
		GS_GOTO_CLEAN();

	memcpy(DataStart + Offset, Buf, BufLen);

	if (OffsetNew)
		*OffsetNew = Offset + BufLen;

clean:

	return r;
}

int aux_frame_read_size(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t SizeOfSize, uint32_t *oSize, uint32_t *oDataLengthLimit)
{
	int r = 0;

	uint32_t Size = 0;

	if (!!(r = aux_frame_enough_space(DataLength, Offset, SizeOfSize)))
		GS_GOTO_CLEAN();

	aux_LE_to_uint32(&Size, (char *)(DataStart + Offset), SizeOfSize);


	if (oSize)
		*oSize = Size;

	if (oDataLengthLimit) {
		// FIXME: not implemented / senseless for read size, specialize into a read limit
		//*oDataLengthLimit = Offset + SizeOfSize + Size;
		assert(0);
		GS_ERR_CLEAN(1);
	}

	if (OffsetNew)
		*OffsetNew = Offset + SizeOfSize;

clean:

	return r;
}

int aux_frame_write_size(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t SizeOfSize, uint32_t Size)
{
	int r = 0;

	assert(SizeOfSize == sizeof(uint32_t));

	if (!!(r = aux_frame_enough_space(DataLength, Offset, SizeOfSize)))
		GS_GOTO_CLEAN();

	aux_uint32_to_LE(Size, (char *)(DataStart + Offset), SizeOfSize);

	if (OffsetNew)
		*OffsetNew = Offset + SizeOfSize;

clean:

	return r;
}

int aux_frame_read_size_ensure(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew, uint32_t MSize) {
	int r = 0;

	uint32_t SizeFound = 0;

	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &SizeFound, NULL)))
		GS_GOTO_CLEAN();

	if (SizeFound != MSize)
		GS_ERR_CLEAN(1);

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_read_size_limit(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t SizeOfSize, uint32_t *oDataLengthLimit)
{
	int r = 0;

	uint32_t Size = 0;

	if (!!(r = aux_frame_enough_space(DataLength, Offset, SizeOfSize)))
		GS_GOTO_CLEAN();

	aux_LE_to_uint32(&Size, (char *)(DataStart + Offset), SizeOfSize);

	if (!!(r = aux_frame_enough_space(DataLength, Offset + SizeOfSize, Size)))
		GS_GOTO_CLEAN();

	if (oDataLengthLimit)
		*oDataLengthLimit = Offset + SizeOfSize + Size;

	if (OffsetNew)
		*OffsetNew = Offset + SizeOfSize;

clean:

	return r;
}

int aux_frame_read_frametype(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	GsFrameType *oFrameType)
{
	int r = 0;

	GsFrameType FrameType = {};

	if (!!(r = aux_frame_enough_space(DataLength, Offset, GS_FRAME_HEADER_STR_LEN + GS_FRAME_HEADER_NUM_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_buf(
		DataStart, DataLength, Offset, &Offset,
		(uint8_t *)FrameType.mTypeName, GS_FRAME_HEADER_STR_LEN, 0, GS_FRAME_HEADER_STR_LEN)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_HEADER_NUM_LEN, &FrameType.mTypeNum, NULL)))
		GS_GOTO_CLEAN();

	if (oFrameType)
		*oFrameType = FrameType;

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_write_frametype(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	GsFrameType *FrameType)
{
	int r = 0;

	if (!!(r = aux_frame_enough_space(DataLength, Offset, GS_FRAME_HEADER_STR_LEN + GS_FRAME_HEADER_NUM_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_buf(DataStart, DataLength, Offset, &Offset, (uint8_t *)FrameType->mTypeName, GS_FRAME_HEADER_STR_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_HEADER_NUM_LEN, FrameType->mTypeNum)))
		GS_GOTO_CLEAN();

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_ensure_frametype(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	const GsFrameType &FrameType)
{
	int r = 0;

	GsFrameType FoundFrameType = {};

	if (!!(r = aux_frame_read_frametype(DataStart, DataLength, Offset, &Offset, &FoundFrameType)))
		GS_GOTO_CLEAN();

	if (! aux_frametype_equals(FoundFrameType, FrameType))
		GS_ERR_CLEAN(1);

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_read_oid(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint8_t *oOid, uint32_t OidSize)
{
	int r = 0;

	assert(OidSize == GIT_OID_RAWSZ && GS_PAYLOAD_OID_LEN == GIT_OID_RAWSZ);

	if (!!(r = aux_frame_read_buf(
		DataStart, DataLength, Offset, &Offset,
		oOid, OidSize, 0, OidSize)))
	{
		GS_GOTO_CLEAN();
	}

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_write_oid(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint8_t *Oid, uint32_t OidSize)
{
	int r = 0;

	assert(OidSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);

	if (!!(r = aux_frame_write_buf(DataStart, DataLength, Offset, &Offset, Oid, OidSize)))
		GS_GOTO_CLEAN();

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_read_oid_vec(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	void *ctx, gs_bypart_cb_t cb)
{
	int r = 0;

	uint32_t OidNum = 0;

	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &OidNum, NULL)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < OidNum; i++) {
		uint8_t OidBuf[GIT_OID_RAWSZ];
		if (!!(r = aux_frame_read_oid(DataStart, DataLength, Offset, &Offset, OidBuf, GIT_OID_RAWSZ)))
			GS_GOTO_CLEAN();
		if (!!(r = cb(ctx, (char *)OidBuf, GIT_OID_RAWSZ)))
			GS_GOTO_CLEAN();
	}

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_write_oid_vec(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	const GsStrided OidVec)
{
	int r = 0;

	if (!!(r = aux_frame_write_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, OidVec.mEltNum)))
		GS_GOTO_CLEAN();

	assert(OidVec.mEltSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);

	for (uint32_t i = 0; i < OidVec.mEltNum; i++) {
		if (!!(r = aux_frame_write_oid(DataStart, DataLength, Offset, &Offset, GS_STRIDED_PIDX(OidVec, i), OidVec.mEltSize)))
			GS_GOTO_CLEAN();
	}

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_full_aux_write_empty(
	GsFrameType *FrameType,
	gs_bysize_cb_t cb, void *ctx)
{
	int r = 0;

	uint32_t Offset = 0;
	uint32_t BufferSize = GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + 0;
	uint8_t * BufferData = NULL;

	if (!!(r = cb(ctx, BufferSize, &BufferData)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_frametype(BufferData, BufferSize, Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size(BufferData, BufferSize, Offset, &Offset, GS_FRAME_SIZE_LEN, 0)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int aux_frame_full_aux_write_oid(
	GsFrameType *FrameType, uint8_t *Oid, uint32_t OidSize,
	gs_bysize_cb_t cb, void *ctx)
{
	int r = 0;

	uint32_t Offset = 0;
	uint32_t PayloadSize = OidSize;
	uint32_t BufferSize = GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + PayloadSize;
	uint8_t *BufferData = NULL;

	assert(OidSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);

	if (!!(r = cb(ctx, BufferSize, &BufferData)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_frametype(BufferData, BufferSize, Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();

	assert(OidSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);

	if (!!(r = aux_frame_write_size(BufferData, BufferSize, Offset, &Offset, GS_FRAME_SIZE_LEN, PayloadSize)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_oid(BufferData, BufferSize, Offset, &Offset, Oid, OidSize)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int aux_frame_full_aux_write_oid_vec(
	GsFrameType *FrameType, const GsStrided OidVec,
	gs_bysize_cb_t cb, void *ctx)
{
	int r = 0;

	uint32_t Offset = 0;
	uint32_t PayloadSize = GS_FRAME_SIZE_LEN + OidVec.mEltNum * OidVec.mEltSize;
	uint32_t BufferSize = GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + PayloadSize;
	uint8_t *BufferData = NULL;

	assert(OidVec.mEltSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);

	if (!!(r = cb(ctx, BufferSize, &BufferData)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_frametype(BufferData, BufferSize, Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size(BufferData, BufferSize, Offset, &Offset, GS_FRAME_SIZE_LEN, PayloadSize)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_oid_vec(BufferData, BufferSize, Offset, &Offset, OidVec)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int aux_frame_full_aux_read_paired_vec_noalloc(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	uint32_t *oPairedVecLen, uint32_t *oOffsetSizeBuffer, uint32_t *oOffsetObjectBuffer)
{
	int r = 0;

	uint32_t PairedVecLen = 0;
	uint32_t OffsetSizeBuffer = 0;
	uint32_t OffsetObjectBuffer = 0;
	uint32_t OffsetEnd = 0;

	uint32_t CumulativeSize = 0;

	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &PairedVecLen, NULL)))
		GS_GOTO_CLEAN();

	OffsetSizeBuffer = Offset;

	OffsetObjectBuffer = OffsetSizeBuffer + GS_FRAME_SIZE_LEN * PairedVecLen;

	if (!!(r = aux_frame_enough_space(DataLength, OffsetSizeBuffer, GS_FRAME_SIZE_LEN * PairedVecLen)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < PairedVecLen; i++) {
		uint32_t FoundSize = 0;
		aux_LE_to_uint32(&FoundSize, (char *)(DataStart + OffsetSizeBuffer + GS_FRAME_SIZE_LEN * i), GS_FRAME_SIZE_LEN);
		CumulativeSize += FoundSize;
	}

	if (!!(r = aux_frame_enough_space(DataLength, OffsetObjectBuffer, CumulativeSize)))
		GS_GOTO_CLEAN();

	OffsetEnd = OffsetObjectBuffer + CumulativeSize;

	if (oPairedVecLen)
		*oPairedVecLen = PairedVecLen;

	if (oOffsetSizeBuffer)
		*oOffsetSizeBuffer = OffsetSizeBuffer;

	if (oOffsetObjectBuffer)
		*oOffsetObjectBuffer = OffsetObjectBuffer;

	if (OffsetNew)
		*OffsetNew = OffsetEnd;

clean:

	return r;
}

int aux_frame_full_aux_write_paired_vec(
	GsFrameType *FrameType, uint32_t PairedVecLen,
	uint8_t *SizeBufferTreeData, uint32_t SizeBufferTreeSize,
	uint8_t *ObjectBufferTreeData, uint32_t ObjectBufferTreeSize,
	gs_bysize_cb_t cb, void *ctx)
{
	int r = 0;

	uint32_t Offset = 0;
	uint32_t PayloadSize = GS_FRAME_SIZE_LEN + SizeBufferTreeSize + ObjectBufferTreeSize;
	uint32_t BufferSize = GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + PayloadSize;
	uint8_t *BufferData = NULL;

	assert(GS_PAYLOAD_OID_LEN == GIT_OID_RAWSZ);

	if (!!(r = cb(ctx, BufferSize, &BufferData)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_frametype(BufferData, BufferSize, Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size(BufferData, BufferSize, Offset, &Offset, GS_FRAME_SIZE_LEN, PayloadSize)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size(BufferData, BufferSize, Offset, &Offset, GS_FRAME_SIZE_LEN, PairedVecLen)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_buf(BufferData, BufferSize, Offset, &Offset, SizeBufferTreeData, SizeBufferTreeSize)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_buf(BufferData, BufferSize, Offset, &Offset, ObjectBufferTreeData, ObjectBufferTreeSize)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int aux_frame_full_write_serv_aux_interrupt_requested(
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(SERV_AUX_INTERRUPT_REQUESTED);

	return aux_frame_full_aux_write_empty(&FrameType, cb, ctx);
}

int aux_frame_full_write_request_latest_commit_tree(
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_LATEST_COMMIT_TREE);

	return aux_frame_full_aux_write_empty(&FrameType, cb, ctx);
}

int aux_frame_full_write_response_latest_commit_tree(
	uint8_t *Oid, uint32_t OidSize,
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE);

	return aux_frame_full_aux_write_oid(&FrameType, Oid, OidSize, cb, ctx);
}

int aux_frame_full_write_request_treelist(
	uint8_t *Oid, uint32_t OidSize,
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_TREELIST);

	return aux_frame_full_aux_write_oid(&FrameType, Oid, OidSize, cb, ctx);
}

int aux_frame_full_write_response_treelist(
	GsStrided OidVecStrided,
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_TREELIST);

	return aux_frame_full_aux_write_oid_vec(&FrameType, OidVecStrided, cb, ctx);
}

int aux_frame_full_write_request_trees(
	GsStrided OidVecStrided,
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_TREES);

	return aux_frame_full_aux_write_oid_vec(&FrameType, OidVecStrided, cb, ctx);
}

int aux_frame_full_write_response_trees(
	uint32_t PairedVecLen,
	uint8_t *SizeBufferTreeData, uint32_t SizeBufferTreeSize,
	uint8_t *ObjectBufferTreeData, uint32_t ObjectBufferTreeSize,
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_TREES);

	return aux_frame_full_aux_write_paired_vec(&FrameType, PairedVecLen,
		SizeBufferTreeData, SizeBufferTreeSize,
		ObjectBufferTreeData, ObjectBufferTreeSize,
		cb, ctx);
}

int aux_frame_full_write_request_blobs(
	GsStrided OidVecStrided,
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_BLOBS);

	return aux_frame_full_aux_write_oid_vec(&FrameType, OidVecStrided, cb, ctx);
}

int aux_frame_full_write_request_blobs_selfupdate(
	GsStrided OidVecStrided,
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_BLOBS_SELFUPDATE);

	return aux_frame_full_aux_write_oid_vec(&FrameType, OidVecStrided, cb, ctx);
}

int aux_frame_full_write_response_blobs(
	const GsFrameType &FrameType, uint32_t PairedVecLen,
	uint8_t *SizeBufferBlobData, uint32_t SizeBufferBlobSize,
	uint8_t *ObjectBufferBlobData, uint32_t ObjectBufferBlobSize,
	gs_bysize_cb_t cb, void *ctx)
{
	GsFrameType FrameTypeTmp = FrameType;

	return aux_frame_full_aux_write_paired_vec(&FrameTypeTmp, PairedVecLen,
		SizeBufferBlobData, SizeBufferBlobSize,
		ObjectBufferBlobData, ObjectBufferBlobSize,
		cb, ctx);
}

int aux_frame_full_write_request_latest_selfupdate_blob(
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_LATEST_SELFUPDATE_BLOB);

	return aux_frame_full_aux_write_empty(&FrameType, cb, ctx);
}

int aux_frame_full_write_response_latest_selfupdate_blob(
	uint8_t *Oid, uint32_t OidSize,
	gs_bysize_cb_t cb, void *ctx)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_LATEST_SELFUPDATE_BLOB);

	return aux_frame_full_aux_write_oid(&FrameType, Oid, OidSize, cb, ctx);
}

int gs_bypart_cb_String(void *ctx, const char *d, int64_t l) {
	int r = 0;

	git_oid Oid = {};
	GS_BYPART_DATA_VAR_CTX_NONUCF(String, Data, ctx);

	Data->m0Buffer->append(d, l);

clean:

	return r;
}

int gs_bysize_cb_String(void *ctx, int64_t l, uint8_t **od) {
	int r = 0;

	GS_BYPART_DATA_VAR_CTX_NONUCF(String, Data, ctx);

	Data->m0Buffer->resize(l);

	if (od)
		*od = (uint8_t *)Data->m0Buffer->data();

clean:

	return r;
}

int gs_strided_for_oid_vec_cpp(std::vector<git_oid> *OidVec, GsStrided *oStrided) {
	int r = 0;

	uint8_t *DataStart = (uint8_t *)OidVec->data();
	uint32_t DataOffset = 0 + offsetof(git_oid, id);
	uint32_t EltNum = OidVec->size();
	uint32_t EltSize = sizeof *OidVec->data();
	uint32_t EltStride = GIT_OID_RAWSZ;

	GsStrided Strided = {
		DataStart,
		DataOffset,
		EltNum,
		EltSize,
		EltStride,
	};

	uint32_t DataLength = OidVec->size() * sizeof *OidVec->data();

	if (EltSize > EltStride || DataOffset + EltStride * EltNum > DataLength)
		GS_ERR_CLEAN(1);

	if (oStrided)
		*oStrided = Strided;

clean:

	return r;
}

int aux_frame_read_oid_vec_cpp(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	std::vector<git_oid> *oOidVec)
{
	int r = 0;

	std::vector<git_oid> OidVec;
	uint32_t OidNum = 0;

	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &OidNum, NULL)))
		GS_GOTO_CLEAN();

	// FIXME: hmmm, almost unbounded allocation, from a single uint32_t read off the network
	OidVec.resize(OidNum);
	for (uint32_t i = 0; i < OidNum; i++) {
		if (!!(r = aux_frame_read_oid(DataStart, DataLength, Offset, &Offset, (uint8_t *)OidVec[i].id, GIT_OID_RAWSZ)))
			GS_GOTO_CLEAN();
	}

	if (oOidVec)
		oOidVec->swap(OidVec);

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_write_oid_vec_cpp(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	GsStrided OidVecStrided)
{
	return aux_frame_write_oid_vec(DataStart, DataLength, Offset, OffsetNew, OidVecStrided);
}
