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

#define GS_ERR_CLEAN(THE_R) { r = (THE_R); goto clean; }

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
	int mInterruptRequested;
	sp<std::mutex> mAuxDataMutex;
	sp<std::condition_variable> mAuxDataCond;
};

void ServAuxData::InterruptRequestedEnqueue() {
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		mInterruptRequested = true;
	}
	mAuxDataCond->notify_one();
}

bool ServAuxData::InterruptRequestedDequeueTimeout(const std::chrono::milliseconds &WaitForMillis) {
	bool IsPredicateTrue = false;
	{
		std::unique_lock<std::mutex> lock(*mAuxDataMutex);
		IsPredicateTrue = mAuxDataCond->wait_for(lock, WaitForMillis, [&]() { return !!mInterruptRequested; });
		assert(mInterruptRequested);
	}
	return IsPredicateTrue;
}

int serv_worker_thread_func() {
	int r = 0;

clean:

	return r;
}

int serv_aux_thread_func() {
	int r = 0;

	ENetAddress address;
	ENetHost *client = NULL;
	ENetPeer *peer = NULL;
	ENetEvent event;
	
	/* 127.0.0.1 */
	address.host = ENET_HOST_TO_NET_32(1 | 0 << 8 | 0 << 16 | 0x7F << 24);
	address.port = GS_PORT;

	if (!(client = enet_host_create(NULL, 32, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (!(peer = enet_host_connect(client, &address, 1, 0)))
		GS_ERR_CLEAN(1);

	while (true) {
		int retcode = 0;
		if ((retcode = enet_host_service(client, &event, 1000)) < 0)
			GS_ERR_CLEAN(1);
		assert(retcode >= 0);
		if (retcode == 0)
			continue;

		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT:
			printf("[serv_aux] connected\n");
			event.peer->data = NULL;
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			assert(0);
			enet_packet_destroy(event.packet);
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
			printf("[serv_aux] disconnected\n");
			event.peer->data = NULL;
		}
	}

clean:

	return r;
}

int serv_thread_func() {
	int r = 0;

	ENetAddress address;
	ENetHost *server = NULL;
	ENetEvent event;

	address.host = ENET_HOST_ANY;
	address.port = GS_PORT;

	if (!(server = enet_host_create(&address, 32, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	while (true) {
		int retcode = 0;
		if ((retcode = enet_host_service(server, &event, 1000)) < 0)
			GS_ERR_CLEAN(1);
		assert(retcode >= 0);
		if (retcode == 0)
			continue;

		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT:
			printf("[serv] A new client connected from %x:%u.\n",
				event.peer->address.host,
				event.peer->address.port);
			event.peer->data = "Client information";
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			printf("[serv] A packet of length %lu containing %.*s was received from %s on channel %u.\n",
				(unsigned long)event.packet->dataLength,
				(int)event.packet->dataLength, event.packet->data,
				event.peer->data,
				event.channelID);
			enet_packet_destroy(event.packet);
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
			printf("[serv] %s disconnected.\n", event.peer->data);
			event.peer->data = NULL;
		}
	}

clean:
	if (server)
		enet_host_destroy(server);

	return r;
}

int clnt_thread_func() {
	int r = 0;

	ENetHost *client = NULL;
	ENetAddress address;
	ENetEvent event;
	ENetPeer *peer = NULL;
	ENetPacket *packet = NULL;

	if (!(client = enet_host_create(NULL, 1, 1, 0, 0)))
		GS_ERR_CLEAN(1);

	if (!!(r = enet_address_set_host(&address, "localhost")))
		GS_ERR_CLEAN(1);
	address.port = GS_PORT;

	if (!(peer = enet_host_connect(client, &address, 1, 0)))
		GS_ERR_CLEAN(1);

	int retcode = 0;
	if ((retcode = enet_host_service(client, &event, 5000)) < 0)
		GS_ERR_CLEAN(1);
	assert(retcode >= 0);
	if (retcode == 0)
		GS_ERR_CLEAN(2);
	if (event.type != ENET_EVENT_TYPE_CONNECT)
		GS_ERR_CLEAN(3);

	printf("[clnt] Client connection succeeded.\n");

	if (!(packet = enet_packet_create("packet", strlen("packet") + 1, ENET_PACKET_FLAG_RELIABLE)))
		GS_ERR_CLEAN(1);

	if (!!(r = enet_peer_send(peer, 0, packet)))
		goto clean;
	packet = NULL;  /* lost ownership after enet_peer_send */

	enet_host_flush(client);

clean:
	if (packet)
		enet_packet_destroy(packet);

	if (peer)
		enet_peer_reset(peer);

	if (client)
		enet_host_destroy(client);

	return r;
}

void serv_worker_thread_func_f(confmap_t *ServKeyVal, sp<ServWorkerData> ServWorkerData) {
	int r = 0;
	if (!!(r = serv_worker_thread_func()))
		assert(0);
	for (;;) {}
}

void serv_aux_thread_func_f(confmap_t *ServKeyVal, sp<ServAuxData> ServAuxData) {
	int r = 0;
	if (!!(r = serv_aux_thread_func()))
		assert(0);
	for (;;) {}
}

void serv_thread_func_f(confmap_t *ServKeyVal) {
	int r = 0;
	if (!!(r = serv_thread_func()))
		assert(0);
	for (;;) {}
}

void clnt_thread_func_f(confmap_t *ClntKeyVal) {
	int r = 0;
	if (!!(r = clnt_thread_func()))
		assert(0);
	for (;;) {}
}

int stuff() {
	int r = 0;

	confmap_t ServKeyVal;
	confmap_t ClntKeyVal;

	if (!!(r = aux_config_read("../data/", "gittest_config_serv.conf", &ServKeyVal)))
		goto clean;
	ClntKeyVal = ServKeyVal;

	{
		sp<ServWorkerData> ServWorkerData(new ServWorkerData);
		sp<ServAuxData> ServAuxData(new ServAuxData);

		sp<std::thread> ServerWorkerThread(new std::thread(serv_worker_thread_func_f, &ServKeyVal, ServWorkerData));
		sp<std::thread> ServerAuxThread(new std::thread(serv_aux_thread_func_f, &ServKeyVal, ServAuxData));
		sp<std::thread> ServerThread(new std::thread(serv_thread_func_f, &ServKeyVal));
		sp<std::thread> ClientThread(new std::thread(clnt_thread_func_f, &ClntKeyVal));

		ServerThread->join();
		ClientThread->join();
	}

clean:

	return r;
}

int main(int argc, char **argv) {
	int r = 0;

	if (!!(r = aux_gittest_init()))
		goto clean;

	if (!!(r = enet_initialize()))
		goto clean;

	if (!!(r = stuff()))
		goto clean;

clean:
	if (!!r) {
		assert(0);
	}

	return EXIT_SUCCESS;
}
