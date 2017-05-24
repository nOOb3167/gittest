#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cstdlib>
#include <cassert>
#include <cstdio>

#include <thread>
#include <string>
#include <chrono>

#include <gittest/misc.h>
#include <gittest/frame.h>
#include <gittest/net2.h>
#include <gittest/crank_test.h>

#define GS_TEST_NUM_WORKERS 1
#define GS_TEST_SERV_PORT 3756
#define GS_TEST_HOST_NAME "localhost"

class ClntSpawnBase;

int gs_net2_test_stuff_02();

//int gs_net2_test_stuff_02();
//if (!!(r = gs_net2_test_stuff_02()))
//	GS_GOTO_CLEAN();
//GS_ERR_CLEAN(1);

static int serv_cb_crank_t(struct GsCrankData *CrankData)
{
	int r = 0;

	struct GsStoreWorkerTest *pStoreWorker = (struct GsStoreWorkerTest *) CrankData->mStoreWorker;

	struct GsAffinityToken AffinityToken = {};

	if (pStoreWorker->base.magic != GS_STORE_WORKER_TEST_MAGIC)
		GS_ERR_CLEAN(1);

	while (true) {
		struct GsPacket *Packet = NULL;
		gs_connection_surrogate_id_t IdForSend = 0;

		if (!!(r = gs_worker_packet_dequeue_timeout_reconnects2(
			CrankData,
			GS_SERV_AUX_VERYHIGH_TIMEOUT_U32_MS,
			&AffinityToken,
			&Packet,
			&IdForSend)))
		{
			GS_GOTO_CLEAN();
		}

		printf("packet\n");
	}

clean:
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	return r;
}

static void clnt_thread_func()
{
	int r = 0;

	struct GsHostSurrogate Host = {};
	struct GsAddressSurrogate Addr = {};
	struct GsPeerSurrogate Peer = {};
	struct GsPacketSurrogate Packet = {};
	struct GsEventSurrogate Event = {};

	std::string Buffer;

	GS_BYPART_DATA_VAR(String, BysizeBuffer);
	GS_BYPART_DATA_INIT(String, BysizeBuffer, &Buffer);

	if (!!(r = gs_host_surrogate_setup_host_nobind(128, &Host)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_address_surrogate_setup_addr_name_port(GS_TEST_SERV_PORT, GS_TEST_HOST_NAME, (sizeof GS_TEST_HOST_NAME) - 1, &Addr)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_host_surrogate_connect(&Host, &Addr, &Peer)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_host_surrogate_connect_wait_blocking(&Host, &Peer)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_serv_aux_interrupt_requested(gs_bysize_cb_String, &BysizeBuffer)))
		GS_GOTO_CLEAN();

	Packet.mPacket = enet_packet_create(Buffer.data(), Buffer.size(), ENET_PACKET_FLAG_RELIABLE);
	if (!!(r = enet_peer_send(Peer.mPeer, 0, GS_ARGOWN(&Packet.mPacket))))
		GS_GOTO_CLEAN();

	while (0 <= enet_host_service(Host.mHost, &Event.event, GS_SERV_AUX_VERYHIGH_TIMEOUT_U32_MS))
		{}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

clean:
	if (!!r)
		GS_ASSERT(0);
}

struct ClntSpawnRunnerData
{
	uint32_t ServPort;
	const char *ServHostName;
	uint32_t NumThreads;
	uint32_t NumConnectionPerThread;

	uint32_t ThreadId;
};

class ClntSpawnBase
{
public:
	ClntSpawnBase() {}
	virtual ~ClntSpawnBase() = 0;
	virtual void Runner(ClntSpawnRunnerData RunnerData) = 0;
};

static int runner_connection_make_n(
	ClntSpawnRunnerData *RunnerData,
	std::vector<struct GsHostSurrogate> *ioHost,
	std::vector<struct GsPeerSurrogate> *ioPeer)
{
	int r = 0;

	for (uint32_t i = 0; i < RunnerData->NumConnectionPerThread; i++) {
		struct GsHostSurrogate Host = {};
		struct GsAddressSurrogate Addr = {};
		struct GsPeerSurrogate Peer = {};

		if (!!(r = gs_host_surrogate_setup_host_nobind(128, &Host)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_address_surrogate_setup_addr_name_port(RunnerData->ServPort, RunnerData->ServHostName, strlen(RunnerData->ServHostName), &Addr)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_host_surrogate_connect(&Host, &Addr, &Peer)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_host_surrogate_connect_wait_blocking(&Host, &Peer)))
			GS_GOTO_CLEAN();

		ioHost->push_back(Host);
		ioPeer->push_back(Peer);
	}

clean:

	return r;
}

static int runner_host_service_n(std::vector<struct GsHostSurrogate> *HostVec)
{
	int r = 0;

	int err;

	for (size_t i = 0; i < HostVec->size(); i++) {
		struct GsHostSurrogate Host = (*HostVec)[i];

		for(bool serviced = false;;) {
			struct GsEventSurrogate Event = {};
			if(!serviced) { if(0 >= (err = enet_host_service(Host.mHost, &Event.event, 0))) break; serviced = true; }
			else if(0 >= (err = enet_host_check_events(Host.mHost, &Event.event))) break;
			switch(Event.event.type) {
			case ENET_EVENT_TYPE_RECEIVE:
				enet_packet_destroy(Event.event.packet);
			}
		}
		if (err < 0)
			GS_ERR_CLEAN(1);
	}

clean:

	return r;
}

static int runner_frame_make_skeleton(
	uint32_t ExtraLen,
	std::string *oBuffer,
	uint32_t *oExtraOffset)
{
	int r = 0;

	GsFrameType FrameType = aux_frametype_make("TESTFRAME", 0xFFFFFFFF);

	uint32_t Offset = 0;
	uint32_t PayloadSize = ExtraLen;
	uint32_t BufferSize = GS_FRAME_HEADER_LEN + GS_FRAME_SIZE_LEN + PayloadSize;
	std::string Buffer(BufferSize, '\0');
	uint8_t *BufferData = (uint8_t *)Buffer.data();

	if (!!(r = aux_frame_write_frametype(BufferData, BufferSize, Offset, &Offset, &FrameType)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_write_size(BufferData, BufferSize, Offset, &Offset, GS_FRAME_SIZE_LEN, PayloadSize)))
		GS_GOTO_CLEAN();

	if (oBuffer)
		oBuffer->swap(Buffer);

	if (oExtraOffset)
		*oExtraOffset = Offset;

clean:

	return r;
}

class ClntSpawn00 : public ClntSpawnBase
{
public:
	ClntSpawn00()
	{
		if (!!runner_frame_make_skeleton(sizeof(uint32_t), &BufferMsg, &BufferMsgDataOffset))
			GS_ASSERT(0);
	};
	~ClntSpawn00() {};
	static ClntSpawnBase *Factory(size_t idx) { return new ClntSpawn00(); }
	void Runner(ClntSpawnRunnerData RunnerData)
	{
		int r = 0;

		if (!!(r = runner_connection_make_n(&RunnerData, &Host, &Peer)))
			GS_GOTO_CLEAN();

		if (!!(r = runner_host_service_n(&Host)))
			GS_GOTO_CLEAN();

	clean:
		if (!!r)
			GS_ASSERT(0);
	}

private:
	std::vector<struct GsHostSurrogate> Host;
	std::vector<struct GsPeerSurrogate> Peer;
	std::string BufferMsg;
	uint32_t BufferMsgDataOffset;
};

static int clnt_spawn_n_m(
	uint32_t ServPort,
	const char *ServHostName,
	uint32_t NumThreads,
	uint32_t NumConnectionPerThread,
	std::function<ClntSpawnBase *(size_t idx)> ClntSpawnFactory)
{
	int r = 0;

	std::vector<ClntSpawnBase *> ThreadData;
	std::vector<std::thread> Threads;

	for (size_t i = 0; i < NumThreads; i++)
		ThreadData.push_back(ClntSpawnFactory(i));

	for (size_t i = 0; i < NumThreads; i++) {
		ClntSpawnRunnerData RunnerData;
		RunnerData.ServPort = ServPort;
		RunnerData.ServHostName = ServHostName;
		RunnerData.NumThreads = NumThreads;
		RunnerData.NumConnectionPerThread = NumConnectionPerThread;
		RunnerData.ThreadId = i;
		Threads.push_back(std::thread(&ClntSpawnBase::Runner, ThreadData[i], RunnerData));
	}

clean:

	return r;
}

int gs_net2_test_stuff_02()
{
	int r = 0;

	struct GsFullConnection *ConnectionServ = NULL;

	// FIXME: race condition wrt clients connecting before server start

	if (!!(r = gs_net_full_create_connection_test(
		GS_TEST_NUM_WORKERS,
		GS_TEST_SERV_PORT,
		"test02",
		serv_cb_crank_t,
		NULL,
		&ConnectionServ)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_spawn_n_m(
		GS_TEST_SERV_PORT,
		GS_TEST_HOST_NAME,
		GS_TEST_NUM_WORKERS,
		1,
		ClntSpawn00::Factory)))
	{
		GS_GOTO_CLEAN();
	}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

clean:

	return r;
}
