#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif _MSC_VER

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <deque>

#include <enet/enet.h>
#include <git2.h>

#include <gittest.h>

#define GS_PORT 3756

#define GS_SERV_AUX_ARBITRARY_TIMEOUT_MS 5000
#define GS_CONNECT_NUMRETRY   5
#define GS_CONNECT_TIMEOUT_MS 1000
#define GS_RECEIVE_TIMEOUT_MS 5000

#define GS_FRAME_HEADER_STR_LEN 40
#define GS_FRAME_HEADER_NUM_LEN 4
#define GS_FRAME_HEADER_LEN (GS_FRAME_HEADER_STR_LEN + GS_FRAME_HEADER_NUM_LEN)
#define GS_FRAME_SIZE_LEN 4

#define GS_PAYLOAD_OID_SIZE 20

#define GS_DBG_CLEAN {}
//#define GS_DBG_CLEAN { assert(0); }
//#define GS_DBG_CLEAN { DebugBreak(); }

#define GS_ERR_CLEAN(THE_R) { r = (THE_R); GS_DBG_CLEAN; goto clean; }
#define GS_GOTO_CLEAN() { GS_DBG_CLEAN; goto clean; }
#define GS_GOTO_CLEANSUB() { GS_DBG_CLEAN; goto cleansub; }

#define GS_FRAME_TYPE_SERV_AUX_INTERRUPT_REQUESTED 0
#define GS_FRAME_TYPE_REQUEST_LATEST_COMMIT_TREE 1
#define GS_FRAME_TYPE_RESPONSE_LATEST_COMMIT_TREE 2

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
};

template<typename T>
using sp = ::std::shared_ptr<T>;

typedef ::std::shared_ptr<ENetPacket> gs_packet_t;

class ServWorkerRequestData {
public:
	ENetHost *mHost;
	ENetPeer *mPeer;
	gs_packet_t mPacket;
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


int aux_packet_full_send(ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags);


gs_packet_t aux_gs_make_packet(ENetPacket *packet) {
	return gs_packet_t(packet, [](ENetPacket *xpacket) { enet_packet_destroy(xpacket); });
}

int aux_make_serv_worker_request_data(ENetHost *host, ENetPeer *peer, const gs_packet_t &Packet, sp<ServWorkerRequestData> *oServWorkerRequestData) {
	int r = 0;

	sp<ServWorkerRequestData> ServWorkerRequestData(new ServWorkerRequestData);
	ServWorkerRequestData->mHost = host;
	ServWorkerRequestData->mPeer = peer;
	ServWorkerRequestData->mPacket = Packet;

	if (oServWorkerRequestData)
		*oServWorkerRequestData = ServWorkerRequestData;

clean:

	return r;
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
	if (OffsetNew)
		*OffsetNew = Offset + SizeOfSize;
	if (oSize)
		*oSize = Size;
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
	if (OffsetNew)
		*OffsetNew = Offset;
	if (oFrameType)
		*oFrameType = FrameType;
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
	int r = 0;
	static GsFrameType FrameType = GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE);
	std::string Buffer;
	Buffer.resize(GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + GS_PAYLOAD_OID_SIZE);
	uint32_t Offset = 0;
	if (!!(r = aux_frame_write_frametype((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, &FrameType)))
		GS_GOTO_CLEAN();
	assert(OidSize == GS_PAYLOAD_OID_SIZE);
	if (!!(r = aux_frame_write_size((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, GS_FRAME_SIZE_LEN, GS_PAYLOAD_OID_SIZE)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_frame_write_buf((uint8_t *)Buffer.data(), Buffer.size(), Offset, &Offset, Oid, OidSize)))
		GS_GOTO_CLEAN();
	if (oBuffer)
		oBuffer->swap(Buffer);
clean:
	return r;
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

int serv_worker_thread_func(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, sp<ServWorkerData> ServWorkerData) {
	int r = 0;

	while (true) {
		sp<ServWorkerRequestData> Request;

		ServWorkerData->RequestDequeue(&Request);

		const gs_packet_t &Packet = Request->mPacket;

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

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			uint8_t Oid[GS_PAYLOAD_OID_SIZE] = {};
			memset(Oid, 0x10, sizeof Oid);

			if (!!(r = aux_frame_full_write_response_latest_commit_tree(&ResponseBuffer, Oid, sizeof Oid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_packet_full_send(Request->mHost, Request->mPeer, ServAuxData.get(),
				ResponseBuffer.data(), ResponseBuffer.size(), ENET_PACKET_FLAG_RELIABLE)))
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

int serv_host_service(ENetHost *server, const sp<ServWorkerData> &ServWorkerData) {
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

				gs_packet_t Packet = aux_gs_make_packet(Events[i].packet);

				sp<ServWorkerRequestData> ServWorkerRequestData;

				if (!!(r = aux_make_serv_worker_request_data(server, peer, Packet, &ServWorkerRequestData)))
					GS_GOTO_CLEAN();

				ServWorkerData->RequestEnqueue(ServWorkerRequestData);
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

int serv_aux_thread_func(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;

	std::string BufferFrameInterruptRequested;

	uint32_t ServPort = 0;
	uint32_t ServHostIp = ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24);
	ENetAddress address = {};

	ENetHost *client = NULL;
	ENetPeer *peer = NULL;

	uint32_t Offset = 0;
	if (!!(r = aux_frame_full_write_serv_aux_interrupt_requested(&BufferFrameInterruptRequested)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_uint32(ServKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	assert(ServHostIp == ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24));

	if (!!(r = aux_enet_address_create_ip(ServPort, ServHostIp, &address)))
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

int serv_thread_func(const confmap_t &ServKeyVal, sp<ServWorkerData> ServWorkerData) {
	int r = 0;

	ENetHost *server = NULL;

	uint32_t ServPort = 0;

	if (!!(r = aux_config_key_uint32(ServKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_host_create_serv(ServPort, &server)))
		GS_GOTO_CLEAN();

	while (true) {
		if (!!(r = serv_host_service(server, ServWorkerData)))
			GS_GOTO_CLEAN();
	}

clean:
	if (server)
		enet_host_destroy(server);

	return r;
}

int clnt_thread_func(const confmap_t &ClntKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;

	ENetAddress address = {};
	ENetHost *client = NULL;
	ENetPeer *peer = NULL;
	ENetPacket *packet = NULL;

	std::string Buffer;
	gs_packet_t Packet;

	const char *ServHostName = aux_config_key(ClntKeyVal, "ConfServHostName");
	uint32_t ServPort = 0;

	if (!ServHostName)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_config_key_uint32(ClntKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_address_create_hostname(ServPort, ServHostName, &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &client, &peer)))
		GS_GOTO_CLEAN();

	printf("[clnt] Client connection succeeded.\n");

	uint32_t Offset = 0;

	if (!!(r = aux_frame_full_write_request_latest_commit_tree(&Buffer)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_packet_full_send(client, peer, ServAuxData.get(), Buffer.data(), Buffer.size(), 0)))
		GS_GOTO_CLEAN();

	Offset = 0;
	if (!!(r = aux_host_service_one_type_receive(client, GS_RECEIVE_TIMEOUT_MS, &Packet)))
		GS_GOTO_CLEAN();

	// FIXME: handle timeout (Packet will be null)

	if (!!(r = aux_frame_ensure_frametype(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_COMMIT_TREE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_SIZE)))
		GS_GOTO_CLEAN();

clean:
	if (packet)
		enet_packet_destroy(packet);

	if (peer)
		enet_peer_reset(peer);

	if (client)
		enet_host_destroy(client);

	return r;
}

void serv_worker_thread_func_f(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData, sp<ServWorkerData> ServWorkerData) {
	int r = 0;
	if (!!(r = serv_worker_thread_func(ServKeyVal, ServAuxData, ServWorkerData)))
		assert(0);
	for (;;) {}
}

void serv_aux_thread_func_f(const confmap_t &ServKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;
	if (!!(r = serv_aux_thread_func(ServKeyVal, ServAuxData)))
		assert(0);
	for (;;) {}
}

void serv_thread_func_f(const confmap_t &ServKeyVal, sp<ServWorkerData> ServWorkerData) {
	int r = 0;
	if (!!(r = serv_thread_func(ServKeyVal, ServWorkerData)))
		assert(0);
	for (;;) {}
}

void clnt_thread_func_f(const confmap_t &ClntKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;
	if (!!(r = clnt_thread_func(ClntKeyVal, ServAuxData)))
		assert(0);
	for (;;) {}
}

int stuff2() {
	int r = 0;

	confmap_t ServKeyVal;
	confmap_t ClntKeyVal;

	if (!!(r = aux_config_read("../data/", "gittest_config_serv.conf", &ServKeyVal)))
		GS_GOTO_CLEAN();
	ClntKeyVal = ServKeyVal;

	{
		sp<ServWorkerData> ServWorkerData(new ServWorkerData);
		sp<ServAuxData> ServAuxData(new ServAuxData);

		sp<std::thread> ServerWorkerThread(new std::thread(serv_worker_thread_func_f, ServKeyVal, ServAuxData, ServWorkerData));
		sp<std::thread> ServerAuxThread(new std::thread(serv_aux_thread_func_f, ServKeyVal, ServAuxData));
		sp<std::thread> ServerThread(new std::thread(serv_thread_func_f, ServKeyVal, ServWorkerData));
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
