#include <cstdlib>
#include <cassert>
#include <cstdio>

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

#define GS_FRAME_HEADER_LEN 40
#define GS_FRAME_SIZE_LEN 4

#define GS_DBG_CLEAN {}
//#define GS_DBG_CLEAN { assert(0); }

#define GS_ERR_CLEAN(THE_R) { r = (THE_R); GS_DBG_CLEAN; goto clean; }
#define GS_GOTO_CLEAN() { GS_DBG_CLEAN; goto clean; }
#define GS_GOTO_CLEANSUB() { GS_DBG_CLEAN; goto cleansub; }

template<typename T>
using sp = ::std::shared_ptr<T>;

class ServWorkerRequestData {};

class ServWorkerData {
public:
	ServWorkerData()
		: mWorkerQueue(new std::deque<sp<ServWorkerRequestData> >),
		mWorkerDataMutex(new std::mutex),
		mWorkerDataCond(new std::condition_variable)
	{}

	void RequestEnqueue(sp<ServWorkerRequestData> RequestData);
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

bool aux_frame_enough_space(uint32_t TotalLength, uint32_t Offset, uint32_t WantedSpace) {
	return (TotalLength >= Offset) && (TotalLength - Offset) >= WantedSpace;
}

int aux_frame_size(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew, uint32_t MSize) {
	assert(sizeof(uint8_t) == 1 && sizeof(uint32_t) == 4 && sizeof(uint32_t) == GS_FRAME_SIZE_LEN);
	assert(aux_frame_enough_space(DataLength, Offset, sizeof(uint32_t)));
	aux_uint32_to_LE(MSize, (char *)DataStart+Offset, sizeof(uint32_t));
	if (OffsetNew)
		*OffsetNew = Offset + sizeof(uint32_t);
	return 0;
}

int aux_frame_type_interrupt_requested(uint8_t *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew) {
	static const char ServAuxStaticMagicPackageBuffer[] = "SERV_AUX_INTERRUPT_REQUESTED";
	assert(sizeof ServAuxStaticMagicPackageBuffer - 1 <= GS_FRAME_HEADER_LEN);
	assert(aux_frame_enough_space(DataLength, Offset, GS_FRAME_HEADER_LEN));
	memset(DataStart+Offset, 0, GS_FRAME_HEADER_LEN);
	memcpy(DataStart+Offset, ServAuxStaticMagicPackageBuffer, sizeof ServAuxStaticMagicPackageBuffer - 1);
	if (OffsetNew)
		*OffsetNew = Offset + GS_FRAME_HEADER_LEN;
	return 0;
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

void ServWorkerData::RequestEnqueue(sp<ServWorkerRequestData> RequestData) {
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

int serv_worker_thread_func(const confmap_t &ServKeyVal, sp<ServWorkerData> ServWorkerData) {
	int r = 0;

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

int serv_host_service(ENetHost *server) {
	int r = 0;

	std::vector<ENetEvent> Events;

	if (!!(r = aux_host_service(server, GS_SERV_AUX_ARBITRARY_TIMEOUT_MS, &Events)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < Events.size(); i++) {
		switch (Events[i].type)
		{
		case ENET_EVENT_TYPE_CONNECT:
			printf("[serv] A new client connected from %x:%u.\n",
				Events[i].peer->address.host,
				Events[i].peer->address.port);
			Events[i].peer->data = "Client information";
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			printf("[serv] A packet of length %lu containing %.*s was received from %s on channel %u.\n",
				(unsigned long)Events[i].packet->dataLength,
				(int)Events[i].packet->dataLength, Events[i].packet->data,
				Events[i].peer->data,
				Events[i].channelID);
			enet_packet_destroy(Events[i].packet);
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
			printf("[serv] %s disconnected.\n", Events[i].peer->data);
			Events[i].peer->data = NULL;
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

	uint8_t FrameInterruptRequested[GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN] = {};

	uint32_t ServPort = 0;
	uint32_t ServHostIp = ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24);
	ENetAddress address = {};

	ENetHost *client = NULL;
	ENetPeer *peer = NULL;

	uint32_t Offset = 0;
	if (!!(r = aux_frame_type_interrupt_requested(FrameInterruptRequested, sizeof FrameInterruptRequested, Offset, &Offset)))
		GS_GOTO_CLEAN();
	if (!!(r = aux_frame_size(FrameInterruptRequested, sizeof FrameInterruptRequested, Offset, &Offset, 0)))
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
				&FrameInterruptRequested, sizeof FrameInterruptRequested, ENET_PACKET_FLAG_NO_ALLOCATE);

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
		if (!!(r = serv_host_service(server)))
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

	const char *ServHostName = aux_config_key(ClntKeyVal, "ConfServHostName");
	uint32_t ServPort = 0;

	if (!ServHostName)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_config_key_uint32(ClntKeyVal, "ConfServPort", &ServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_enet_address_create_hostname(ServPort, ServHostName, &address)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_host_connect(&address, GS_CONNECT_NUMRETRY, GS_CONNECT_TIMEOUT_MS, &client, &peer)))
		GS_ERR_CLEAN(1);

	printf("[clnt] Client connection succeeded.\n");

	if (!(packet = enet_packet_create("packet", strlen("packet") + 1, ENET_PACKET_FLAG_RELIABLE)))
		GS_ERR_CLEAN(1);

	if (!!(r = enet_peer_send(peer, 0, packet)))
		GS_GOTO_CLEAN();
	packet = NULL;  /* lost ownership after enet_peer_send */

	enet_host_flush(client);

	ServAuxData->InterruptRequestedEnqueue();

clean:
	if (packet)
		enet_packet_destroy(packet);

	if (peer)
		enet_peer_reset(peer);

	if (client)
		enet_host_destroy(client);

	return r;
}

void serv_worker_thread_func_f(const confmap_t &ServKeyVal, sp<ServWorkerData> ServWorkerData) {
	int r = 0;
	if (!!(r = serv_worker_thread_func(ServKeyVal, ServWorkerData)))
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

		sp<std::thread> ServerWorkerThread(new std::thread(serv_worker_thread_func_f, ServKeyVal, ServWorkerData));
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
