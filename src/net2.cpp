#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <gittest/misc.h>
#include <gittest/log.h>
#include <gittest/net2.h>

/** Use as a deleter function of std::thread shared pointers
	whom are to be detached before destruction.

    @sa
	   ::GsFullConnection
*/
static void gs_sp_thread_detaching_deleter(std::thread *t)
{
	if (t->joinable())
		t->detach();
	delete t;
}

int gs_bypart_cb_OidVector(void *ctx, const char *d, int64_t l) {
	int r = 0;

	git_oid Oid = {};
	GS_BYPART_DATA_VAR_CTX_NONUCF(OidVector, Data, ctx);

	if (!!(r = aux_frame_read_oid((uint8_t *)d, (uint32_t)l, 0, NULL, (uint8_t *)Oid.id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	Data->m0OidVec->push_back(Oid);

clean:

	return r;
}

int gs_connection_surrogate_map_create(GsConnectionSurrogateMap **oConnectionSurrogateMap)
{
	GsConnectionSurrogateMap *ConnectionSurrogateMap = new GsConnectionSurrogateMap();
	
	ConnectionSurrogateMap->mAtomicCount = std::atomic<uint32_t>(0);
	ConnectionSurrogateMap->mConnectionSurrogateMap = sp<gs_connection_surrogate_map_t>(new gs_connection_surrogate_map_t);

	if (oConnectionSurrogateMap)
		*oConnectionSurrogateMap = ConnectionSurrogateMap;

	return 0;
}

int gs_connection_surrogate_map_clear(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	ioConnectionSurrogateMap->mConnectionSurrogateMap->clear();
	return 0;
}

int gs_connection_surrogate_map_insert_id(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	const struct GsConnectionSurrogate valConnectionSurrogate)
{
	int r = 0;

	if (! ioConnectionSurrogateMap->mConnectionSurrogateMap->insert(
			gs_connection_surrogate_map_t::value_type(ConnectionSurrogateId, valConnectionSurrogate)).second)
	{
		GS_ERR_CLEAN_L(1, E, S, "insertion prevented (is a stale element present, and why?)");
	}

clean:

	return r;
}

int gs_connection_surrogate_map_insert(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	const struct GsConnectionSurrogate valConnectionSurrogate,
	gs_connection_surrogate_id_t *oConnectionSurrogateId)
{
	int r = 0;

	gs_connection_surrogate_id_t Id = ioConnectionSurrogateMap->mAtomicCount.fetch_add(1);

	if (!!(r = gs_connection_surrogate_map_insert_id(ioConnectionSurrogateMap, Id, valConnectionSurrogate)))
		GS_GOTO_CLEAN();

	if (oConnectionSurrogateId)
		*oConnectionSurrogateId = Id;

clean:

	return r;
}

int gs_connection_surrogate_map_get_try(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	struct GsConnectionSurrogate *oConnectionSurrogate,
	uint32_t *oIsPresent)
{
	int r = 0;

	struct GsConnectionSurrogate ConnectionSurrogate = {};
	uint32_t IsPresent = false;

	gs_connection_surrogate_map_t::iterator it =
		ioConnectionSurrogateMap->mConnectionSurrogateMap->find(ConnectionSurrogateId);

	if (it != ioConnectionSurrogateMap->mConnectionSurrogateMap->end()) {
		ConnectionSurrogate = it->second;
		IsPresent = true;
	}

	if (oConnectionSurrogate)
		*oConnectionSurrogate = ConnectionSurrogate;

	if (oIsPresent)
		*oIsPresent = IsPresent;

clean:

	return r;
}

int gs_connection_surrogate_map_get(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	struct GsConnectionSurrogate *oConnectionSurrogate)
{
	int r = 0;

	uint32_t IsPresent = false;

	if (!!(r = gs_connection_surrogate_map_get_try(
		ioConnectionSurrogateMap,
		ConnectionSurrogateId,
		oConnectionSurrogate,
		&IsPresent)))
	{
		GS_GOTO_CLEAN();
	}

	if (! IsPresent)
		GS_ERR_CLEAN_L(1, E, S, "retrieval prevented (is an element missing, and why?)");

clean:

	return r;
}

int gs_connection_surrogate_map_erase(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId)
{
	int r = 0;

	gs_connection_surrogate_map_t::iterator it =
		ioConnectionSurrogateMap->mConnectionSurrogateMap->find(ConnectionSurrogateId);

	if (it == ioConnectionSurrogateMap->mConnectionSurrogateMap->end())
		GS_ERR_CLEAN_L(1, E, S, "removal prevented (is an element missing, and why?)");
	else
		ioConnectionSurrogateMap->mConnectionSurrogateMap->erase(it);

clean:

	return r;
}

int clnt_state_reconnect_make_default(ClntStateReconnect *oStateReconnect) {
	ClntStateReconnect StateReconnect;
	StateReconnect.NumReconnections = GS_CONNECT_NUMRECONNECT;
	StateReconnect.NumReconnectionsLeft = StateReconnect.NumReconnections;
	if (oStateReconnect)
		*oStateReconnect = StateReconnect;
	return 0;
}

bool clnt_state_reconnect_have_remaining(ClntStateReconnect *StateReconnect) {
	return StateReconnect->NumReconnectionsLeft >= 1;
}

int clnt_state_reconnect_expend(ClntStateReconnect *ioStateReconnect) {
	int r = 0;

	if (! clnt_state_reconnect_have_remaining(ioStateReconnect))
		GS_ERR_CLEAN(1);

	ioStateReconnect->NumReconnectionsLeft -= 1;

clean:

	return r;
}

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

int gs_ctrl_con_create(struct GsCtrlCon **oCtrlCon, uint32_t ExitedSignalLeft)
{
	struct GsCtrlCon *CtrlCon = new GsCtrlCon();

	CtrlCon->mExitedSignalLeft = ExitedSignalLeft;
	CtrlCon->mCtrlConMutex = sp<std::mutex>(new std::mutex);
	CtrlCon->mCtrlConCondExited = sp<std::condition_variable>(new std::condition_variable);

	if (oCtrlCon)
		*oCtrlCon = CtrlCon;

	return 0;
}

int gs_ctrl_con_signal_exited(struct GsCtrlCon *CtrlCon)
{
	bool WantNotify = false;
	{
		std::unique_lock<std::mutex> lock(*CtrlCon->mCtrlConMutex);
		if (CtrlCon->mExitedSignalLeft)
			CtrlCon->mExitedSignalLeft -= 1;
		if (!CtrlCon->mExitedSignalLeft)
			WantNotify = true;
	}
	if (WantNotify)
		CtrlCon->mCtrlConCondExited->notify_all();
	return 0;
}

int gs_ctrl_con_wait_exited(struct GsCtrlCon *CtrlCon)
{
	{
		std::unique_lock<std::mutex> lock(*CtrlCon->mCtrlConMutex);
		CtrlCon->mCtrlConCondExited->wait(lock, [&]() { return ! CtrlCon->mExitedSignalLeft; });
	}
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

int gs_ntwk_host_service_worker_disconnect(
	struct GsHostSurrogate *ReferenceHostSurrogate,
	struct GsWorkerRequestData *RequestSend,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	struct GsConnectionSurrogate Con = {};
	uint32_t ConIsPresent = false;

	if (!!(r = gs_connection_surrogate_map_get_try(
		ioConnectionSurrogateMap,
		RequestSend->mId,
		&Con,
		&ConIsPresent)))
	{
		GS_GOTO_CLEAN();
	}

	if (!ConIsPresent)
		GS_ERR_NO_CLEAN_L(0, W, PF, "suppressing disconnect for GsConnectionSurrogate [%llu]", (unsigned long long) RequestSend->mId);

	if (Con.mHost != ReferenceHostSurrogate->mHost)
		GS_ERR_NO_CLEAN_L(0, W, PF, "suppressing disconnect for GsConnectionSurrogate [%llu]", (unsigned long long) RequestSend->mId);

	if (!!(r = gs_connection_surrogate_map_erase(ioConnectionSurrogateMap, RequestSend->mId)))
		GS_GOTO_CLEAN();

	enet_peer_disconnect(Con.mPeer, 0);

noclean:

clean:

	return r;
}

int gs_ntwk_host_service_worker_packet(
	struct GsHostSurrogate *ReferenceHostSurrogate,
	struct GsWorkerRequestData *RequestSend,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	gs_connection_surrogate_id_t IdOfSend = RequestSend->mId;

	struct GsConnectionSurrogate Con = {};
	uint32_t ConIsPresent = false;

	if (!!(r = gs_connection_surrogate_map_get_try(
		ioConnectionSurrogateMap,
		IdOfSend,
		&Con,
		&ConIsPresent)))
	{
		GS_GOTO_CLEAN();
	}

	/* if a reconnection occurred, outstanding send requests would have missing send IDs */
	if (! ConIsPresent)
		GS_ERR_NO_CLEAN_L(0, W, PF, "suppressing packet for GsConnectionSurrogate [%llu]", (unsigned long long) IdOfSend);

	/* did not remove a surrogate ID soon enough? */
	/* FIXME: racing against worker, right?
			just before reconnect, worker may have queued some requests against the current host.
			after reconnect, the queued requests get processed (ex here), and the host mismatches. */
	if (Con.mHost != ReferenceHostSurrogate->mHost)
		GS_ERR_NO_CLEAN_L(0, W, PF, "suppressing packet for GsConnectionSurrogate [%llu]", (unsigned long long) IdOfSend);

	if (!!(r = gs_connection_surrogate_packet_send(
		&Con,
		RequestSend->mPacket)))
	{
		GS_GOTO_CLEAN();
	}

noclean:

clean:

	return r;
}

int gs_ntwk_host_service_sends(
	struct GsWorkerData *WorkerDataSend,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap)
{
	int r = 0;

	std::deque<GsWorkerRequestData> RequestSend;

	if (!!(r = gs_worker_request_dequeue_all_opt_cpp(WorkerDataSend, &RequestSend)))
		GS_GOTO_CLEAN();

	for (uint32_t i = 0; i < RequestSend.size(); i++)
	{
		switch (RequestSend[i].type)
		{
		case GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT:
		{
			GS_LOG(I, S, "GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT");

			GS_ERR_CLEAN(GS_ERRCODE_EXIT);
		}
		break;

		case GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT:
		{
			GS_LOG(I, S, "GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT");

			if (!!(r = gs_ntwk_host_service_worker_disconnect(
				HostSurrogate,
				&RequestSend[i],
				ioConnectionSurrogateMap)))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET:
		{
			GS_LOG(I, S, "GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET");

			if (!!(r = gs_ntwk_host_service_worker_packet(
				HostSurrogate,
				&RequestSend[i],
				ioConnectionSurrogateMap)))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		default:
			GS_ASSERT(0);
			break;
		}
	}

	/* absolutely no reason to flush if nothing was sent */
	// FIXME: flush only if any GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET were serviced

	if (RequestSend.size())
		enet_host_flush(HostSurrogate->mHost);

clean:

	return r;
}

int gs_ntwk_host_service_event(
	struct GsWorkerData *WorkerDataRecv,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	int errService,
	GsEventSurrogate *Event)
{
	int r = 0;

	if (errService == 0)
		{ r = 0; goto clean; } // not GS_ERR_NO_CLEAN(0) due to high call volume

	switch (Event->event.type)
	{
		
	case ENET_EVENT_TYPE_CONNECT:
	{
		GS_LOG(I, S, "ENET_EVENT_TYPE_CONNECT");

		gs_connection_surrogate_id_t AssignedId = 0;

		GsConnectionSurrogate ConnectionSurrogate = {};

		ConnectionSurrogate.mHost = HostSurrogate->mHost;
		ConnectionSurrogate.mPeer = Event->event.peer;
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
			Event->event.peer->address.host,
			Event->event.peer->address.port);
	}
	break;

	case ENET_EVENT_TYPE_DISCONNECT:
	{
		gs_connection_surrogate_id_t Id = 0;

		GS_LOG(I, S, "ENET_EVENT_TYPE_DISCONNECT");

		/* get the id, then just dispose of the structure */
		{
			GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, Event->event.peer->data);
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

		PacketSurrogate.mPacket = Event->event.packet;

		GS_LOG(I, S, "ENET_EVENT_TYPE_RECEIVE");

		GS_BYPART_DATA_VAR_CTX_NONUCF(GsConnectionSurrogateId, ctxstruct, Event->event.peer->data);

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

noclean:

clean:

	return r;
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

	GsEventSurrogate Event = {};

	ENetIntrNtwk Intr = {};
	Intr.base.cb_last_chance = gs_aux_aux_aux_cb_last_chance_t;
	Intr.WorkerDataSend = WorkerDataSend;

	while (0 <= (errService = enet_host_service_interruptible(
		HostSurrogate->mHost,
		&Event.event,
		GS_TIMEOUT_1SEC,
		StoreNtwk->mIntrTokenSurrogate.mIntrToken,
		&Intr.base)))
	{
		if (!!(r = gs_ntwk_host_service_sends(
			WorkerDataSend,
			HostSurrogate,
			ioConnectionSurrogateMap)))
		{
			GS_GOTO_CLEAN();
		}

		if (!!(r = gs_ntwk_host_service_event(
			WorkerDataRecv,
			HostSurrogate,
			ioConnectionSurrogateMap,
			errService,
			&Event)))
		{
			GS_GOTO_CLEAN();
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

	GsConnectionSurrogateMap *rawConnectionSurrogateMap = NULL;

	sp<GsConnectionSurrogateMap> ConnectionSurrogateMap;

	GsHostSurrogate HostSurrogate = {};

	uint32_t WantReconnect = true;

	GS_LOG(I, S, "entering reconnect-service cycle");

	if (!!(r = gs_connection_surrogate_map_create(&rawConnectionSurrogateMap)))
		GS_GOTO_CLEAN();

	GS_SP_SET_RAW_NULLING(ConnectionSurrogateMap, rawConnectionSurrogateMap, GsConnectionSurrogateMap);

	if (!!(r = clnt_state_reconnect_make_default(&StateReconnect)))
		GS_GOTO_CLEAN();

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

		/* NOTE: special error handling */
		r = gs_ntwk_host_service(
			WorkerDataRecv.get(),
			WorkerDataSend.get(),
			StoreNtwk.get(),
			&HostSurrogate,
			ConnectionSurrogateMap.get());

		if (!!r && r == GS_ERRCODE_RECONNECT) {
			GS_LOG(E, S, "ntwk reconnect attempt");
			WantReconnect = true;
			continue;
		}
		if (!!r)
			GS_ERR_NO_CLEAN(r);
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

	r = gs_ntwk_reconnecter(
		WorkerDataRecv,
		WorkerDataSend,
		StoreNtwk,
		ExtraHostCreate);

	if (!!r && r == GS_ERRCODE_EXIT) {
		
		if (!!(r = gs_ctrl_con_signal_exited(StoreNtwk->mCtrlCon)))
			GS_ERR_CLEAN(r);

		GS_ERR_CLEAN(r);
	}
	if (!!r) {
		//GS_ASSERT(0);
		// FIXME:
		gs_log_crash_handler_dump_global_log_list_suffix(ThreadName.c_str(), ThreadName.size());
	}

clean:

	GS_LOG(I, S, "thread exiting");

	/* NOTE: void return */
}

int gs_worker_exit(
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker)
{
	int r = 0;

	struct GsWorkerRequestData RequestExit = {};

	if (!!(r = gs_ctrl_con_signal_exited(StoreWorker->mCtrlCon)))
		GS_ERR_CLEAN(r);

	if (!!(r = gs_worker_request_data_type_exit_make(&RequestExit)))
		GS_ERR_CLEAN(r);

	if (!!(r = gs_worker_request_enqueue(WorkerDataSend, &RequestExit)))
		GS_ERR_CLEAN(r);

clean:

	return r;
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

/** @param ioExtraWorker beware of leaking a pointed-to pre-existing ExtraWorker */
int gs_worker_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsExtraWorker **ioExtraWorker)
{
	int r = 0;

	GsWorkerRequestData Reconnect = {};

	GS_LOG(I, S, "reconnection wanted");

	GS_LOG(I, S, "receiving notification");

	if (!!(r = gs_worker_dequeue_handling_double_notify(WorkerDataRecv, &Reconnect)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "received notification");

	// FIXME: cb_destroy_t return value
	if (ioExtraWorker && (*ioExtraWorker))
		(*ioExtraWorker)->cb_destroy_t(*ioExtraWorker);

	if (ioExtraWorker)
		*ioExtraWorker = Reconnect.mExtraWorker;

clean:

	return r;
}

int gs_worker_reconnecter(
	GsWorkerData *WorkerDataRecv,
	GsWorkerData *WorkerDataSend,
	GsStoreWorker *StoreWorker)
{
	int r = 0;

	GsExtraWorker *ExtraWorker = NULL;

	GS_LOG(I, S, "entering reconnect-service cycle");

	while (true) {

		if (!!(r = gs_worker_reconnect(WorkerDataRecv, &ExtraWorker)))
			GS_GOTO_CLEAN();

		r = StoreWorker->cb_crank_t(
			WorkerDataRecv,
			WorkerDataSend,
			StoreWorker,
			ExtraWorker);

		if (!!r && r == GS_ERRCODE_RECONNECT) {
			GS_LOG(E, S, "worker reconnect attempt");
			continue;
		}
		if (!!r && r == GS_ERRCODE_EXIT) {
			GS_LOG(E, S, "worker exit attempt");
			if (!!(r = gs_worker_exit(WorkerDataSend, StoreWorker)))
				GS_GOTO_CLEAN();
			GS_ERR_NO_CLEAN(0);
		}
		if (!!r)
			GS_GOTO_CLEAN();
	}

noclean:

clean:
	// FIXME: cb_destroy_t return value
	if (ExtraWorker)
		ExtraWorker->cb_destroy_t(ExtraWorker);

	return r;
}

void gs_worker_thread_func(
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
		WorkerDataRecv.get(),
		WorkerDataSend.get(),
		StoreWorker.get())))
	{
		GS_GOTO_CLEAN();
	}

clean:

	GS_LOG(I, S, "thread exiting");

	if (!!r) {
		GS_ASSERT(0);
	}

	/* NOTE: void return */
}

int gs_net_full_create_connection(
	uint32_t ServPort,
	sp<GsExtraHostCreate> pExtraHostCreate,
	sp<GsStoreNtwk>       pStoreNtwk,
	sp<GsStoreWorker>     pStoreWorker,
	sp<GsFullConnection> *oConnection,
	const char *optExtraThreadName)
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
			optExtraThreadName),
		gs_sp_thread_detaching_deleter);

	ClientNtwkThread = sp<std::thread>(new std::thread(
			gs_ntwk_thread_func,
			WorkerDataRecv,
			WorkerDataSend,
			pStoreNtwk,
			pExtraHostCreate,
			optExtraThreadName),
		gs_sp_thread_detaching_deleter);

	Connection = sp<GsFullConnection>(new GsFullConnection);
	Connection->ThreadNtwk = ClientNtwkThread;
	Connection->ThreadWorker = ClientWorkerThread;
	Connection->ThreadNtwkExtraHostCreate = pExtraHostCreate;

	if (oConnection)
		*oConnection = Connection;

clean:

	return r;
}
