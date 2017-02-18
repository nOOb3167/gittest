#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif _MSC_VER

#include <cassert>
#include <cstdint>
#include <cstring>

#include <gittest.h>
#include <gittest_serv.h>

#include <frame.h>

GsFrameType GsFrameTypes[] = {
	GS_FRAME_TYPE_DECL(SERV_AUX_INTERRUPT_REQUESTED),
	GS_FRAME_TYPE_DECL(REQUEST_LATEST_COMMIT_TREE),
	GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE),
	GS_FRAME_TYPE_DECL(REQUEST_TREELIST),
	GS_FRAME_TYPE_DECL(RESPONSE_TREELIST),
	GS_FRAME_TYPE_DECL(REQUEST_TREES),
	GS_FRAME_TYPE_DECL(RESPONSE_TREES),
	GS_FRAME_TYPE_DECL(REQUEST_BLOBS),
	GS_FRAME_TYPE_DECL(RESPONSE_BLOBS),
};

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

	if (oDataLengthLimit)
		*oDataLengthLimit = Offset + SizeOfSize + Size;

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
	git_oid *oOid)
{
	int r = 0;

	git_oid Oid = {};
	uint8_t OidBuf[GIT_OID_RAWSZ] = {};

	assert(GS_PAYLOAD_OID_LEN == GIT_OID_RAWSZ);

	if (!!(r = aux_frame_read_buf(
		DataStart, DataLength, Offset, &Offset,
		OidBuf, GIT_OID_RAWSZ, 0, GIT_OID_RAWSZ)))
	{
		GS_GOTO_CLEAN();
	}

	/* FIXME: LUL GOOD API NO SIZE PARAMETER IMPLEMENTED AS RAW MEMCPY */
	assert(sizeof(unsigned char) == sizeof(uint8_t));
	git_oid_fromraw(&Oid, (unsigned char *)OidBuf);

	if (oOid)
		git_oid_cpy(oOid, &Oid);

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
		if (!!(r = aux_frame_read_oid(DataStart, DataLength, Offset, &Offset, &OidVec[i])))
			GS_GOTO_CLEAN();
	}

	if (oOidVec)
		oOidVec->swap(OidVec);

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_write_oid_vec(
	uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	git_oid *OidVec, uint32_t OidNum, uint32_t OidSize)
{
	int r = 0;

	if (!!(r = aux_frame_write_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, OidNum)))
		GS_GOTO_CLEAN();

	assert(OidSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);

	for (uint32_t i = 0; i < OidNum; i++) {
		if (!!(r = aux_frame_write_oid(DataStart, DataLength, Offset, &Offset, (OidVec + i)->id, OidSize)))
			GS_GOTO_CLEAN();
	}

	if (OffsetNew)
		*OffsetNew = Offset;

clean:

	return r;
}

int aux_frame_full_aux_write_oid(
	std::string *oBuffer,
	GsFrameType *FrameType, uint8_t *Oid, uint32_t OidSize)
{
	int r = 0;

	std::string Buffer;
	uint32_t PayloadSize = 0;
	uint32_t Offset = 0;

	PayloadSize = OidSize;
	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + GS_PAYLOAD_OID_LEN);

	if (!!(r = aux_frame_write_frametype((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();

	assert(OidSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);

	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, PayloadSize)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_oid((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, Oid, OidSize)))
		GS_GOTO_CLEAN();

	if (oBuffer)
		oBuffer->swap(Buffer);

clean:

	return r;
}

int aux_frame_full_aux_write_oid_vec(
	std::string *oBuffer,
	GsFrameType *FrameType, git_oid *OidVec, uint32_t OidNum, uint32_t OidSize)
{
	int r = 0;

	std::string Buffer;
	uint32_t PayloadSize = 0;
	uint32_t Offset = 0;

	PayloadSize = GS_FRAME_SIZE_LEN + OidNum * OidSize;
	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + PayloadSize);

	assert(OidSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);

	if (!!(r = aux_frame_write_frametype((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, PayloadSize)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_oid_vec((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, OidVec, OidNum, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	if (oBuffer)
		oBuffer->swap(Buffer);

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
	std::string *oBuffer,
	GsFrameType *FrameType, uint32_t PairedVecLen, std::string *SizeBufferTree, std::string *ObjectBufferTree)
{
	int r = 0;

	std::string Buffer;
	uint32_t PayloadSize = 0;
	uint32_t Offset = 0;

	PayloadSize = GS_FRAME_SIZE_LEN + SizeBufferTree->size() + ObjectBufferTree->size();
	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + PayloadSize);

	assert(GS_PAYLOAD_OID_LEN == GIT_OID_RAWSZ);

	if (!!(r = aux_frame_write_frametype((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, PayloadSize)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, PairedVecLen)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_buf((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, (uint8_t *)SizeBufferTree->data(), SizeBufferTree->size())))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_buf((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, (uint8_t *)ObjectBufferTree->data(), ObjectBufferTree->size())))
		GS_GOTO_CLEAN();

	if (oBuffer)
		oBuffer->swap(Buffer);

clean:

	return r;
}

int aux_frame_full_write_serv_aux_interrupt_requested(
	std::string *oBuffer)
{
	int r = 0;

	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(SERV_AUX_INTERRUPT_REQUESTED);

	std::string Buffer;
	uint32_t Offset = 0;

	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + 0);

	if (!!(r = aux_frame_write_frametype((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, &FrameType)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, 0)))
		GS_GOTO_CLEAN();

	if (oBuffer)
		oBuffer->swap(Buffer);

clean:

	return r;
}

int aux_frame_full_write_request_latest_commit_tree(
	std::string *oBuffer)
{
	int r = 0;

	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_LATEST_COMMIT_TREE);

	std::string Buffer;
	uint32_t Offset = 0;

	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + 0);

	if (!!(r = aux_frame_write_frametype((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, &FrameType)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, 0)))
		GS_GOTO_CLEAN();

	if (oBuffer)
		oBuffer->swap(Buffer);

clean:

	return r;
}

int aux_frame_full_write_response_latest_commit_tree(
	std::string *oBuffer,
	uint8_t *Oid, uint32_t OidSize)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE);

	return aux_frame_full_aux_write_oid(oBuffer, &FrameType, Oid, OidSize);
}

int aux_frame_full_write_request_treelist(
	std::string *oBuffer,
	uint8_t *Oid, uint32_t OidSize)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_TREELIST);

	return aux_frame_full_aux_write_oid(oBuffer, &FrameType, Oid, OidSize);
}

int aux_frame_full_write_response_treelist(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_TREELIST);

	return aux_frame_full_aux_write_oid_vec(oBuffer, &FrameType, OidVec->data(), OidVec->size(), GIT_OID_RAWSZ);
}

int aux_frame_full_write_request_trees(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_TREES);

	return aux_frame_full_aux_write_oid_vec(oBuffer, &FrameType, OidVec->data(), OidVec->size(), GIT_OID_RAWSZ);
}

int aux_frame_full_write_response_trees(
	std::string *oBuffer,
	uint32_t PairedVecLen, std::string *SizeBufferTree, std::string *ObjectBufferTree)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_TREES);

	return aux_frame_full_aux_write_paired_vec(oBuffer, &FrameType, PairedVecLen, SizeBufferTree, ObjectBufferTree);
}

int aux_frame_full_write_request_blobs(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_BLOBS);

	return aux_frame_full_aux_write_oid_vec(oBuffer, &FrameType, OidVec->data(), OidVec->size(), GIT_OID_RAWSZ);
}

int aux_frame_full_write_response_blobs(
	std::string *oBuffer,
	uint32_t PairedVecLen, std::string *SizeBufferBlob, std::string *ObjectBufferBlob)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_BLOBS);

	return aux_frame_full_aux_write_paired_vec(oBuffer, &FrameType, PairedVecLen, SizeBufferBlob, ObjectBufferBlob);
}
