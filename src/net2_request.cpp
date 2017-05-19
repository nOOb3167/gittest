#include <cstdint>

#include <mutex>
#include <condition_variable>
#include <vector>
#include <deque>

#include <gittest/misc.h>

#include <gittest/net2.h>
#include <gittest/net2_fwd.h>
#include <gittest/net2_request.h>

int gs_worker_request_data_type_packet_make(
	struct GsPacket *Packet,
	gs_connection_surrogate_id_t Id,
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET;
	W.mPacket = Packet;
	W.mId = Id;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

int gs_worker_request_data_type_reconnect_prepare_make(
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

int gs_worker_request_data_type_reconnect_reconnect_make(
	struct GsExtraWorker *ExtraWorker,
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT;
	W.mExtraWorker = ExtraWorker;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

int gs_worker_request_data_type_exit_make(
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

int gs_worker_request_data_type_disconnect_make(
	gs_connection_surrogate_id_t Id,
	struct GsWorkerRequestData *outValWorkerRequest)
{
	struct GsWorkerRequestData W = {};

	W.type = GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT;
	W.mId = Id;

	if (outValWorkerRequest)
		*outValWorkerRequest = W;

	return 0;
}

bool gs_worker_request_isempty(struct GsWorkerData *pThis)
{
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		return gs_worker_request_isempty_nolock(pThis, &lock);
	}
}

bool gs_worker_request_isempty_nolock(
	struct GsWorkerData *pThis,
	std::unique_lock<std::mutex> *Lock)
{
	{
		GS_ASSERT(Lock->owns_lock());
		return pThis->mWorkerQueue->empty();
	}
}

int gs_worker_request_enqueue(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *valRequestData)
{
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		pThis->mWorkerQueue->push_back(*valRequestData);
	}
	pThis->mWorkerDataCond->notify_one();
	return 0;
}

int gs_worker_request_enqueue_double_notify(
	struct GsWorkerData *pThis,
	struct GsExtraWorker *ExtraWorker)
{
	int r = 0;

	struct GsWorkerRequestData Prepare = {};
	struct GsWorkerRequestData Reconnect = {};

	if (!!(r = gs_worker_request_data_type_reconnect_prepare_make(&Prepare)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_request_data_type_reconnect_reconnect_make(ExtraWorker, &Reconnect)))
		GS_GOTO_CLEAN();

	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);

		pThis->mWorkerQueue->push_back(Prepare);
		pThis->mWorkerQueue->push_back(Reconnect);
	}
	pThis->mWorkerDataCond->notify_one();

clean:

	return 0;
}

/** @retval GS_ERRCODE_TIMEOUT on timeout */
int gs_worker_request_dequeue_timeout(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs)
{
	std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
	return gs_worker_request_dequeue_timeout_nolock(
		pThis,
		oValRequestData,
		TimeoutMs,
		&lock);
}

int gs_worker_request_dequeue_timeout_nolock(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs,
	std::unique_lock<std::mutex> *Lock)
{
	std::chrono::milliseconds To = std::chrono::milliseconds(TimeoutMs);
	{
		GS_ASSERT(Lock->owns_lock());
		if (! pThis->mWorkerDataCond->wait_for(*Lock, To, [&]() { return !pThis->mWorkerQueue->empty(); }))
			return GS_ERRCODE_TIMEOUT;
		*oValRequestData = pThis->mWorkerQueue->front();
		pThis->mWorkerQueue->pop_front();
	}
	return 0;
}

/** Skips through if RECONNECT_PREPARE is dequeued.
*/
int gs_worker_request_dequeue_timeout_noprepare(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs)
{
	std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
	return gs_worker_request_dequeue_timeout_noprepare_nolock(
		pThis,
		oValRequestData,
		TimeoutMs,
		&lock);
}

int gs_worker_request_dequeue_timeout_noprepare_nolock(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs,
	std::unique_lock<std::mutex> *Lock)
{
	int r = 0;

	GS_ASSERT(Lock->owns_lock());

	if (!!(r = gs_worker_request_dequeue_timeout_nolock(
		pThis,
		oValRequestData,
		TimeoutMs,
		Lock)))
	{
		GS_GOTO_CLEAN();
	}

	if (oValRequestData->type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE) {
		if (!!(r = gs_worker_request_dequeue_timeout_nolock(
			pThis,
			oValRequestData,
			TimeoutMs,
			Lock)))
		{
			GS_GOTO_CLEAN();
		}
		GS_ASSERT(oValRequestData->type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT);
	}

clean:

	return r;
}

int gs_worker_request_dequeue(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData)
{
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		pThis->mWorkerDataCond->wait(lock, [&]() { return !pThis->mWorkerQueue->empty(); });
		*oValRequestData = pThis->mWorkerQueue->front();
		pThis->mWorkerQueue->pop_front();
	}
	return 0;
}

int gs_worker_request_dequeue_all_opt_cpp(
	struct GsWorkerData *pThis,
	std::deque<struct GsWorkerRequestData> *oValRequestData)
{
	oValRequestData->clear();
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		oValRequestData->swap(*pThis->mWorkerQueue);
	}
	return 0;
}

/** Discard all requests until a reconnect request is reached.
    Once a reconnect request is reached, leave it in the queue and succeed.
*/
int gs_worker_request_dequeue_discard_until_reconnect(
	struct GsWorkerData *pThis)
{
	while (true) {
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
		pThis->mWorkerDataCond->wait(lock, [&]() { return !pThis->mWorkerQueue->empty(); });
		while (! pThis->mWorkerQueue->empty()) {
			struct GsWorkerRequestData &Request = pThis->mWorkerQueue->front();
			if (! (Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE ||
				   Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT))
			{
				pThis->mWorkerQueue->pop_front();
			}
			else
				return 0;
		}
	}
}

/** FIXME: likely error prone code. depends a lot on GsWorkerRequestData request semantics
*/
int gs_worker_request_dequeue_steal_except_nolock(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t DstWorkerId,
	gs_worker_id_t SrcWorkerId,
	gs_connection_surrogate_id_t ExceptId,
	std::unique_lock<std::mutex> *LockQueue,
	std::unique_lock<std::mutex> LockWorker[2])
{
	int r = 0;

	struct GsWorkerData *DstWorker = gs_worker_data_vec_id(WorkerDataVec, DstWorkerId);
	struct GsWorkerData *SrcWorker = gs_worker_data_vec_id(WorkerDataVec, SrcWorkerId);

	{
		GS_ASSERT(LockWorker[0].owns_lock() && LockWorker[1].owns_lock());

		/* there can be no PACKET request connid sequences split across a RECONNECT type request, right?
		   because having received a RECONNECT type request means we are sequenced after
		   the reconnect (ie are in a new reconnect cycle), and connections established (and any packages received from them)
		   after a reconnect have IDs distinct from any connections of the previous reconnect cycle. */

		std::deque<GsWorkerRequestData>::iterator ItStealBound = SrcWorker->mWorkerQueue->begin();
		
		/* find last EXIT or RECONNECT request (bound) - bound at begin() if none found */
		// FIXME: search in reverse order for efficiency

		for (std::deque<GsWorkerRequestData>::iterator it = ItStealBound;
			it != SrcWorker->mWorkerQueue->end();
			it++)
		{
			if (it->type == GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT ||
				it->type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE ||
				it->type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT)
			{
				ItStealBound = it;
			}
		}

		/* choose a PACKET request ID, present in src queue, different from 'ExceptId', from
		   requests after StealBound. move all those requests to destination.
		   this is done in three steps
		     - iterate src from bound to end, pushing steals to dst queue, nonsteals to tmp queue
		     - force-resize src queue to just before bound
		     - push whole tmp queue to src
			 recall prio needs to be adjusted if queues were manipulated
		*/

		std::deque<GsWorkerRequestData>::iterator StealIt = ItStealBound;
		gs_connection_surrogate_id_t StealId;
		bool FoundStealableRequest = false;

		std::vector<GsWorkerRequestData> TmpVec;

		/* == choose the PACKET request ID (also improve the steal bound from last EXIT or RECONNECT to first stolen request) == */

		for (/* empty */;
			StealIt != SrcWorker->mWorkerQueue->end();
			StealIt++)
		{
			if (StealIt->type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET && StealIt->mId != ExceptId) {
				StealId = StealIt->mId;
				ItStealBound = StealIt;
				FoundStealableRequest = true;
				break;
			}
		}

		/* == can confirm early whether anything will be actually stolen == */

		if (FoundStealableRequest) {

			/* == iterate src from bound to end, pushing steals to dst queue, nonsteals to tmp queue == */

			for (/* empty */;
				StealIt != SrcWorker->mWorkerQueue->end();
				StealIt++)
			{
				if (StealIt->type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET && StealIt->mId == StealId)
					DstWorker->mWorkerQueue->push_back(*StealIt);
				else
					TmpVec.push_back(*StealIt);
			}

			/* == force-resize src queue to just before bound == */

			SrcWorker->mWorkerQueue->resize(GS_MAX(0, std::distance(SrcWorker->mWorkerQueue->begin(), ItStealBound) - 1));

			/* == push whole tmp queue to src == */

			for (size_t i = 0; i < TmpVec.size(); i++)
				SrcWorker->mWorkerQueue->push_back(TmpVec[i]);

			/* == fixup prio == */

			{
				GS_ASSERT(LockQueue->owns_lock());

				if (!!(r = gs_affinity_queue_prio_increment_nolock(
					AffinityQueue,
					DstWorkerId,
					&LockWorker[0])))
				{
					GS_GOTO_CLEAN();
				}

				if (!!(r = gs_affinity_queue_prio_decrement_nolock(
					AffinityQueue,
					SrcWorkerId,
					&LockWorker[1])))
				{
					GS_GOTO_CLEAN();
				}
			}
		}
	}

clean:

	return r;
}

int gs_worker_packet_enqueue(
	struct GsWorkerData *pThis,
	struct GsIntrTokenSurrogate *IntrToken,
	gs_connection_surrogate_id_t Id,
	const char *Data, uint32_t DataSize)
{
	int r = 0;

	GsPacketSurrogate PacketSurrogate = {};
	GsPacket *Packet = NULL;
	GsWorkerRequestData Request = {};

	if (!(PacketSurrogate.mPacket = enet_packet_create(Data, DataSize, ENET_PACKET_FLAG_RELIABLE)))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_packet_create(&Packet, &PacketSurrogate)))
		GS_GOTO_CLEAN();

	/* lost ownership */
	if (!!(r = gs_packet_surrogate_release_ownership(&PacketSurrogate)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_request_data_type_packet_make(Packet, Id, &Request)))
		GS_GOTO_CLEAN();

	/* NOTE: lost ownership of Request (but can't just modify Request.mPacket since pointer field) */
	if (!!(r = gs_worker_request_enqueue(pThis, &Request)))
		GS_GOTO_CLEAN();

	IntrToken->mIntrToken->cb_token_interrupt(IntrToken->mIntrToken);

clean:
	// FIXME: resource management

	return r;
}

/** FIXME: OBSOLETE / DEPRECATED: use gs_worker_packet_dequeue_timeout_reconnects

    Principal blocking read facility for blocking-type worker.

	@retval: GS_ERRCODE_RECONNECT if reconnect request dequeued
*/
int gs_worker_packet_dequeue_(
	struct GsWorkerData *pThis,
	struct GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId)
{
	int r = 0;

	GsWorkerRequestData Request = {};

	if (!!(r = gs_worker_request_dequeue(pThis, &Request)))
		GS_GOTO_CLEAN();

	/* actually - reconnect_prepare only is expected.
	 * if a double notify is triggered and we somehow get just the reconnect_reconnect,
	 * we, here, have jst discarded it - but it contains possibly valuable extra data. */
	if (Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE ||
		Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT)
	{
		GS_ERR_CLEAN(GS_ERRCODE_RECONNECT);
	}

	/* ensure request field validity by type check */
	if (Request.type != GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET)
		GS_ERR_CLEAN(1);

	if (oPacket)
		*oPacket = Request.mPacket;

	if (oId)
		*oId = Request.mId;

clean:

	return r;
}

/** Principal read facility for blocking-type worker.

	Due to reconnecting on timeout (and handling the double notify),
	This function must be ready to marshall a newly obtained ExtraWorker out.

	@param ioExtraWorkerCond replaced on _failure_ with GS_ERRCODE_RECONNECT

	@retval GS_ERRCODE_RECONNECT if reconnect request dequeued
	         either normally or after timeout.
*/
int gs_worker_packet_dequeue_timeout_reconnects(
	struct GsWorkerDataVec *WorkerDataVec,
	struct GsWorkerData *WorkerDataSend,
	gs_worker_id_t WorkerId,
	uint32_t TimeoutMs,
	struct GsAffinityQueue *AffinityQueue,
	struct GsAffinityToken *ioAffinityToken,
	struct GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId,
	struct GsExtraWorker **ioExtraWorkerCond)
{
	int r = 0;

	struct GsWorkerData *WorkerData = gs_worker_data_vec_id(WorkerDataVec, WorkerId);

	struct GsWorkerRequestData Request = {};

	/* attempt dequeuing a request normally.
	   GS_ERRCODE_TIMEOUT not considered an error and will be handled specially. */
	r = gs_affinity_queue_request_dequeue_and_acquire(
		AffinityQueue,
		WorkerDataVec,
		WorkerId,
		TimeoutMs,
		&Request,
		ioAffinityToken);
	if (!!r && r != GS_ERRCODE_TIMEOUT)
		GS_GOTO_CLEAN();

	/* GS_ERRCODE_TIMEOUT causes a second request dequeue attempt.
	   In case of, the reconnect protocol is also ran. */
	if (r == GS_ERRCODE_TIMEOUT)
		if (!!(r = gs_helper_api_worker_reconnect(WorkerData, WorkerDataSend, &Request)))
			GS_GOTO_CLEAN();

	if (Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT)
	{
		if (!!(r = gs_extra_worker_replace(ioExtraWorkerCond, Request.mExtraWorker)))
			GS_GOTO_CLEAN();

		GS_ERR_NO_CLEAN(GS_ERRCODE_RECONNECT);
	}

	GS_ASSERT(Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET);

noclean:
	if (oPacket && Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET)
		*oPacket = Request.mPacket;

	if (oId && Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET)
		*oId = Request.mId;

clean:

	return r;
}

int gs_worker_packet_dequeue_timeout_reconnects2(
	struct GsCrankData *CrankData,
	uint32_t TimeoutMs,
	struct GsAffinityToken *ioAffinityToken,
	struct GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId)
{
	return gs_worker_packet_dequeue_timeout_reconnects(
		CrankData->mWorkerDataVecRecv,
		CrankData->mWorkerDataSend,
		CrankData->mWorkerId,
		TimeoutMs,
		CrankData->mStoreWorker->mAffinityQueue,
		ioAffinityToken,
		oPacket,
		oId,
		&CrankData->mExtraWorker);
}

int gs_worker_data_create(struct GsWorkerData **oWorkerData)
{
	struct GsWorkerData *WorkerData = new GsWorkerData();

	WorkerData->mWorkerQueue = sp<std::deque<GsWorkerRequestData> >(new std::deque<GsWorkerRequestData>);
	WorkerData->mWorkerDataMutex = sp<std::mutex>(new std::mutex);
	WorkerData->mWorkerDataCond = sp<std::condition_variable>(new std::condition_variable);

	if (oWorkerData)
		*oWorkerData = WorkerData;

	return 0;
}

int gs_worker_data_destroy(struct GsWorkerData *WorkerData)
{
	GS_DELETE(&WorkerData, GsWorkerData);
	return 0;
}

int gs_worker_data_vec_create(
	uint32_t NumWorkers,
	struct GsWorkerDataVec **oWorkerDataVec)
{
	int r = 0;

	struct GsWorkerDataVec *WorkerDataVec = new GsWorkerDataVec();

	WorkerDataVec->mLen = NumWorkers;
	WorkerDataVec->mData = new GsWorkerData * [NumWorkers];

	for (size_t i = 0; i < WorkerDataVec->mLen; i++)
		if (!!(r = gs_worker_data_create(&WorkerDataVec->mData[i])))
			GS_GOTO_CLEAN();

	if (oWorkerDataVec)
		*oWorkerDataVec = WorkerDataVec;

clean:

	return r;
}

int gs_worker_data_vec_destroy(
	struct GsWorkerDataVec *WorkerDataVec)
{
	for (size_t i = 0; i < WorkerDataVec->mLen; i++)
		GS_DELETE_F(&WorkerDataVec->mData[i], gs_worker_data_destroy);
	delete [] WorkerDataVec->mData;
	delete WorkerDataVec;
	return 0;
}

struct GsWorkerData * gs_worker_data_vec_id(
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t WorkerId)
{
	if (WorkerId >= WorkerDataVec->mLen)
		GS_ASSERT(0);
	return WorkerDataVec->mData[WorkerId];
}
