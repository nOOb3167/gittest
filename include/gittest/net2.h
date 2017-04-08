#ifndef _GITTEST_NET2_H_
#define _GITTEST_NET2_H_

#include <stddef.h>
#include <stdint.h>
#include <enet/enet.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>

#include <enet/enet.h>

#include <gittest/frame.h>

#define GS_PORT 3756

#define GS_SERV_AUX_ARBITRARY_TIMEOUT_MS 5000
#define GS_CONNECT_NUMRETRY   5
#define GS_CONNECT_TIMEOUT_MS 1000
#define GS_CONNECT_NUMRECONNECT 5
#define GS_RECEIVE_TIMEOUT_MS 500000

#define GS_TIMEOUT_1SEC 1000

#define GS_EXTRA_HOST_CREATE_CLIENT_MAGIC 0x501C325E
#define GS_EXTRA_WORKER_CLIENT_MAGIC      0x501C325F
#define GS_STORE_NTWK_CLIENT_MAGIC        0x501C3260
#define GS_STORE_WORKER_CLIENT_MAGIC      0x501C3261

#define GS_EXTRA_HOST_CREATE_SERVER_MAGIC 0x502C325E
#define GS_EXTRA_WORKER_SERVER_MAGIC      0x502C325F
#define GS_STORE_NTWK_SERVER_MAGIC        0x502C3260
#define GS_STORE_WORKER_SERVER_MAGIC      0x502C3261

#define GS_EXTRA_HOST_CREATE_SELFUPDATE_BASIC_MAGIC 0x503C325E
#define GS_EXTRA_WORKER_SELFUPDATE_BASIC_MAGIC      0x503C325F
#define GS_STORE_NTWK_SELFUPDATE_BASIC_MAGIC        0x503C3260
#define GS_STORE_WORKER_SELFUPDATE_BASIC_MAGIC      0x503C3261

/* GsBypartCbDataOidVector */
GS_BYPART_DATA_DECL(OidVector, std::vector<git_oid> *m0OidVec;);
#define GS_BYPART_TRIPWIRE_OidVector 0x23132358
#define GS_BYPART_DATA_INIT_OidVector(VARNAME, POIDVEC) (VARNAME).m0OidVec = POIDVEC;
int gs_bypart_cb_OidVector(void *ctx, const char *d, int64_t l);

struct GsConnectionSurrogate;

typedef uint64_t gs_connection_surrogate_id_t;
typedef ::std::map<gs_connection_surrogate_id_t, GsConnectionSurrogate> gs_connection_surrogate_map_t;

GS_BYPART_DATA_DECL(GsConnectionSurrogateId, gs_connection_surrogate_id_t m0Id;);
#define GS_BYPART_TRIPWIRE_GsConnectionSurrogateId 0x68347232
#define GS_BYPART_DATA_INIT_GsConnectionSurrogateId(VARNAME, ID) (VARNAME).m0Id = ID;

struct GsExtraWorker;
struct GsWorkerData;

/** Design:
	Entries enter this structure on connection (ENet ENET_EVENT_TYPE_CONNECT).
	Entries leave this structure on disconnection (ENet ENET_EVENT_TYPE_DISCONNECT).
	Throughout operation, IDs are dealt out (even to other threads eg worker).
	There may be attempted ID uses after an entry has already left the structure.
	Design operations (especially the query/get kind) to handle missing entries.

	@sa
       ::gs_connection_surrogate_map_create
	   ::gs_connection_surrogate_map_clear
	   ::gs_connection_surrogate_map_insert_id
	   ::gs_connection_surrogate_map_insert
	   ::gs_connection_surrogate_map_get_try
	   ::gs_connection_surrogate_map_get
	   ::gs_connection_surrogate_map_erase
*/
struct GsConnectionSurrogateMap {
	std::atomic<uint64_t> mAtomicCount;
	sp<gs_connection_surrogate_map_t> mConnectionSurrogateMap;
};

/** Wrapper for a count.
	Semantically the number of reconnections remaining.

    @sa
	   ::clnt_state_reconnect_make_default
	   ::clnt_state_reconnect_have_remaining
	   ::clnt_state_reconnect_expend
*/
struct ClntStateReconnect {
	uint32_t NumReconnections;
	uint32_t NumReconnectionsLeft;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

/** manual-init struct
    value struct
*/
struct ENetIntrNtwk {
	struct ENetIntr base;

	struct GsWorkerData *WorkerDataSend;
};

/** manual-init struct
    value struct
*/
struct GsIntrTokenSurrogate {
	struct ENetIntrToken *mIntrToken;
};

/** manual-init struct
    value struct
*/
struct GsHostSurrogate {
	ENetHost *mHost;
};

/** manual-init struct
    value struct
*/
struct GsConnectionSurrogate {
	uint32_t mIsPrincipalClientConnection;

	ENetHost *mHost;
	ENetPeer *mPeer;
};

/** manual-init struct
	value struct
*/
struct GsEventSurrogate {
	ENetEvent event;
};

/** manual-init struct
    value struct

    FIXME: think about using a shared pointer

	@sa
	   ::gs_packet_surrogate_release_ownership
*/
struct GsPacketSurrogate {
	ENetPacket *mPacket;
};

/** @sa
       ::gs_packet_create
*/
struct GsPacket {
	struct GsPacketSurrogate mPacket;

	uint8_t *data;
	size_t   dataLength;
};

/** manual-init struct
    value struct
*/
struct GsPacketWithOffset {
	struct GsPacket *mPacket;
	uint32_t  mOffsetSize;
	uint32_t  mOffsetObject;
};

enum GsWorkerRequestDataType
{
	GS_SERV_WORKER_REQUEST_DATA_TYPE_INVALID = 0,
	GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET = 1,
	GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE = 2,
	GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT = 3,
	GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT = 4,
	GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT = 5,
};

/**
  Extra data fields set inside this structure depend on the type field:
  - GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET: mPacket, mId
  - GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE: no data fields
  - GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT: mExtraWorker
  - GS_SERV_WORKER_REQUEST_DATA_TYPE_EXIT: no data fields
  - GS_SERV_WORKER_REQUEST_DATA_TYPE_DISCONNECT: mId

  @sa
     ::gs_worker_request_data_type_packet_make
	 ::gs_worker_request_data_type_reconnect_prepare_make
	 ::gs_worker_request_data_type_reconnect_reconnect_make
	 ::gs_worker_request_data_type_exit_make
*/
struct GsWorkerRequestData
{
	enum GsWorkerRequestDataType type;

	struct GsPacket      *mPacket;
	gs_connection_surrogate_id_t mId;

	struct GsExtraWorker *mExtraWorker;
};

/** @sa
       ::gs_worker_data_create
	   ::gs_worker_request_isempty
       ::gs_worker_request_enqueue
	   ::gs_worker_request_enqueue_double_notify
	   ::gs_worker_request_dequeue
	   ::gs_worker_packet_enqueue
	   ::gs_worker_packet_dequeue
*/
struct GsWorkerData
{
	sp<std::deque<GsWorkerRequestData> > mWorkerQueue;
	sp<std::mutex> mWorkerDataMutex;
	sp<std::condition_variable> mWorkerDataCond;
};

/** @sa
       ::gs_ctrl_con_create
	   ::gs_ctrl_con_signal_exited
	   ::gs_ctrl_con_wait_exited
*/
struct GsCtrlCon
{
	uint32_t mExitedSignalLeft;
	sp<std::mutex> mCtrlConMutex;
	sp<std::condition_variable> mCtrlConCondExited;
};

struct GsExtraHostCreate
{
	uint32_t magic;

	int(*cb_create_t)(
		struct GsExtraHostCreate *ExtraHostCreate,
		struct GsHostSurrogate *ioHostSurrogate,
		GsConnectionSurrogateMap *ioConnectionSurrogateMap,
		struct GsExtraWorker **oExtraWorker);
	// FIXME also destroy
};

struct GsExtraWorker
{
	uint32_t magic;

	// FIXME: cb_create_t should be a freestanding function - can't have
	//   derived struct specifics like gs_connection_surrogate_id..
	int(*cb_create_t)(
		struct GsExtraWorker **oExtraWorker,
		gs_connection_surrogate_id_t Id);
	int(*cb_destroy_t)(struct GsExtraWorker *ExtraWorker);
};

struct GsStoreNtwk
{
	uint32_t magic;

	struct GsIntrTokenSurrogate mIntrTokenSurrogate;

	struct GsCtrlCon *mCtrlCon;
};

struct GsStoreWorker
{
	uint32_t magic;

	GsCtrlCon *mCtrlCon;

	int(*cb_crank_t)(
		struct GsWorkerData *WorkerDataRecv,
		struct GsWorkerData *WorkerDataSend,
		struct GsStoreWorker *StoreWorker,
		struct GsExtraWorker *ExtraWorker);
};

/**

    WARNING: custom / special destruction protocol
	- first: wait until mCtrlCon is signalled
	           (the launched threads must signal before exiting)
	- second: destroy the GsFullConnection
	Signaling mCtrlCon before exiting from a launched thread races
	against waiting on mCtrlCon and destroying GsFullConnection.
	This is a problem because a thread is required to be
	deached or joined before destruction.
	The chosen resolution is ensure the threads are detached before
	being destroyed.
	This is accomplished containing threads in a shared pointer
	with a custom thread-detaching deleter.

    @sa
	   ::gs_ctrl_con_wait_exited
       ::gs_sp_thread_detaching_deleter
	   ::std::thread::detach
*/
struct GsFullConnection
{
	sp<std::thread> ThreadNtwk;
	sp<std::thread> ThreadWorker;
	sp<GsExtraHostCreate> ThreadNtwkExtraHostCreate;
	sp<GsCtrlCon> mCtrlCon;
};

int gs_connection_surrogate_map_create(GsConnectionSurrogateMap **oConnectionSurrogateMap);
int gs_connection_surrogate_map_clear(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_connection_surrogate_map_insert_id(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	const struct GsConnectionSurrogate valConnectionSurrogate);
int gs_connection_surrogate_map_insert(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	const GsConnectionSurrogate valConnectionSurrogate,
	gs_connection_surrogate_id_t *oConnectionSurrogateId);
int gs_connection_surrogate_map_get_try(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	struct GsConnectionSurrogate *oConnectionSurrogate,
	uint32_t *oIsPresent);
int gs_connection_surrogate_map_get(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	struct GsConnectionSurrogate *oConnectionSurrogate);
int gs_connection_surrogate_map_erase(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId);

int clnt_state_reconnect_make_default(ClntStateReconnect *oStateReconnect);
bool clnt_state_reconnect_have_remaining(ClntStateReconnect *StateReconnect);
int clnt_state_reconnect_expend(ClntStateReconnect *ioStateReconnect);

int gs_connection_surrogate_packet_send(
	GsConnectionSurrogate *ConnectionSurrogate,
	GsPacket *ioPacket);
int gs_packet_surrogate_release_ownership(struct GsPacketSurrogate *ioPacketSurrogate);
int gs_packet_create(
	struct GsPacket **oPacket,
	struct GsPacketSurrogate *valPacketSurrogate);

int gs_worker_data_create(struct GsWorkerData **oWorkerData);

int gs_ctrl_con_create(struct GsCtrlCon **oCtrlCon, uint32_t ExitedSignalLeft);
int gs_ctrl_con_signal_exited(struct GsCtrlCon *CtrlCon);
int gs_ctrl_con_wait_exited(struct GsCtrlCon *CtrlCon);

int gs_worker_request_data_type_packet_make(
	struct GsPacket *Packet,
	gs_connection_surrogate_id_t Id,
	struct GsWorkerRequestData *outValWorkerRequest);
int gs_worker_request_data_type_reconnect_prepare_make(
	struct GsWorkerRequestData *outValWorkerRequest);
int gs_worker_request_data_type_reconnect_reconnect_make(
	struct GsExtraWorker *ExtraWorker,
	struct GsWorkerRequestData *outValWorkerRequest);
int gs_worker_request_data_type_exit_make(
	struct GsWorkerRequestData *outValWorkerRequest);
int gs_worker_request_data_type_disconnect_make(
	gs_connection_surrogate_id_t Id,
	struct GsWorkerRequestData *outValWorkerRequest);
bool gs_worker_request_isempty(struct GsWorkerData *pThis);
int gs_worker_request_enqueue(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *valRequestData);
int gs_worker_request_enqueue_double_notify(
	struct GsWorkerData *pThis,
	struct GsExtraWorker *ExtraWorker);
int gs_worker_request_dequeue_timeout(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs);
int gs_worker_request_dequeue(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData);
int gs_worker_request_dequeue_all_opt_cpp(
	struct GsWorkerData *pThis,
	std::deque<struct GsWorkerRequestData> *oValRequestData);

int gs_worker_packet_enqueue(
	GsWorkerData *pThis,
	GsIntrTokenSurrogate *IntrToken,
	gs_connection_surrogate_id_t Id,
	const char *Data, uint32_t DataSize);
int gs_worker_packet_dequeue(
	GsWorkerData *pThis,
	GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId);

int gs_worker_packet_dequeue_timeout_reconnects(
	GsWorkerData *pThis,
	GsWorkerData *WorkerDataSend,
	uint32_t TimeoutMs,
	GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId);
int gs_ntwk_reconnect_expend(
	GsExtraHostCreate *ExtraHostCreate,
	GsWorkerData *WorkerDataRecv,
	ClntStateReconnect *ioStateReconnect,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsHostSurrogate *ioHostSurrogate,
	uint32_t *ioWantReconnect);
int gs_aux_aux_aux_connection_register_transfer_ownership(
	GsConnectionSurrogate valConnectionSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t *oAssignedId);
int gs_aux_aux_aux_cb_last_chance_t(
	struct ENetIntr *Intr,
	struct ENetIntrToken *IntrToken);
int gs_ntwk_host_service_worker_disconnect(
	struct GsHostSurrogate *ReferenceHostSurrogate,
	struct GsWorkerRequestData *RequestSend,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_ntwk_host_service_worker_packet(
	struct GsHostSurrogate *ReferenceHostSurrogate,
	struct GsWorkerRequestData *RequestSend,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_ntwk_host_service_sends(
	struct GsWorkerData *WorkerDataSend,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_ntwk_host_service_event(
	struct GsWorkerData *WorkerDataRecv,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	int errService,
	GsEventSurrogate *Event);
int gs_ntwk_host_service(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk  *StoreNtwk,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_ntwk_reconnecter(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreNtwk> StoreNtwk,
	sp<GsExtraHostCreate> ExtraHostCreate);
void gs_ntwk_thread_func(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreNtwk> StoreNtwk,
	sp<GsExtraHostCreate> ExtraHostCreate,
	const char *optExtraThreadName);

int gs_worker_exit(
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker);
int gs_worker_dequeue_handling_double_notify(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerRequestData *outValRequest);
int gs_worker_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsExtraWorker **oExtraWorker);
int gs_worker_reconnecter(
	GsWorkerData *WorkerDataRecv,
	GsWorkerData *WorkerDataSend,
	GsStoreWorker *StoreWorker);
void gs_worker_thread_func(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreWorker> StoreWorker,
	const char *optExtraThreadName);

int gs_net_full_create_connection(
	uint32_t ServPort,
	sp<GsExtraHostCreate> pExtraHostCreate,
	sp<GsStoreNtwk>       pStoreNtwk,
	sp<GsStoreWorker>     pStoreWorker,
	sp<GsFullConnection> *oConnection,
	const char *optExtraThreadName);

#endif /* _GITTEST_NET2_H_ */
