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

int gs_net2_test_stuff_02();

//int gs_net2_test_stuff_02();
//if (!!(r = gs_net2_test_stuff_02()))
//	GS_GOTO_CLEAN();
//GS_ERR_CLEAN(1);

int serv_cb_crank_t(struct GsCrankData *CrankData)
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

int gs_net2_test_stuff_02()
{
	int r = 0;

	std::thread ThreadClnt(clnt_thread_func);

	struct GsFullConnection *ConnectionServ = NULL;

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

	ThreadClnt.join();

clean:

	return r;
}
