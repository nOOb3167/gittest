#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif _MSC_VER

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <memory>
#include <utility>  // std::move
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <deque>

#include <enet/enet.h>
#include <git2.h>

#include <gittest.h>

/*
* = Packet size vs Frame size =
* currently sizes are checked against Packet size, instead of the size field of the sent Frame.
* = Inferred size vs Explicit size for frame vector serialization =
*/

#define GS_AUX_MARKER_STRUCT_IS_COPYABLE /* dummy (marker / documentation purpose) */

#define GS_PORT 3756

#define GS_SERV_AUX_ARBITRARY_TIMEOUT_MS 5000
#define GS_CONNECT_NUMRETRY   5
#define GS_CONNECT_TIMEOUT_MS 1000
#define GS_CONNECT_NUMRECONNECT 5
#define GS_RECEIVE_TIMEOUT_MS 500000

#define GS_FRAME_HEADER_STR_LEN 40
#define GS_FRAME_HEADER_NUM_LEN 4
#define GS_FRAME_HEADER_LEN (GS_FRAME_HEADER_STR_LEN + GS_FRAME_HEADER_NUM_LEN)
#define GS_FRAME_SIZE_LEN 4

#define GS_PAYLOAD_OID_LEN 20

//#define GS_DBG_CLEAN {}
#define GS_DBG_CLEAN { assert(0); }
//#define GS_DBG_CLEAN { DebugBreak(); }

#define GS_ERR_CLEAN(THE_R) { r = (THE_R); GS_DBG_CLEAN; goto clean; }
#define GS_GOTO_CLEAN() { GS_DBG_CLEAN; goto clean; }
#define GS_ERR_CLEANSUB(THE_R) { r = (THE_R); GS_DBG_CLEAN; goto cleansub; }
#define GS_GOTO_CLEANSUB() { GS_DBG_CLEAN; goto cleansub; }

#define GS_FRAME_TYPE_SERV_AUX_INTERRUPT_REQUESTED 0
#define GS_FRAME_TYPE_REQUEST_LATEST_COMMIT_TREE 1
#define GS_FRAME_TYPE_RESPONSE_LATEST_COMMIT_TREE 2
#define GS_FRAME_TYPE_REQUEST_TREELIST 3
#define GS_FRAME_TYPE_RESPONSE_TREELIST 4
#define GS_FRAME_TYPE_REQUEST_TREES 5
#define GS_FRAME_TYPE_RESPONSE_TREES 6
#define GS_FRAME_TYPE_REQUEST_BLOBS 7
#define GS_FRAME_TYPE_RESPONSE_BLOBS 8

#define GS_FRAME_TYPE_DECL2(name) GS_FRAME_TYPE_ ## name
#define GS_FRAME_TYPE_DECL(name) { # name, GS_FRAME_TYPE_DECL2(name) }

struct GsFrameType {
	char mTypeName[GS_FRAME_HEADER_STR_LEN];
	uint32_t mTypeNum;
};

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

template<typename T>
using sp = ::std::shared_ptr<T>;

typedef ::std::shared_ptr<ENetPacket *> gs_packet_t;
typedef ::std::unique_ptr<ENetPacket *, void (*)(ENetPacket **)> gs_packet_unique_t;

gs_packet_unique_t gs_packet_unique_t_null();

class ServWorkerRequestData {
public:
	ServWorkerRequestData(gs_packet_unique_t *ioPacket, ENetHost *Host, ENetPeer *Peer)
		: mPacket(gs_packet_unique_t_null()),
		mHost(Host),
		mPeer(Peer)
	{
		mPacket = std::move(*ioPacket);
	}

public:
	gs_packet_unique_t mPacket;

private:
	ENetHost *mHost;
	ENetPeer *mPeer;

	friend int aux_make_serv_worker_request_data_for_response(
		ServWorkerRequestData *RequestBeingResponded, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oServWorkerRequestData);
	friend void aux_serv_worker_request_data_getprivate(ServWorkerRequestData *Request, ENetHost **oHost, ENetPeer **oPeer);
};

class ServWorkerData {
public:
	ServWorkerData()
		: mWorkerQueue(new std::deque<sp<ServWorkerRequestData> >),
		mWorkerDataMutex(new std::mutex),
		mWorkerDataCond(new std::condition_variable)
	{}

	void RequestEnqueue(const sp<ServWorkerRequestData> &RequestData);
	void RequestDequeue(sp<ServWorkerRequestData> *oRequestData);
	void RequestDequeueAllOpt(std::deque<sp<ServWorkerRequestData> > *oRequestData);

private:
	sp<std::deque<sp<ServWorkerRequestData> > > mWorkerQueue;
	sp<std::mutex> mWorkerDataMutex;
	sp<std::condition_variable> mWorkerDataCond;
};

class ServAuxData {
public:
	ServAuxData()
		: mInterruptRequested(0),
		mAuxDataMutex(new std::mutex),
		mAuxDataCond(new std::condition_variable)
	{}

	void InterruptRequestedEnqueue();
	bool InterruptRequestedDequeueTimeout(const std::chrono::milliseconds &WaitForMillis);

private:

	void InterruptRequestedDequeueMT_();

private:
	int mInterruptRequested;
	sp<std::mutex> mAuxDataMutex;
	sp<std::condition_variable> mAuxDataCond;
};

class FullConnectionClient {
public:
	FullConnectionClient(const sp<std::thread> &ThreadAux, const sp<std::thread> &Thread)
		: ThreadAux(ThreadAux),
		Thread(Thread)
	{}

private:
	sp<std::thread> ThreadAux;
	sp<std::thread> Thread;
};

int aux_packet_full_send(ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags);
int aux_packet_response_queue_interrupt_request_reliable(ServAuxData *ServAuxData, ServWorkerData *WorkerDataSend, ServWorkerRequestData *Request, const char *Data, uint32_t DataSize);

gs_packet_t aux_gs_make_packet(ENetPacket *packet) {
	return gs_packet_t(new ENetPacket *(packet), [](ENetPacket **xpacket) { enet_packet_destroy(*xpacket); delete xpacket; });
}

gs_packet_unique_t aux_gs_make_packet_unique(ENetPacket *packet) {
	return gs_packet_unique_t(new ENetPacket *(packet), [](ENetPacket **xpacket) { enet_packet_destroy(*xpacket); delete xpacket; });
}

gs_packet_unique_t gs_packet_unique_t_null() {
	return gs_packet_unique_t(nullptr, [](ENetPacket **xpacket) { /* dummy */ });
}

int aux_make_serv_worker_request_data(ENetHost *host, ENetPeer *peer, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oServWorkerRequestData) {
	int r = 0;

	sp<ServWorkerRequestData> ServWorkerRequestData(new ServWorkerRequestData(ioPacket, host, peer));

	if (oServWorkerRequestData)
		*oServWorkerRequestData = ServWorkerRequestData;

clean:

	return r;
}

int aux_make_serv_worker_request_data_for_response(
	ServWorkerRequestData *RequestBeingResponded, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oServWorkerRequestData)
{
	int r = 0;

	sp<ServWorkerRequestData> ServWorkerRequestData(new ServWorkerRequestData(
		ioPacket, RequestBeingResponded->mHost, RequestBeingResponded->mPeer));

	if (oServWorkerRequestData)
		*oServWorkerRequestData = ServWorkerRequestData;

clean:

	return r;
}

void aux_serv_worker_request_data_getprivate(ServWorkerRequestData *Request, ENetHost **oHost, ENetPeer **oPeer) {

	if (oHost)
		*oHost = Request->mHost;

	if (oPeer)
		*oPeer = Request->mPeer;
}

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

int aux_frame_read_buf(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew, uint8_t *Buf, uint32_t BufLen) {
	int r = 0;
	if (!!(r = aux_frame_enough_space(DataLength, Offset, BufLen)))
		GS_GOTO_CLEAN();
	memcpy(Buf, DataStart + Offset, BufLen);
	if (OffsetNew)
		*OffsetNew = Offset + BufLen;
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
	uint32_t SizeOfSize, uint32_t *oSize)
{
	int r = 0;
	uint32_t Size = 0;
	if (!!(r = aux_frame_enough_space(DataLength, Offset, SizeOfSize)))
		GS_GOTO_CLEAN();
	aux_LE_to_uint32(&Size, (char *)(DataStart + Offset), SizeOfSize);
	if (oSize)
		*oSize = Size;
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
	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &SizeFound)))
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
	if (!!(r = aux_frame_read_buf(DataStart, DataLength, Offset, &Offset, (uint8_t *)FrameType.mTypeName, GS_FRAME_HEADER_STR_LEN)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_HEADER_NUM_LEN, &FrameType.mTypeNum)))
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

int aux_frame_ensure_frametype(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew, const GsFrameType &FrameType) {
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

int aux_frame_read_oid(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	git_oid *oOid)
{
	int r = 0;
	git_oid Oid = {};
	uint8_t OidBuf[GIT_OID_RAWSZ] = {};
	assert(GS_PAYLOAD_OID_LEN == GIT_OID_RAWSZ);
	if (!!(r = aux_frame_read_buf(DataStart, DataLength, Offset, &Offset, OidBuf, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();
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

int aux_frame_write_oid(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
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

int aux_frame_read_oid_vec(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	std::vector<git_oid> *oOidVec)
{
	int r = 0;
	std::vector<git_oid> OidVec;
	uint32_t OidNum = 0;
	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &OidNum)))
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

int aux_frame_write_oid_vec(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew,
	git_oid *oOidVec, uint32_t OidNum, uint32_t OidSize)
{
	int r = 0;
	if (!!(r = aux_frame_write_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, OidNum)))
		GS_GOTO_CLEAN();
	assert(OidSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);
	for (uint32_t i = 0; i < OidNum; i++) {
		if (!!(r = aux_frame_write_oid(DataStart, DataLength, Offset, &Offset, (oOidVec + i)->id, OidSize)))
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
	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + GS_PAYLOAD_OID_LEN);
	uint32_t Offset = 0;
	if (!!(r = aux_frame_write_frametype((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();
	assert(OidSize == GIT_OID_RAWSZ && GIT_OID_RAWSZ == GS_PAYLOAD_OID_LEN);
	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, OidSize)))
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
	GsFrameType *FrameType, std::vector<git_oid> *OidVec)
{
	int r = 0;

	std::string Buffer;
	uint32_t PayloadSize = 0;
	uint32_t Offset = 0;

	PayloadSize = GS_FRAME_SIZE_LEN + GS_PAYLOAD_OID_LEN * OidVec->size();
	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + PayloadSize);
	assert(GS_PAYLOAD_OID_LEN == GIT_OID_RAWSZ);

	if (!!(r = aux_frame_write_frametype((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, FrameType)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, PayloadSize)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_frame_write_oid_vec((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, OidVec->data(), OidVec->size(), GIT_OID_RAWSZ)))
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

	if (!!(r = aux_frame_read_size(DataStart, DataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &PairedVecLen)))
		GS_GOTO_CLEAN();

	OffsetSizeBuffer = Offset;

	OffsetObjectBuffer = Offset + GS_FRAME_SIZE_LEN * PairedVecLen;

	if (oPairedVecLen)
		*oPairedVecLen = PairedVecLen;
	if (oOffsetSizeBuffer)
		*oOffsetSizeBuffer = OffsetSizeBuffer;
	if (oOffsetObjectBuffer)
		*oOffsetObjectBuffer = OffsetObjectBuffer;
	if (OffsetNew)
		*OffsetNew = Offset;
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
	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + 0);
	uint32_t Offset = 0;
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
	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + 0);
	uint32_t Offset = 0;
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
	return aux_frame_full_aux_write_oid_vec(oBuffer, &FrameType, OidVec);
}

int aux_frame_full_write_request_trees(
	std::string *oBuffer,
	std::vector<git_oid> *OidVec)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(REQUEST_TREES);
	return aux_frame_full_aux_write_oid_vec(oBuffer, &FrameType, OidVec);
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
	return aux_frame_full_aux_write_oid_vec(oBuffer, &FrameType, OidVec);
}

int aux_frame_full_write_response_blobs(
	std::string *oBuffer,
	uint32_t PairedVecLen, std::string *SizeBufferBlob, std::string *ObjectBufferBlob)
{
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_BLOBS);
	return aux_frame_full_aux_write_paired_vec(oBuffer, &FrameType, PairedVecLen, SizeBufferBlob, ObjectBufferBlob);
}

/* FIXME: race condition between server startup and client connection.
 *   connect may send packet too early to be seen. subsequently enet_host_service call here will timeout.
 *   the fix is having the connect be retried multiple times. */
int aux_connect_ensure_timeout(ENetHost *client, uint32_t TimeoutMs, uint32_t *oHasTimedOut) {
	int r = 0;

	ENetEvent event = {};

	int retcode = 0;

	if ((retcode = enet_host_service(client, &event, TimeoutMs)) < 0)
		GS_ERR_CLEAN(1);

	assert(retcode >= 0);

	if (retcode > 0 && event.type != ENET_EVENT_TYPE_CONNECT)
		GS_ERR_CLEAN(2);

	if (oHasTimedOut)
		*oHasTimedOut = (retcode == 0);

clean:

	return r;
}

void ServWorkerData::RequestEnqueue(const sp<ServWorkerRequestData> &RequestData) {
	{
		std::unique_lock<std::mutex> lock(*mWorkerDataMutex);
		mWorkerQueue->push_back(RequestData);
	}
	mWorkerDataCond->notify_one();
}

void ServWorkerData::RequestDequeue(sp<ServWorkerRequestData> *oRequestData) {
	sp<ServWorkerRequestData> RequestData;
	{
		std::unique_lock<std::mutex> lock(*mWorkerDataMutex);
		mWorkerDataCond->wait(lock, [&]() { return !mWorkerQueue->empty(); });
		assert(! mWorkerQueue->empty());
		RequestData = mWorkerQueue->front();
		mWorkerQueue->pop_front();
	}
	if (oRequestData)
		oRequestData->swap(RequestData);
}

void ServWorkerData::RequestDequeueAllOpt(std::deque<sp<ServWorkerRequestData> > *oRequestData) {
	{
		std::unique_lock<std::mutex> lock(*mWorkerDataMutex);
		oRequestData->clear();
		oRequestData->swap(*mWorkerQueue);
	}
}

void ServAuxData::InterruptRequestedEnqueue() {
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		mInterruptRequested = true;
	}
	mAuxDataCond->notify_one();
}

bool ServAuxData::InterruptRequestedDequeueTimeout(const std::chrono::milliseconds &WaitForMillis) {
	/* @return: Interrupt (aka send message from serv_aux to serv counts as requested
	*    if a thread sets mInterruptRequested and notifies us, or timeout expires but
	*    mInterruptRequested still got set */

	bool IsPredicateTrue = false;
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		IsPredicateTrue = mAuxDataCond->wait_for(lock, WaitForMillis, [&]() { return !!mInterruptRequested; });
		if (IsPredicateTrue)
			InterruptRequestedDequeueMT_();
	}
	return IsPredicateTrue;
}

void ServAuxData::InterruptRequestedDequeueMT_() {
	mInterruptRequested = false;
}

int serv_worker_thread_func(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend) {
	int r = 0;

	git_repository *Repository = NULL;

	const char *ConfRefName = aux_config_key(ServKeyVal, "RefName");
	const char *ConfRepoOpenPath = aux_config_key(ServKeyVal, "ConfRepoOpenPath");

	if (!ConfRefName || !ConfRepoOpenPath)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_repository_open(ConfRepoOpenPath, &Repository)))
		GS_GOTO_CLEAN();

	while (true) {
		sp<ServWorkerRequestData> Request;

		WorkerDataRecv->RequestDequeue(&Request);

		ENetPacket * const &Packet = *Request->mPacket;

		uint32_t OffsetStart = 0;
		uint32_t OffsetSize = 0;

		GsFrameType FoundFrameType = {};

		if (!!(r = aux_frame_read_frametype(Packet->data, Packet->dataLength, OffsetStart, &OffsetSize, &FoundFrameType)))
			GS_GOTO_CLEAN();

		printf("[worker] packet received [%.*s]\n", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);

		switch (FoundFrameType.mTypeNum)
		{
		case GS_FRAME_TYPE_REQUEST_LATEST_COMMIT_TREE:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid CommitHeadOid = {};
			git_oid TreeHeadOid = {};

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(Repository, ConfRefName, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_latest_commit_tree(&ResponseBuffer, TreeHeadOid.id, GIT_OID_RAWSZ)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(), ResponseBuffer.data(), ResponseBuffer.size())))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_TREELIST:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid TreeOid = {};
			std::vector<git_oid> Treelist;

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, &TreeOid)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_oid_treelist(Repository, &TreeOid, &Treelist)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_treelist(&ResponseBuffer, &Treelist)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(), ResponseBuffer.data(), ResponseBuffer.size())))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_TREES:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			uint32_t IgnoreSize = 0;
			std::vector<git_oid> TreelistRequested;
			std::string SizeBufferTree;
			std::string ObjectBufferTree;
			uint32_t PairedVecLen = 0;

			if (!!(r = aux_frame_read_size(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &IgnoreSize)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid_vec(Packet->data, Packet->dataLength, Offset, &Offset, &TreelistRequested)))
				GS_GOTO_CLEAN();

			PairedVecLen = TreelistRequested.size();

			if (!!(r = serv_serialize_trees(Repository, &TreelistRequested, &SizeBufferTree, &ObjectBufferTree)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_trees(&ResponseBuffer, PairedVecLen, &SizeBufferTree, &ObjectBufferTree)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(), ResponseBuffer.data(), ResponseBuffer.size())))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_BLOBS:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			uint32_t IgnoreSize = 0;
			std::vector<git_oid> BloblistRequested;
			std::string SizeBufferBlob;
			std::string ObjectBufferBlob;
			uint32_t PairedVecLen = 0;

			if (!!(r = aux_frame_read_size(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &IgnoreSize)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid_vec(Packet->data, Packet->dataLength, Offset, &Offset, &BloblistRequested)))
				GS_GOTO_CLEAN();

			PairedVecLen = BloblistRequested.size();

			if (!!(r = serv_serialize_blobs(Repository, &BloblistRequested, &SizeBufferBlob, &ObjectBufferBlob)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_blobs(&ResponseBuffer, PairedVecLen, &SizeBufferBlob, &ObjectBufferBlob)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_response_queue_interrupt_request_reliable(
				ServAuxData.get(), WorkerDataSend.get(), Request.get(), ResponseBuffer.data(), ResponseBuffer.size())))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		default:
		{
			printf("[worker] unknown frametype received [%.*s]\n", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);
			if (1)
				GS_ERR_CLEAN(1);
		}
		break;
		}

	}

clean:
	if (Repository)
		git_repository_free(Repository);

	return r;
}

int aux_enet_host_create_serv(uint32_t EnetAddressPort, ENetHost **oServer) {
	int r = 0;

	ENetAddress address = {};
	ENetHost *host = NULL;

	address.host = ENET_HOST_ANY;
	address.port = EnetAddressPort;

	if (!(host = enet_host_create(&address, 256, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (oServer)
		*oServer = host;

clean:
	if (!!r) {
		if (host)
			enet_host_destroy(host);
	}

	return r;
}

int aux_enet_host_client_create_addr(ENetHost **oHost, ENetAddress *oAddressHost) {
	/**
	* https://msdn.microsoft.com/en-us/library/windows/desktop/ms738543(v=vs.85).aspx
	* To apply the INTERRUPT_REQUESTED workaround to a socket,
	* a local connection (from a second socket) is required.
	* To establish that local connection, the socket must have an address assigned.
	* Address assignment occurs if the first socket is bound,
	* either explicitly (call to bind) or 'implicitly'
	* (call to connect, (also recvfrom and some others?)).
	* The socket being bound allows its address to be retrieved via getsockname.
	* enet codepath through enet_host_create only calls bind and getsockname if
	* the ENetAddress parameter is specified (non-null).
	*
	* As we are creating a client socket, we do not wish to bind to a specific port.
	* (to allow multiple client connections be established from the same host, for example)
	* To 'request' a dynamically assigned port via the ENetAddress structure,
	* set port value as ENET_PORT_ANY (aka zero).
	*
	* Once enet_host_create completes, the assigned port can be retrieved via
	* ENetHost->address.
	*/

	int r = 0;

	ENetAddress address = {};
	ENetHost *host = NULL;

	address.host = ENET_HOST_ANY;
	address.port = ENET_PORT_ANY;

	// FIXME: two peer limit (for connection, and for INTERRUPT_REQUESTED workaround local connection)
	if (!!(host = enet_host_create(&address, 2, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (oHost)
		*oHost = host;

	if (oAddressHost)
		*oAddressHost = host->address;

clean:
	if (!!r) {
		if (host)
			enet_host_destroy(host);
	}

	return r;
}

int aux_enet_host_create_connect_addr(
	ENetAddress *address,
	ENetHost **oHost, ENetPeer **oPeer)
{
	int r = 0;

	ENetHost *host = NULL;
	ENetPeer *peer = NULL;

	if (!(host = enet_host_create(NULL, 1, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (!(peer = enet_host_connect(host, address, 1, 0)))
		GS_ERR_CLEAN(1);

	if (oHost)
		*oHost = host;

	if (oPeer)
		*oPeer = peer;

clean:
	if (!!r) {
		if (peer)
			enet_peer_disconnect_now(peer, NULL);

		if (host)
			enet_host_destroy(host);
	}

	return r;
}

int aux_enet_address_create_ip(
	uint32_t EnetAddressPort, uint32_t EnetAddressHostNetworkByteOrder,
	ENetAddress *oAddress)
{
	ENetAddress address;

	address.host = EnetAddressHostNetworkByteOrder;
	address.port = EnetAddressPort;

	if (oAddress)
		*oAddress = address;

	return 0;
}

int aux_enet_address_create_hostname(
	uint32_t EnetAddressPort, const char *EnetHostName,
	ENetAddress *oAddress)
{
	int r = 0;

	ENetAddress address = {};

	if (!!(r = enet_address_set_host(&address, EnetHostName)))
		GS_ERR_CLEAN(1);
	address.port = EnetAddressPort;

	if (oAddress)
		*oAddress = address;

clean:

	return r;
}

int aux_packet_full_send(ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags) {
	int r = 0;

	/* only flag expected to be useful with this function is ENET_PACKET_FLAG_RELIABLE, really */
	assert((EnetPacketFlags & ~(ENET_PACKET_FLAG_RELIABLE)) == 0);

	ENetPacket *packet = NULL;

	if (!(packet = enet_packet_create(Data, DataSize, EnetPacketFlags)))
		GS_ERR_CLEAN(1);

	if (!!(r = enet_peer_send(peer, 0, packet)))
		GS_GOTO_CLEAN();
	packet = NULL;  /* lost ownership after enet_peer_send */

	enet_host_flush(host);

	ServAuxData->InterruptRequestedEnqueue();

clean:
	if (packet)
		enet_packet_destroy(packet);

	return r;
}

int aux_packet_response_queue_interrupt_request_reliable(ServAuxData *ServAuxData, ServWorkerData *WorkerDataSend, ServWorkerRequestData *Request, const char *Data, uint32_t DataSize) {
	int r = 0;

	ENetPacket *Packet = NULL;
	gs_packet_unique_t GsPacket = gs_packet_unique_t_null();
	sp<ServWorkerRequestData> RequestResponseData;

	if (!(Packet = enet_packet_create(Data, DataSize, ENET_PACKET_FLAG_RELIABLE)))
		GS_ERR_CLEAN(1);

	GsPacket = aux_gs_make_packet_unique(Packet);
	Packet = NULL; /* lost ownership */

	if (!!(r = aux_make_serv_worker_request_data_for_response(Request, &GsPacket, &RequestResponseData)))
		GS_GOTO_CLEAN();

	WorkerDataSend->RequestEnqueue(RequestResponseData);

	ServAuxData->InterruptRequestedEnqueue();

clean:
	if (!!r) {
		if (Packet)
			enet_packet_destroy(Packet);
	}

	return r;
}

int aux_host_service_one_type_receive(ENetHost *host, uint32_t TimeoutMs, gs_packet_t *oPacket) {
	/* NOTE: special errorhandling */

	int r = 0;

	int retcode = 0;

	ENetEvent event = {};
	gs_packet_t Packet;

	retcode = enet_host_service(host, &event, TimeoutMs);

	if (retcode > 0 && event.type != ENET_EVENT_TYPE_RECEIVE)
		GS_ERR_CLEAN(1);

	Packet = aux_gs_make_packet(event.packet);

	if (oPacket)
		*oPacket = Packet;

clean:

	return r;
}

int aux_host_service(ENetHost *host, uint32_t TimeoutMs, std::vector<ENetEvent> *oEvents) {
	/* http://lists.cubik.org/pipermail/enet-discuss/2012-June/001927.html */

	/* NOTE: special errorhandling */

	int retcode = 0;

	std::vector<ENetEvent> Events;
	
	ENetEvent event;

	for ((retcode = enet_host_service(host, &event, TimeoutMs))
		; retcode > 0
		; (retcode = enet_host_check_events(host, &event)))
	{
		// FIXME: copies an ENetEvent structure. not sure if part of the official enet API.
		Events.push_back(event);
	}

	if (oEvents)
		oEvents->swap(Events);

	return retcode < 0;
}

int serv_aux_host_service(ENetHost *client) {
	int r = 0;

	std::vector<ENetEvent> Events;

	if (!!(r = aux_host_service(client, 0, &Events)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < Events.size(); i++) {
		switch (Events[i].type)
		{
		case ENET_EVENT_TYPE_CONNECT:
		case ENET_EVENT_TYPE_DISCONNECT:
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			assert(0);
			enet_packet_destroy(Events[i].packet);
			break;
		}
	}

clean:

	return r;
}

int serv_host_service(ENetHost *server, const sp<ServWorkerData> &WorkerDataRecv, const sp<ServWorkerData> &WorkerDataSend) {
	int r = 0;

	std::vector<ENetEvent> Events;

	if (!!(r = aux_host_service(server, GS_SERV_AUX_ARBITRARY_TIMEOUT_MS, &Events)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < Events.size(); i++) {
		switch (Events[i].type)
		{
		case ENET_EVENT_TYPE_CONNECT:
		{
			printf("[serv] A new client connected from %x:%u.\n",
				Events[i].peer->address.host,
				Events[i].peer->address.port);
			Events[i].peer->data = "Client information";
		}
		break;

		case ENET_EVENT_TYPE_RECEIVE:
		{
			ENetPeer *peer = Events[i].peer;

			const GsFrameType &FrameTypeInterruptRequested = GS_FRAME_TYPE_DECL(SERV_AUX_INTERRUPT_REQUESTED);
			GsFrameType FoundFrameType = {};

			if (!!(r = aux_frame_read_frametype(Events[i].packet->data, Events[i].packet->dataLength, 0, NULL, &FoundFrameType)))
				GS_GOTO_CLEAN();

			/* filter out interrupt requested frames and only dispatch other */

			if (! aux_frametype_equals(FoundFrameType, FrameTypeInterruptRequested)) {
				
				printf("[serv] packet received\n");

				gs_packet_unique_t Packet = aux_gs_make_packet_unique(Events[i].packet);

				sp<ServWorkerRequestData> ServWorkerRequestData;

				if (!!(r = aux_make_serv_worker_request_data(server, peer, &Packet, &ServWorkerRequestData)))
					GS_GOTO_CLEAN();

				WorkerDataRecv->RequestEnqueue(ServWorkerRequestData);
			}

			/* check out if any send requests need servicing */

			{
				std::deque<sp<ServWorkerRequestData> > RequestedSends;

				WorkerDataSend->RequestDequeueAllOpt(&RequestedSends);

				for (uint32_t i = 0; i < RequestedSends.size(); i++) {
					ENetHost *GotHost = NULL;
					ENetPeer *GotPeer = NULL;

					aux_serv_worker_request_data_getprivate(RequestedSends[i].get(), &GotHost, &GotPeer);

					// FIXME: assuming reconnection is a thing, how to assure host and peer are still valid?
					//   likely want to clear outstanding requests on the worker queues before a reconnect.
					//   for now at least assure host from the request is the same as passed to this function.
					assert(GotHost == server);

					/* ownership of packet is lost after enet_peer_send */
					ENetPacket *Packet = *RequestedSends[i]->mPacket.release();

					if (enet_peer_send(GotPeer, 0, Packet) < 0)
						GS_GOTO_CLEAN();
				}

				/* absolutely no reason to flush if nothing was sent */
				/* notice we are flushing 'server', above find an assert equaling against RequestedSends host */

				if (RequestedSends.size())
					enet_host_flush(server);
			}
		}
		break;

		case ENET_EVENT_TYPE_DISCONNECT:
		{
			printf("[serv] %s disconnected.\n", Events[i].peer->data);
			Events[i].peer->data = NULL;
		}
		break;

		}
	}

clean:

	return r;
}

int aux_host_connect(
	ENetAddress *address,
	uint32_t NumRetry, uint32_t RetryTimeoutMs,
	ENetHost **oClient, ENetPeer **oPeer)
{
	int r = 0;

	ENetHost *nontimedout_client = NULL;
	ENetPeer *nontimedout_peer = NULL;

	for (uint32_t i = 0; i < NumRetry; i++) {
		ENetHost *client = NULL;
		ENetPeer *peer = NULL;
		uint32_t HasTimedOut = 0;

		if (!!(r = aux_enet_host_create_connect_addr(address, &client, &peer)))
			GS_GOTO_CLEANSUB();

		if (!!(r = aux_connect_ensure_timeout(client, RetryTimeoutMs, &HasTimedOut)))
			GS_GOTO_CLEANSUB();

		if (!HasTimedOut) {
			nontimedout_client = client;
			nontimedout_peer = peer;
			break;
		}

	cleansub:
		if (!!r || HasTimedOut) {
			if (peer)
				enet_peer_disconnect_now(peer, NULL);
			if (client)
				enet_host_destroy(client);
		}
		if (!!r)
			GS_GOTO_CLEAN();
	}

	if (!nontimedout_client || !nontimedout_peer)
		GS_ERR_CLEAN(1);

	if (oClient)
		*oClient = nontimedout_client;

	if (oPeer)
		*oPeer = nontimedout_peer;

clean:
	if (!!r) {
		if (nontimedout_peer)
			enet_peer_disconnect_now(nontimedout_peer, NULL);
		if (nontimedout_client)
			enet_host_destroy(nontimedout_client);
	}

	return r;
}

int aux_serv_aux_thread_func(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, ENetAddress address /* by val */) {
	int r = 0;

	std::string BufferFrameInterruptRequested;

	ENetHost *client = NULL;
	ENetPeer *peer = NULL;

	if (!!(r = aux_frame_full_write_serv_aux_interrupt_requested(&BufferFrameInterruptRequested)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &client, &peer)))
		GS_GOTO_CLEAN();

	while (true) {

		if (!!(r = serv_aux_host_service(client)))
			GS_GOTO_CLEAN();

		/* set a timeout to ensure serv_aux_host_service cranks the enet event loop regularly */

		bool IsInterruptRequested = ServAuxData->InterruptRequestedDequeueTimeout(
			std::chrono::milliseconds(GS_SERV_AUX_ARBITRARY_TIMEOUT_MS));

		if (IsInterruptRequested) {
			/* NOTE: searching enet source for uses of ENET_PACKET_FLAG_NO_ALLOCATE turns out a fun easter egg:
			*   enet_packet_resize essentially chokes and sets new size without validation so never call that */

			ENetPacket *packet = enet_packet_create(
				BufferFrameInterruptRequested.data(), BufferFrameInterruptRequested.size(), ENET_PACKET_FLAG_NO_ALLOCATE);

			/* NOTE: enet tutorial claims that enet_packet_destroy need not be called after packet handoff via enet_peer_send.
			*   but reading enet source reveals obvious leaks on some error conditions. (undistinguishable observing return code) */

			if (enet_peer_send(peer, 0, packet) < 0)
				GS_ERR_CLEAN(1);

			/* enet packet sends are ensured sufficiently by enet_host_flush. only receives require serv_aux_host_service.
			* however serv_aux send only, does not really receive anything. serv_aux_host_service just helps crank internal
			* enet state such as acknowledgment packets.
			* FIXME: refactor to only call serv_aux_host_service every GS_SERV_AUX_ARBITRARY_TIMEOUT_MS ms
			*   instead of after every IsInterruptRequested. */

			enet_host_flush(client);
		}
	}

clean:

	return r;
}

int serv_serv_aux_thread_func(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;

	uint32_t ServPort = 0;
	uint32_t ServHostIp = ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24);
	ENetAddress address = {};

	if (!!(r = aux_config_key_uint32(ServKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	assert(ServHostIp == ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24));

	if (!!(r = aux_enet_address_create_ip(ServPort, ServHostIp, &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_serv_aux_thread_func(ServKeyVal, ServAuxData, address)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int serv_thread_func(const confmap_t &ServKeyVal, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend) {
	int r = 0;

	ENetHost *server = NULL;

	uint32_t ServPort = 0;

	if (!!(r = aux_config_key_uint32(ServKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_host_create_serv(ServPort, &server)))
		GS_GOTO_CLEAN();

	while (true) {
		if (!!(r = serv_host_service(server, WorkerDataRecv, WorkerDataSend)))
			GS_GOTO_CLEAN();
	}

clean:
	if (server)
		enet_host_destroy(server);

	return r;
}

typedef std::pair<ENetHost *, ENetPeer *> gs_host_peer_pair_t;

struct PacketWithOffset {
	gs_packet_t mPacket;
	uint32_t mOffsetSize;
	uint32_t mOffsetObject;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

int aux_make_packet_with_offset(gs_packet_t Packet, uint32_t OffsetSize, uint32_t OffsetObject, PacketWithOffset *oPacketWithOffset) {
	PacketWithOffset ret;
	ret.mPacket = Packet;
	ret.mOffsetSize = OffsetSize;
	ret.mOffsetObject = OffsetObject;
	if (oPacketWithOffset)
		*oPacketWithOffset = ret;
	return 0;
}

struct ClntStateReconnect {
	uint32_t NumReconnections;
	uint32_t NumReconnectionsLeft;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

struct ClntState {
	sp<git_repository *> mRepositoryT;

	sp<gs_host_peer_pair_t> mConnection;

	sp<git_oid> mTreeHeadOid;

	sp<std::vector<git_oid> > mTreelist;
	sp<std::vector<git_oid> > mMissingTreelist;

	sp<std::vector<git_oid> > mMissingBloblist;
	sp<PacketWithOffset>      mTreePacketWithOffset;

	sp<std::vector<git_oid> > mWrittenBlob;
	sp<std::vector<git_oid> > mWrittenTree;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

#define GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(PTR_VARNAME_CLNTSTATE, CODE, VARNAME_TMPSTATE, STATEMENTBLOCK) \
	{ ClntState VARNAME_TMPSTATE;                                                                       \
      if (!!clnt_state_cpy(& (VARNAME_TMPSTATE), (PTR_VARNAME_CLNTSTATE)))                              \
        GS_ERR_CLEAN(9998);                                                                             \
	  { STATEMENTBLOCK }                                                                                \
	  if (!!clnt_state_code_ensure(& (VARNAME_TMPSTATE), (CODE)))                                       \
	    GS_ERR_CLEAN(9999);                                                                             \
	  if (!!clnt_state_cpy((PTR_VARNAME_CLNTSTATE), & (VARNAME_TMPSTATE)))                              \
	    GS_ERR_CLEAN(9998); }

int clnt_state_reconnect_make_default(ClntStateReconnect *oStateReconnect) {
	ClntStateReconnect StateReconnect;
	StateReconnect.NumReconnections = GS_CONNECT_NUMRECONNECT;
	StateReconnect.NumReconnectionsLeft = StateReconnect.NumReconnections;
	if (oStateReconnect)
		*oStateReconnect = StateReconnect;
	return 0;
}

int clnt_state_make_default(ClntState *oState) {
	ClntState State;
	if (oState)
		*oState = State;
	return 0;
}

int clnt_state_cpy(ClntState *dst, const ClntState *src) {
	*dst = *src;
	return 0;
}

int clnt_state_code(ClntState *State, uint32_t *oCode) {
	int r = 0;
	
	int Code = 0;

	if (! State->mRepositoryT)
		{ Code = 0; goto s0; }
	if (! State->mConnection)
		{ Code = 1; goto s1; }
	if (! State->mTreeHeadOid)
		{ Code = 2; goto s2; }
	if (! State->mTreelist || ! State->mMissingTreelist)
		{ Code = 3; goto s3; }
	if (! State->mMissingBloblist || ! State->mTreePacketWithOffset)
		{ Code = 4; goto s4; }
	if (! State->mWrittenBlob || ! State->mWrittenTree)
		{ Code = 5; goto s5; }
	if (true)
		{ Code = 6; goto s6; }

s0:
	if (State->mConnection)
		GS_ERR_CLEAN(1);
s1:
	if (State->mTreeHeadOid)
		GS_ERR_CLEAN(1);
s2:
	if (State->mTreelist || State->mMissingTreelist)
		GS_ERR_CLEAN(1);
s3:
	if (State->mMissingBloblist || State->mTreePacketWithOffset)
		GS_ERR_CLEAN(1);
s4:
	if (State->mWrittenBlob || State->mWrittenTree)
		GS_ERR_CLEAN(1);
s5:
s6:

	if (oCode)
		*oCode = Code;

clean:

	return r;
}

int clnt_state_code_ensure(ClntState *State, uint32_t WantedCode) {
	int r = 0;

	uint32_t FoundCode = 0;

	if (!!(r = clnt_state_code(State, &FoundCode)))
		GS_GOTO_CLEAN();

	if (WantedCode != FoundCode)
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int clnt_state_0_noown(const char *ConfRepoTOpenPath, git_repository **oRepositoryT) {
	int r = 0;

	if (!!(r = aux_repository_open(ConfRepoTOpenPath, oRepositoryT)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_1_noown(uint32_t ConfServPort, const char *ConfServHostName, ENetHost **oHost, ENetPeer **oPeer) {
	int r = 0;

	ENetAddress address = {};

	if (!!(r = aux_enet_address_create_hostname(ConfServPort, ConfServHostName, &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, oHost, oPeer)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_2_noown(
	const char *ConfRefName, git_repository *RepositoryT, ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData,
	git_oid *oTreeHeadOid)
{
	int r = 0;

	std::string Buffer;
	gs_packet_t GsPacket = aux_gs_make_packet(NULL);
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	if (!!(r = aux_frame_full_write_request_latest_commit_tree(&Buffer)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_packet_full_send(host, peer, ServAuxData, Buffer.data(), Buffer.size(), 0)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_service_one_type_receive(host, GS_RECEIVE_TIMEOUT_MS, &GsPacket)))
		GS_GOTO_CLEAN();

	ENetPacket * const &Packet = *GsPacket;

	if (! Packet)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, oTreeHeadOid)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_latest_commit_tree_oid(RepositoryT, ConfRefName, &CommitHeadOidT, &TreeHeadOidT)))
		GS_GOTO_CLEAN();

	if (git_oid_cmp(&TreeHeadOidT, oTreeHeadOid) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &CommitHeadOidT);
		printf("[clnt] Have latest [%.*s]\n", GIT_OID_HEXSZ, buf);
	}

clean:

	return r;
}

int clnt_state_3_noown(
	git_repository *RepositoryT, ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData,
	git_oid *TreeHeadOid, std::vector<git_oid> *oTreelist, std::vector<git_oid> *oMissingTreelist)
{
	int r = 0;

	std::string Buffer;
	gs_packet_t GsPacket = aux_gs_make_packet(NULL);
	uint32_t Offset = 0;
	uint32_t IgnoreSize = 0;

	if (!!(r = aux_frame_full_write_request_treelist(&Buffer, TreeHeadOid->id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_packet_full_send(host, peer, ServAuxData, Buffer.data(), Buffer.size(), 0)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_service_one_type_receive(host, GS_RECEIVE_TIMEOUT_MS, &GsPacket)))
		GS_GOTO_CLEAN();

	ENetPacket * const &Packet = *GsPacket;

	if (! Packet)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREELIST))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &IgnoreSize)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, Packet->dataLength, Offset, &Offset, oTreelist)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_missing_trees(RepositoryT, oTreelist, oMissingTreelist)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int clnt_state_4_noown(
	git_repository *RepositoryT, ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData,
	std::vector<git_oid> *MissingTreelist,
	std::vector<git_oid> *oMissingBloblist, gs_packet_t *oPacketTree, uint32_t *oOffsetSizeBufferTree, uint32_t *oOffsetObjectBufferTree)
{
	int r = 0;

	std::string Buffer;
	uint32_t Offset = 0;
	uint32_t IgnoreSize = 0;

	uint32_t BufferTreeLen;

	if (!!(r = aux_frame_full_write_request_trees(&Buffer, MissingTreelist)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_packet_full_send(host, peer, ServAuxData, Buffer.data(), Buffer.size(), 0)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketTree Lifetime start */

	if (!!(r = aux_host_service_one_type_receive(host, GS_RECEIVE_TIMEOUT_MS, oPacketTree)))
		GS_GOTO_CLEAN();

	ENetPacket * const &PacketTree = **oPacketTree;

	if (! PacketTree)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(PacketTree->data, PacketTree->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_TREES))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size(PacketTree->data, PacketTree->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &IgnoreSize)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketTree Offsets use start */

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketTree->data, PacketTree->dataLength, Offset, &Offset,
		&BufferTreeLen, oOffsetSizeBufferTree, oOffsetObjectBufferTree)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_missing_blobs_bare(
		RepositoryT,
		PacketTree->data, PacketTree->dataLength, *oOffsetSizeBufferTree,
		PacketTree->data, PacketTree->dataLength, *oOffsetObjectBufferTree, MissingTreelist->size(), oMissingBloblist)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int clnt_state_5_noown(
	git_repository *RepositoryT, ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData,
	std::vector<git_oid> *MissingTreelist, std::vector<git_oid> *MissingBloblist,
	const gs_packet_t &GsPacketTree, uint32_t OffsetSizeBufferTree, uint32_t OffsetObjectBufferTree,
	std::vector<git_oid> *oWrittenBlob, std::vector<git_oid> *oWrittenTree)
{
	int r = 0;

	std::string Buffer;
	gs_packet_t GsPacketBlob;
	uint32_t Offset = 0;
	uint32_t IgnoreSize = 0;

	uint32_t BufferBlobLen;
	uint32_t OffsetSizeBufferBlob;
	uint32_t OffsetObjectBufferBlob;

	if (!!(r = aux_frame_full_write_request_blobs(&Buffer, MissingBloblist)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_packet_full_send(host, peer, ServAuxData, Buffer.data(), Buffer.size(), 0)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketBlob Lifetime start */

	if (!!(r = aux_host_service_one_type_receive(host, GS_RECEIVE_TIMEOUT_MS, &GsPacketBlob)))
		GS_GOTO_CLEAN();

	ENetPacket * const &PacketBlob = *GsPacketBlob;

	if (! PacketBlob)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_frame_ensure_frametype(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &IgnoreSize)))
		GS_GOTO_CLEAN();

	/* NOTE: NOALLOC - PacketBlob Offsets use start */

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset,
		&BufferBlobLen, &OffsetSizeBufferBlob, &OffsetObjectBufferBlob)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_deserialize_blobs(
		RepositoryT,
		PacketBlob->data, PacketBlob->dataLength, OffsetSizeBufferBlob,
		PacketBlob->data, PacketBlob->dataLength, OffsetObjectBufferBlob,
		MissingBloblist->size(), oWrittenBlob)))
	{
		GS_GOTO_CLEAN();
	}

	ENetPacket * const &PacketTree = *GsPacketTree;

	if (!!(r = clnt_deserialize_trees(
		RepositoryT,
		PacketTree->data, PacketTree->dataLength, OffsetSizeBufferTree,
		PacketTree->data, PacketTree->dataLength, OffsetObjectBufferTree,
		MissingTreelist->size(), oWrittenTree)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int clnt_state_0_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	sp<git_repository *> RepositoryT(new git_repository *);

	const char *ConfRepoTOpenPath = aux_config_key(ClntKeyVal, "ConfRepoTOpenPath");

	if (!ConfRepoTOpenPath)
		GS_ERR_CLEAN(1);

	if (!!(r = clnt_state_0_noown(ConfRepoTOpenPath, RepositoryT.get())))
		GS_GOTO_CLEAN();

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 1, a,
		{ a.mRepositoryT = RepositoryT; });

clean:
	if (!!r) {
		if (RepositoryT)
			git_repository_free(*RepositoryT);
	}

	return r;
}

int clnt_state_1_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	sp<gs_host_peer_pair_t> Connection(new gs_host_peer_pair_t(NULL, NULL));

	const char *ConfServHostName = aux_config_key(ClntKeyVal, "ConfServHostName");
	uint32_t ConfServPort = 0;

	if (!ConfServHostName)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_config_key_uint32(ClntKeyVal, "ConfServPort", &ConfServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_state_1_noown(ConfServPort, ConfServHostName, &Connection->first, &Connection->second)))
		GS_GOTO_CLEAN();

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 2, a,
		{ a.mConnection = Connection; });

clean:
	if (!!r) {
		if (Connection && Connection->second)
			enet_peer_disconnect_now(Connection->second, NULL);

		if (Connection && Connection->first)
			enet_host_destroy(Connection->first);
	}

	return r;
}

int clnt_state_2_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	sp<git_oid> TreeHeadOid(new git_oid);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<gs_host_peer_pair_t> &Connection = State->mConnection;
	const char *ConfRefName = aux_config_key(ClntKeyVal, "RefName");

	std::string Buffer;
	gs_packet_t Packet;
	uint32_t Offset = 0;

	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	if (!ConfRefName)
		GS_ERR_CLEAN(1);

	if (!!(r = clnt_state_2_noown(
		ConfRefName, RepositoryT, Connection->first, Connection->second, ServAuxData.get(),
		TreeHeadOid.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 3, a,
		{ a.mTreeHeadOid = TreeHeadOid; });

clean:

	return r;
}

int clnt_state_3_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	sp<std::vector<git_oid> > Treelist(new std::vector<git_oid>);
	sp<std::vector<git_oid> > MissingTreelist(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<gs_host_peer_pair_t> &Connection = State->mConnection;
	const sp<git_oid> &TreeHeadOid = State->mTreeHeadOid;

	if (!!(r = clnt_state_3_noown(
		RepositoryT, Connection->first, Connection->second, ServAuxData.get(),
		TreeHeadOid.get(), Treelist.get(), MissingTreelist.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 4, a,
		{ a.mTreelist = Treelist;
		  a.mMissingTreelist = MissingTreelist; });

clean:

	return r;
}

int clnt_state_4_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	sp<std::vector<git_oid> > MissingBloblist(new std::vector<git_oid>);
	sp<PacketWithOffset> PacketTreeWithOffset(new PacketWithOffset);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<gs_host_peer_pair_t> &Connection = State->mConnection;
	const sp<std::vector<git_oid> > &MissingTreelist = State->mMissingTreelist;

	gs_packet_t PacketTree = aux_gs_make_packet(NULL);

	uint32_t OffsetSizeBufferTree;
	uint32_t OffsetObjectBufferTree;

	sp<PacketWithOffset> TmpTreePacketWithOffset(new PacketWithOffset);

	if (!!(r = clnt_state_4_noown(
		RepositoryT, Connection->first, Connection->second, ServAuxData.get(),
		MissingTreelist.get(), MissingBloblist.get(), &PacketTree, &OffsetSizeBufferTree, &OffsetObjectBufferTree)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_make_packet_with_offset(PacketTree, OffsetSizeBufferTree, OffsetObjectBufferTree, TmpTreePacketWithOffset.get())))
		GS_GOTO_CLEAN();

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 5, a,
		{ a.mMissingBloblist = MissingBloblist;
		  a.mTreePacketWithOffset = TmpTreePacketWithOffset; });

clean:

	return r;
}

int clnt_state_5_setup(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	sp<std::vector<git_oid> > WrittenBlob(new std::vector<git_oid>);
	sp<std::vector<git_oid> > WrittenTree(new std::vector<git_oid>);

	git_repository * const RepositoryT = *State->mRepositoryT;
	const sp<gs_host_peer_pair_t> &Connection = State->mConnection;
	const sp<std::vector<git_oid> > &MissingTreelist = State->mMissingTreelist;
	const sp<std::vector<git_oid> > &MissingBloblist = State->mMissingBloblist;
	const sp<PacketWithOffset> &PacketTreeWithOffset = State->mTreePacketWithOffset;
	const gs_packet_t &PacketTree = PacketTreeWithOffset->mPacket;
	const uint32_t &OffsetSizeBufferTree = PacketTreeWithOffset->mOffsetSize;
	const uint32_t &OffsetObjectBufferTree = PacketTreeWithOffset->mOffsetObject;

	if (!!(r = clnt_state_5_noown(
		RepositoryT, Connection->first, Connection->second, ServAuxData.get(),
		MissingTreelist.get(), MissingBloblist.get(),
		PacketTree, OffsetSizeBufferTree, OffsetObjectBufferTree,
		WrittenBlob.get(), WrittenTree.get())))
	{
		GS_GOTO_CLEAN();
	}

	GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(State.get(), 6, a,
		{ a.mWrittenBlob = WrittenBlob;
		  a.mWrittenTree = WrittenTree; });

clean:

	return r;
}

int clnt_state_crank(const sp<ClntState> &State, const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData) {
	int r = 0;

	uint32_t Code = 0;

	if (!!(r = clnt_state_code(State.get(), &Code)))
		GS_GOTO_CLEAN();

	switch (Code) {
	case 0:
	{
		if (!!(r = clnt_state_0_setup(State, ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}
	break;

	case 1:
	{
		if (!!(r = clnt_state_1_setup(State, ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}
	break;

	case 2:
	{
		if (!!(r = clnt_state_2_setup(State, ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}
	break;

	case 3:
	{
		if (!!(r = clnt_state_3_setup(State, ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}
	break;

	case 4:
	{
		if (!!(r = clnt_state_4_setup(State, ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}
	break;

	case 5:
	{
		if (!!(r = clnt_state_5_setup(State, ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}
	break;

	default:
	{
		assert(0);
	}
	break;
	}

clean:

	return r;
}

int aux_host_peer_pair_reset(sp<gs_host_peer_pair_t> *ioConnection) {
	if (*ioConnection != NULL) {
		ENetHost * const oldhost = (*ioConnection)->first;
		ENetPeer * const oldpeer = (*ioConnection)->second;

		*ioConnection = sp<gs_host_peer_pair_t>();

		enet_peer_disconnect_now(oldpeer, NULL);
		enet_host_destroy(oldhost);
	}

	return 0;
}

int clnt_state_connection_remake(const confmap_t &ClntKeyVal, sp<gs_host_peer_pair_t> *ioConnection) {
	int r = 0;

	std::string ConfServHostName;
	uint32_t ConfServPort = 0;

	ENetAddress address = {};
	ENetHost *newhost = NULL;
	ENetPeer *newpeer = NULL;

	if (!!(r = aux_config_key_ex(ClntKeyVal, "ConfServHostName", &ConfServHostName)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_config_key_uint32(ClntKeyVal, "ConfServPort", &ConfServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_peer_pair_reset(ioConnection)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_address_create_hostname(ConfServPort, ConfServHostName.c_str(), &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &newhost, &newpeer)))
		GS_GOTO_CLEAN();

	if (ioConnection)
		*ioConnection = std::make_shared<gs_host_peer_pair_t>(newhost, newpeer);

clean:
	if (!!r) {
		if (newpeer)
			enet_peer_disconnect_now(newpeer, NULL);

		if (newhost)
			enet_host_destroy(newhost);
	}

	return r;
}

int clnt_state_crank_reconnecter(
	const sp<ClntState> &State, ClntStateReconnect *ioStateReconnect,
	const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData)
{
	int r = 0;

	if (!!(r = clnt_state_crank(State, ClntKeyVal, ServAuxData))) {
		printf("reco+\n");
		if (ioStateReconnect->NumReconnectionsLeft-- == 0)
			GS_GOTO_CLEAN();
		if (!!(r = clnt_state_connection_remake(ClntKeyVal, &State->mConnection)))
			GS_GOTO_CLEAN();
		printf("reco-\n");
	}

clean:

	return r;
}

int clnt_thread_func(const confmap_t &ClntKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;

	sp<ClntState> State(new ClntState);
	sp<ClntStateReconnect> StateReconnect(new ClntStateReconnect);

	if (!!(r = clnt_state_make_default(State.get())))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_state_reconnect_make_default(StateReconnect.get())))
		GS_GOTO_CLEAN();

	while (true) {
		if (!!(r = clnt_state_crank_reconnecter(State, StateReconnect.get(), ClntKeyVal, ServAuxData)))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}

void serv_worker_thread_func_f(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend) {
	int r = 0;
	if (!!(r = serv_worker_thread_func(ServKeyVal, ServAuxData, WorkerDataRecv, WorkerDataSend)))
		assert(0);
	for (;;) {}
}

void serv_serv_aux_thread_func_f(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;
	if (!!(r = serv_serv_aux_thread_func(ServKeyVal, ServAuxData)))
		assert(0);
	for (;;) {}
}

void clnt_serv_aux_thread_func_f(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, ENetAddress address /* by val */) {
	int r = 0;
	if (!!(r = aux_serv_aux_thread_func(ServKeyVal, ServAuxData, address)))
		assert(0);
	for (;;) {}
}

void serv_thread_func_f(const confmap_t &ServKeyVal, sp<ServWorkerData> WorkerDataRecv, sp<ServWorkerData> WorkerDataSend) {
	int r = 0;
	if (!!(r = serv_thread_func(ServKeyVal, WorkerDataRecv, WorkerDataSend)))
		assert(0);
	for (;;) {}
}

void clnt_thread_func_f(const confmap_t &ClntKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;
	if (!!(r = clnt_thread_func(ClntKeyVal, ServAuxData)))
		assert(0);
	for (;;) {}
}

int aux_full_create_connection_client(confmap_t ClntKeyVal, sp<FullConnectionClient> *oConnectionClient) {
	int r = 0;

	sp<FullConnectionClient> ConnectionClient;

	ENetHost *host = NULL;
	ENetAddress AddressHost = {};

	if (!!(r = aux_enet_host_client_create_addr(&host, &AddressHost)))
		GS_GOTO_CLEAN();

	{
		sp<ServAuxData> ClientServAuxData(new ServAuxData);

		sp<std::thread> ClientAuxThread(new std::thread(clnt_serv_aux_thread_func_f, ClntKeyVal, ClientServAuxData, AddressHost));
		sp<std::thread> ClientThread(new std::thread(clnt_thread_func_f, ClntKeyVal, ClientServAuxData));

		ConnectionClient = sp<FullConnectionClient>(new FullConnectionClient(ClientAuxThread, ClientThread));
	}

	if (oConnectionClient)
		*oConnectionClient = ConnectionClient;

clean:

	return r;
}

int stuff2() {
	int r = 0;

	confmap_t ServKeyVal;
	confmap_t ClntKeyVal;

	if (!!(r = aux_config_read("../data/", "gittest_config_serv.conf", &ServKeyVal)))
		GS_GOTO_CLEAN();
	ClntKeyVal = ServKeyVal;

	{
		sp<ServWorkerData> WorkerDataSend(new ServWorkerData);
		sp<ServWorkerData> WorkerDataRecv(new ServWorkerData);
		sp<ServAuxData> ServAuxData(new ServAuxData);

		sp<std::thread> ServerWorkerThread(new std::thread(serv_worker_thread_func_f, ServKeyVal, ServAuxData, WorkerDataRecv, WorkerDataSend));
		sp<std::thread> ServerAuxThread(new std::thread(serv_serv_aux_thread_func_f, ServKeyVal, ServAuxData));
		sp<std::thread> ServerThread(new std::thread(serv_thread_func_f, ServKeyVal, WorkerDataRecv, WorkerDataSend));
		sp<std::thread> ClientThread(new std::thread(clnt_thread_func_f, ClntKeyVal, ServAuxData));

		ServerThread->join();
		ClientThread->join();
	}

clean:

	return r;
}

int main(int argc, char **argv) {
	int r = 0;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = stuff2()))
		GS_GOTO_CLEAN();

clean:
	if (!!r) {
		assert(0);
	}

	return EXIT_SUCCESS;
}
