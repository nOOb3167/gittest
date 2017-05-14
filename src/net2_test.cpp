#include <gittest/misc.h>
#include <gittest/net2.h>

#define GS_NET2_TEST_NUM_WORKER_THREADS 3

int gs_net2_test_stuff()
{
	int r = 0;

	struct GsAffinityQueue *AffinityQueue = NULL;

	struct GsWorkerDataVec *WorkerDataVecRecv = NULL;
	struct GsWorkerData *WorkerDataSend = NULL;

	if (!!(r = gs_affinity_queue_create(GS_NET2_TEST_NUM_WORKER_THREADS, &AffinityQueue)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_data_vec_create(GS_NET2_TEST_NUM_WORKER_THREADS, &WorkerDataVecRecv)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_data_create(&WorkerDataSend)))
		GS_GOTO_CLEAN();
	
	for (int i = 0; i < GS_NET2_TEST_NUM_WORKER_THREADS; i++) {
		struct GsWorkerRequestData Request0 = {};
		if (!!(r = gs_worker_request_data_type_packet_make(NULL, i, &Request0)))
			GS_GOTO_CLEAN();
		if (!!(r = gs_affinity_queue_worker_acquire_ready_and_enqueue(AffinityQueue, WorkerDataVecRecv, &Request0, i)))
			GS_GOTO_CLEAN();
	}

	for (int i = 0; i < GS_NET2_TEST_NUM_WORKER_THREADS; i++) {
		struct GsWorkerRequestData Request0 = {};
		if (!!(r = gs_worker_request_data_type_packet_make(NULL, i, &Request0)))
			GS_GOTO_CLEAN();
		if (!!(r = gs_affinity_queue_worker_acquire_ready_and_enqueue(AffinityQueue, WorkerDataVecRecv, &Request0, i)))
			GS_GOTO_CLEAN();
	}

	{
		struct GsWorkerRequestData Request0 = {};
		if (!!(r = gs_worker_request_data_type_packet_make(NULL, GS_NET2_TEST_NUM_WORKER_THREADS + 0, &Request0)))
			GS_GOTO_CLEAN();
		if (!!(r = gs_affinity_queue_worker_acquire_ready_and_enqueue(AffinityQueue, WorkerDataVecRecv, &Request0, GS_NET2_TEST_NUM_WORKER_THREADS + 0)))
			GS_GOTO_CLEAN();
	}

	{
		// dequeue from WorkerId=1
		struct GsAffinityToken AffinityToken = {};
		if (!!(r = gs_worker_packet_dequeue_timeout_reconnects(WorkerDataVecRecv, WorkerDataSend, 1, GS_SERV_AUX_ARBITRARY_TIMEOUT_MS, AffinityQueue, &AffinityToken, NULL, NULL, NULL)))
			GS_GOTO_CLEAN();
		GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);
		if (!!(r = gs_worker_packet_dequeue_timeout_reconnects(WorkerDataVecRecv, WorkerDataSend, 1, GS_SERV_AUX_ARBITRARY_TIMEOUT_MS, AffinityQueue, &AffinityToken, NULL, NULL, NULL)))
			GS_GOTO_CLEAN();
		GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);
	}

clean:

	return r;
}
