#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <gittest/misc.h>
#include <gittest/log.h>
#include <gittest/net.h>
#include <gittest/net2.h>

/** Send Packet (remember enet_peer_send required an ownership release) */
int gs_connection_surrogate_packet_send(
	GsConnectionSurrogate *ConnectionSurrogate,
	GsPacket *ioPacket)
{
	int r = 0;

	ENetPacket *packet = ioPacket->mPacket.mPacket;

	/* ownership of packet is lost after enet_peer_send */
	if (!!(r = gs_packet_surrogate_release_ownership(&ioPacket->mPacket)))
		GS_GOTO_CLEAN();

	if (enet_peer_send(ConnectionSurrogate->mPeer, 0, packet))
		GS_ERR_CLEAN(1);

clean:

	return r;
}

/** NOTE: release ownership - not destroy */
int gs_packet_surrogate_release_ownership(struct GsPacketSurrogate *ioPacketSurrogate)
{
	ioPacketSurrogate->mPacket = NULL;
	return 0;
}

int gs_packet_create(
	struct GsPacket **oPacket,
	struct GsPacketSurrogate *valPacketSurrogate)
{
	struct GsPacket *Packet = new GsPacket();
	Packet->mPacket = *valPacketSurrogate;
	Packet->data = valPacketSurrogate->mPacket->data;
	Packet->dataLength = valPacketSurrogate->mPacket->dataLength;

	if (oPacket)
		*oPacket = Packet;

	return 0;
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

bool gs_worker_request_isempty(struct GsWorkerData *pThis)
{
	{
		std::unique_lock<std::mutex> lock(*pThis->mWorkerDataMutex);
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
		oValRequestData->clear();
		oValRequestData->swap(*pThis->mWorkerQueue);
	}
	return 0;
}

int gs_worker_packet_enqueue(
	GsWorkerData *pThis,
	GsIntrTokenSurrogate *IntrToken,
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

/* @retval: GS_ERRCODE_RECONNECT if reconnect request dequeued */
int gs_worker_packet_dequeue(
	GsWorkerData *pThis,
	GsPacket **oPacket,
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

int gs_ntwk_reconnect_expend(
	GsExtraHostCreate *ExtraHostCreate,
	GsWorkerData *WorkerDataRecv,
	ClntStateReconnect *ioStateReconnect,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsHostSurrogate *ioHostSurrogate,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

	if (*ioWantReconnect) {

		GsExtraWorker *ExtraWorker = NULL;

		GS_LOG(I, S, "reconnection wanted - performing");

		if (!!(r = ExtraHostCreate->cb_create_t(
			ExtraHostCreate,
			ioHostSurrogate,
			ioConnectionSurrogateMap,
			&ExtraWorker)))
		{
			GS_GOTO_CLEAN();
		}

		GS_LOG(I, S, "notifying worker");

		if (!!(r = gs_worker_request_enqueue_double_notify(WorkerDataRecv, ExtraWorker)))
			GS_GOTO_CLEAN();
	}

	/* connection is ensured if no errors occurred (either existing or newly established)
	*  in either case we no longer need to reconnect */
	if (ioWantReconnect)
		*ioWantReconnect = false;

clean:

	return r;
}

int gs_ntwk_host_service_wrap_want_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk  *StoreNtwk,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	uint32_t WantReconnect = false;

	if (!!(r = gs_ntwk_host_service(
		WorkerDataRecv,
		WorkerDataSend,
		StoreNtwk,
		HostSurrogate,
		ioConnectionSurrogateMap)))
	{
		GS_ERR_NO_CLEAN(1);
	}

noclean:
	if (!!r) {
		WantReconnect = true;
	}

	if (ioWantReconnect)
		*ioWantReconnect = WantReconnect;

clean:

	return r;
}

/** Two part connection registration.
    The connection is assigned an entry with Id within the connection map.
	That same Id is then bonded to the ENetPeer 'data' field.

   @param valConnectionSurrogate copied / copy-constructed (for shared_ptr use)
*/
int gs_aux_aux_aux_connection_register_transfer_ownership(
	GsConnectionSurrogate valConnectionSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t *oAssignedId)
{
	int r = 0;

	ENetPeer *peer = valConnectionSurrogate.mPeer;

	gs_connection_surrogate_id_t Id = 0;

	GS_BYPART_DATA_VAR(GsConnectionSurrogateId, ctxstruct);

	/* assign entry with id */
	if (!!(r = gs_connection_surrogate_map_insert(
		ioConnectionSurrogateMap,
		valConnectionSurrogate,
		&Id)))
	{
		GS_GOTO_CLEAN();
	}

	GS_BYPART_DATA_INIT(GsConnectionSurrogateId, ctxstruct, Id);

	/* bond to the peer */
	/* NOTE: making use of property that mPeer pointer field was copied into the connection surrogate map */
	// FIXME: sigh raw allocation, deletion occurs in principle on receipt of ENET_EVENT_TYPE_DISCONNECT events
	peer->data = new GsBypartCbDataGsConnectionSurrogateId(ctxstruct);

	if (oAssignedId)
		*oAssignedId = Id;

clean:

	return r;
}

int gs_aux_aux_aux_cb_last_chance_t(
	struct ENetIntr *Intr,
	struct ENetIntrToken *IntrToken)
{
	int r = 0;

	ENetIntrNtwk *pIntr = (ENetIntrNtwk *) Intr;

	if (! gs_worker_request_isempty(pIntr->WorkerDataSend))
		return 1;

	return 0;
}

int gs_ntwk_host_service(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk  *StoreNtwk,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	int errService = 0;

	ENetEvent event = {};

	ENetIntrNtwk Intr = {};
	Intr.base.cb_last_chance = gs_aux_aux_aux_cb_last_chance_t;
	Intr.WorkerDataSend = WorkerDataSend;

	while (0 <= (errService = enet_host_service_interruptible(
		HostSurrogate->mHost,
		&event,
		GS_TIMEOUT_1SEC,
		StoreNtwk->mIntrTokenSurrogate.mIntrToken,
		&Intr.base)))
	{
		/* two part reaction to a service call completing:
		*    service sends
		*    service event (if any) */

		/* service sends */

		{
			std::deque<GsWorkerRequestData> RequestSend;

			if (!!(r = gs_worker_request_dequeue_all_opt_cpp(WorkerDataSend, &RequestSend)))
				GS_GOTO_CLEAN();

			for (uint32_t i = 0; i < RequestSend.size(); i++)
			{
				gs_connection_surrogate_id_t IdOfSend = RequestSend[i].mId;

				struct GsConnectionSurrogate ConnectionSurrogateSend = {};
				uint32_t ConnectionSurrogateSendIsPresent = false;

				GS_LOG(I, S, "processing send");

				GS_ASSERT(RequestSend[i].type == GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET);

				if (!!(r = gs_connection_surrogate_map_get_try(
					ioConnectionSurrogateMap,
					IdOfSend,
					&ConnectionSurrogateSend,
					&ConnectionSurrogateSendIsPresent)))
					GS_GOTO_CLEAN();

				/* if a reconnection occurred, outstanding send requests would have missing send IDs */
				if (! ConnectionSurrogateSendIsPresent) {
					GS_LOG(W, PF, "suppressing packet for GsConnectionSurrogate [%llu]", (unsigned long long) IdOfSend);
					continue;
				}

				/* did not remove a surrogate ID soon enough? */
				/* FIXME: racing against worker, right?
				     just before reconnect, worker may have queued some requests against the current host.
					 after reconnect, the queued requests get processed (ex here), and the host mismatches. */
				if (ConnectionSurrogateSend.mHost != HostSurrogate->mHost) {
					GS_LOG(W, PF, "suppressing packet for GsConnectionSurrogate [%llu]", (unsigned long long) IdOfSend);
					continue;
				}

				if (!!(r = gs_connection_surrogate_packet_send(
					&ConnectionSurrogateSend,
					RequestSend[i].mPacket)))
				{
					GS_GOTO_CLEAN();
				}
			}

			/* absolutely no reason to flush if nothing was sent */

			if (RequestSend.size())
				enet_host_flush(HostSurrogate->mHost);
		}

		/* service event */
		
		if (errService == 0)
			continue;

		switch (event.type)
		{
		
		case ENET_EVENT_TYPE_CONNECT:
		{
			GS_LOG(I, S, "ENET_EVENT_TYPE_CONNECT");

			gs_connection_surrogate_id_t AssignedId = 0;

			GsConnectionSurrogate ConnectionSurrogate = {};

			ConnectionSurrogate.mHost = HostSurrogate->mHost;
			ConnectionSurrogate.mPeer = event.peer;
			ConnectionSurrogate.mIsPrincipalClientConnection = false;

			if (!!(r = gs_aux_aux_aux_connection_register_transfer_ownership(
				ConnectionSurrogate,
				ioConnectionSurrogateMap,
				&AssignedId)))
			{
				GS_GOTO_CLEAN();
			}

			GS_LOG(I, PF, "%llu connected [from %x:%u]",
				(unsigned long long)AssignedId,
				event.peer->address.host,
				event.peer->address.port);
		}
		break;

		case ENET_EVENT_TYPE_DISCONNECT:
		{
			gs_connection_surrogate_id_t Id = 0;

			GS_LOG(I, S, "ENET_EVENT_TYPE_DISCONNECT");

			/* get the id, then just dispose of the structure */
			{
				GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, event.peer->data);
				Id = ctxstruct->m0Id;
				// FIXME: sigh raw deletion, should have been allocated at ENET_EVENT_TYPE_CONNECT
				delete ctxstruct;
			}

			if (!!(r = gs_connection_surrogate_map_erase(ioConnectionSurrogateMap, Id)))
				GS_GOTO_CLEAN();

			GS_LOG(I, PF, "%llu disconnected",
				(unsigned long long)Id);
		}
		break;

		case ENET_EVENT_TYPE_RECEIVE:
		{
			gs_connection_surrogate_id_t IdOfRecv = 0;
			GsPacketSurrogate PacketSurrogate = {};
			GsPacket *Packet = {};
			GsWorkerRequestData RequestRecv = {};

			PacketSurrogate.mPacket = event.packet;

			GS_LOG(I, S, "ENET_EVENT_TYPE_RECEIVE");

			GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, event.peer->data);

			IdOfRecv = ctxstruct->m0Id;

			if (!!(r = gs_packet_create(&Packet, &PacketSurrogate)))
				GS_GOTO_CLEAN();

			{
				GsFrameType FoundFrameType = {};
				uint32_t Offset = 0;

				if (!!(r = aux_frame_read_frametype(Packet->data, Packet->dataLength, Offset, &Offset, &FoundFrameType)))
					GS_GOTO_CLEAN();

				GS_LOG(I, PF, "packet received [%.*s]", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);
			}

			if (!!(r = gs_worker_request_data_type_packet_make(Packet, ctxstruct->m0Id, &RequestRecv)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_request_enqueue(WorkerDataRecv, &RequestRecv)))
				GS_GOTO_CLEAN();
		}
		break;

		}
	}

	if (errService < 0)
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int gs_ntwk_reconnecter(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreNtwk> StoreNtwk,
	sp<GsExtraHostCreate> ExtraHostCreate)
{
	int r = 0;

	ClntStateReconnect StateReconnect = {};

	sp<GsConnectionSurrogateMap> ConnectionSurrogateMap(new GsConnectionSurrogateMap());

	GsHostSurrogate HostSurrogate = {};

	uint32_t WantReconnect = true;

	GS_LOG(I, S, "entering reconnect-service cycle");

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

	/* NOTE: special error handling */
	while (true) {

		/* NOTE: no_clean */
		if (!!(r = gs_ntwk_reconnect_expend(
			ExtraHostCreate.get(),
			WorkerDataRecv.get(),
			&StateReconnect,
			ConnectionSurrogateMap.get(),
			&HostSurrogate,
			&WantReconnect)))
		{
			GS_ERR_NO_CLEAN(r);
		}

		/* NOTE: cleansub */
		if (!!(r = gs_ntwk_host_service_wrap_want_reconnect(
			WorkerDataRecv.get(),
			WorkerDataSend.get(),
			StoreNtwk.get(),
			&HostSurrogate,
			ConnectionSurrogateMap.get(),
			&WantReconnect)))
		{
			GS_GOTO_CLEANSUB();
		}

	cleansub:
		if (!!r) {
			GS_LOG(E, S, "clnt_serv error into reconnect attempt");
		}
	}

noclean:

clean:

	return r;
}

void gs_ntwk_thread_func(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreNtwk> StoreNtwk,
	sp<GsExtraHostCreate> ExtraHostCreate,
	const char *optExtraThreadName)
{
	int r = 0;

	std::string ThreadName("ntwk_");

	if (optExtraThreadName)
		ThreadName.append(optExtraThreadName);

	gs_current_thread_name_set_cstr(ThreadName.c_str());

	log_guard_t log(GS_LOG_GET(ThreadName.c_str()));

	if (!!(r = gs_ntwk_reconnecter(
		WorkerDataRecv,
		WorkerDataSend,
		StoreNtwk,
		ExtraHostCreate)))
	{
		GS_ASSERT(0);
	}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

/** we are expecting: possibly a reconnect_prepare, mandatorily a reconnect
    @return GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT type request
	@sa
	   GsWorkerRequestDataType
*/
int gs_worker_dequeue_handling_double_notify(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerRequestData *outValRequest)
{
	int r = 0;

	GsWorkerRequestData Request = {};

	/* possibly reconnect_prepare */
	if (!!(r = gs_worker_request_dequeue(WorkerDataRecv, &Request)))
		GS_GOTO_CLEAN();

	/* if indeed reconnect_prepare, skip it */
	if (Request.type == GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE) {
		if (!!(r = gs_worker_request_dequeue(WorkerDataRecv, &Request)))
			GS_GOTO_CLEAN();
	}

	if (Request.type != GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT)
		GS_ERR_CLEAN_L(1, E, S, "suspected invalid double notify sequence");

	if (outValRequest)
		*outValRequest = Request;

clean:

	return r;
}

/** @param oExtraWorkerOpt do not reseat unless a reconnect has actually occurred.
           beware of a reseat potentally the last reference to a GsExtraWorker, causing a leak.
*/
int gs_worker_reconnect_expend(
	struct GsWorkerData *WorkerDataRecv,
	struct GsExtraWorker **oExtraWorkerCond,
	struct ClntStateReconnect *ioStateReconnect,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	GsExtraWorker *ExtraWorker = *oExtraWorkerCond;

	if (!!(r = clnt_state_reconnect_expend(ioStateReconnect)))
		GS_GOTO_CLEAN();

	if (*ioWantReconnect) {

		GsWorkerRequestData Reconnect = {};

		GS_LOG(I, S, "reconnection wanted");

		GS_LOG(I, S, "receiving notification");

		if (!!(r = gs_worker_dequeue_handling_double_notify(WorkerDataRecv, &Reconnect)))
			GS_GOTO_CLEAN();

		GS_LOG(I, S, "received notification");

		ExtraWorker = Reconnect.mExtraWorker;
	}

	if (oExtraWorkerCond)
		*oExtraWorkerCond = ExtraWorker;

	if (ioWantReconnect)
		*ioWantReconnect = false;

clean:

	return r;
}

int gs_worker_service_wrap_want_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker *ExtraWorker,
	uint32_t *ioWantReconnect)
{
	int r = 0;

	uint32_t WantReconnect = false;

	if (!!(r = StoreWorker->cb_crank_t(
		WorkerDataRecv,
		WorkerDataSend,
		StoreWorker,
		ExtraWorker)))
	{
		GS_ERR_NO_CLEAN(1);
	}

noclean:
	if (!!r) {
		WantReconnect = true;
	}

	if (ioWantReconnect)
		*ioWantReconnect = WantReconnect;

clean:

	return r;
}

int gs_worker_reconnecter(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreWorker> StoreWorker)
{
	int r = 0;

	ClntStateReconnect StateReconnect = {};

	GsExtraWorker *ExtraWorker = NULL;

	uint32_t WantReconnect = true;

	GS_LOG(I, S, "entering reconnect-service cycle");

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

	/* NOTE: special error handling */
	while (true) {

		/* NOTE: no_clean */
		if (!!(r = gs_worker_reconnect_expend(
			WorkerDataRecv.get(),
			&ExtraWorker,
			&StateReconnect,
			&WantReconnect)))
		{
			GS_ERR_NO_CLEAN(1);
		}

		/* NOTE: cleansub */
		if (!!(r = gs_worker_service_wrap_want_reconnect(
			WorkerDataRecv.get(),
			WorkerDataSend.get(),
			StoreWorker.get(),
			ExtraWorker,
			&WantReconnect)))
		{
			GS_GOTO_CLEANSUB();
		}

	cleansub:
		if (!!r) {
			GS_LOG(E, S, "clnt_worker error into reconnect attempt");
		}
	}

noclean:

clean:

	return r;
}

int gs_worker_thread_func(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreWorker> StoreWorker,
	const char *optExtraThreadName)
{
	int r = 0;

	std::string ThreadName("work_");

	if (optExtraThreadName)
		ThreadName.append(optExtraThreadName);

	gs_current_thread_name_set_cstr(ThreadName.c_str());

	log_guard_t log(GS_LOG_GET(ThreadName.c_str()));

	if (!!(r = gs_worker_reconnecter(
		WorkerDataRecv,
		WorkerDataSend,
		StoreWorker)))
	{
		GS_ASSERT(0);
	}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

int gs_net_full_create_connection(
	uint32_t ServPort,
	sp<GsExtraHostCreate> pExtraHostCreate,
	sp<GsStoreNtwk>       pStoreNtwk,
	sp<GsStoreWorker>     pStoreWorker,
	sp<GsFullConnection> *oConnection)
{
	int r = 0;

	sp<GsFullConnection> Connection;

	GsWorkerData *rawWorkerDataRecv = NULL;
	GsWorkerData *rawWorkerDataSend = NULL;

	sp<GsWorkerData> WorkerDataRecv;
	sp<GsWorkerData> WorkerDataSend;

	sp<std::thread> ClientWorkerThread;
	sp<std::thread> ClientNtwkThread;

	if (!!(r = gs_worker_data_create(&rawWorkerDataRecv)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_data_create(&rawWorkerDataSend)))
		GS_GOTO_CLEAN();

	GS_SP_SET_RAW_NULLING(WorkerDataRecv, rawWorkerDataRecv, GsWorkerData);
	GS_SP_SET_RAW_NULLING(WorkerDataSend, rawWorkerDataSend, GsWorkerData);

	ClientWorkerThread = sp<std::thread>(new std::thread(
		gs_worker_thread_func,
		WorkerDataRecv,
		WorkerDataSend,
		pStoreWorker,
		"clnt"));

	ClientNtwkThread = sp<std::thread>(new std::thread(
		gs_ntwk_thread_func,
		WorkerDataRecv,
		WorkerDataSend,
		pStoreNtwk,
		pExtraHostCreate,
		"clnt"));

	Connection = sp<GsFullConnection>(new GsFullConnection);
	Connection->ThreadNtwk = ClientNtwkThread;
	Connection->ThreadWorker = ClientWorkerThread;
	Connection->ThreadNtwkExtraHostCreate = pExtraHostCreate;

	if (oConnection)
		*oConnection = Connection;

clean:

	return r;
}
