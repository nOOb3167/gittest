#ifndef _GITTEST_NET2_H_
#define _GITTEST_NET2_H_

#include <stddef.h>
#include <stdint.h>
#include <enet/enet.h>

#define GS_EXTRA_HOST_CREATE_CLIENT_MAGIC 0x501C325E
#define GS_EXTRA_WORKER_CLIENT_MAGIC      0x501C325F
#define GS_STORE_NTWK_CLIENT_MAGIC        0x501C3260
#define GS_STORE_WORKER_CLIENT_MAGIC      0x501C3261

#define GS_EXTRA_HOST_CREATE_SERVER_MAGIC 0x502C325E
#define GS_EXTRA_WORKER_SERVER_MAGIC      0x502C325F
#define GS_STORE_NTWK_SERVER_MAGIC        0x502C3260
#define GS_STORE_WORKER_SERVER_MAGIC      0x502C3261

#define GS_TIMEOUT_1SEC 1000

struct GsConnectionSurrogate;

typedef uint64_t gs_connection_surrogate_id_t;
typedef ::std::map<gs_connection_surrogate_id_t, GsConnectionSurrogate> gs_connection_surrogate_map_t;

GS_BYPART_DATA_DECL(GsConnectionSurrogateId, gs_connection_surrogate_id_t m0Id;);
#define GS_BYPART_TRIPWIRE_GsConnectionSurrogateId 0x68347232
#define GS_BYPART_DATA_INIT_GsConnectionSurrogateId(VARNAME, ID) (VARNAME).m0Id = ID;

struct GsExtraWorker;
struct GsWorkerData;

struct GsConnectionSurrogateMap {
	std::atomic<uint64_t> mAtomicCount;
	sp<gs_connection_surrogate_map_t> mConnectionSurrogateMap;

	GsConnectionSurrogateMap();
};

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
};

/**
  Extra data fields set inside this structure depend on the type field:
  - GS_SERV_WORKER_REQUEST_DATA_TYPE_PACKET: mPacket, mId
  - GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_PREPARE: no data fields
  - GS_SERV_WORKER_REQUEST_DATA_TYPE_RECONNECT_RECONNECT: mExtraWorker

  @sa
     ::gs_worker_request_data_type_packet_make
	 ::gs_worker_request_data_type_reconnect_prepare_make
	 ::gs_worker_request_data_type_reconnect_reconnect_make
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
};

struct GsStoreWorker
{
	uint32_t magic;

	int(*cb_crank_t)(
		struct GsWorkerData *WorkerDataRecv,
		struct GsWorkerData *WorkerDataSend,
		struct GsStoreWorker *StoreWorker,
		struct GsExtraWorker *ExtraWorker);
};

struct GsFullConnection
{
	sp<std::thread> ThreadNtwk;
	sp<std::thread> ThreadWorker;
	sp<GsExtraHostCreate> ThreadNtwkExtraHostCreate;
};

int gs_connection_surrogate_packet_send(
	GsConnectionSurrogate *ConnectionSurrogate,
	GsPacket *ioPacket);
int gs_packet_surrogate_release_ownership(struct GsPacketSurrogate *ioPacketSurrogate);
int gs_packet_create(
	struct GsPacket **oPacket,
	struct GsPacketSurrogate *valPacketSurrogate);

int gs_worker_data_create(struct GsWorkerData **oWorkerData);

int gs_worker_request_data_type_packet_make(
	struct GsPacket *Packet,
	gs_connection_surrogate_id_t Id,
	struct GsWorkerRequestData *outValWorkerRequest);
int gs_worker_request_data_type_reconnect_prepare_make(
	struct GsWorkerRequestData *outValWorkerRequest);
int gs_worker_request_data_type_reconnect_reconnect_make(
	struct GsExtraWorker *ExtraWorker,
	struct GsWorkerRequestData *outValWorkerRequest);
bool gs_worker_request_isempty(struct GsWorkerData *pThis);
int gs_worker_request_enqueue(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *valRequestData);
int gs_worker_request_enqueue_double_notify(
	struct GsWorkerData *pThis,
	struct GsExtraWorker *ExtraWorker);
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

int gs_ntwk_reconnect_expend(
	GsExtraHostCreate *ExtraHostCreate,
	GsWorkerData *WorkerDataRecv,
	ClntStateReconnect *ioStateReconnect,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsHostSurrogate *ioHostSurrogate,
	uint32_t *ioWantReconnect);
int gs_ntwk_host_service_wrap_want_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreNtwk  *StoreNtwk,
	struct GsHostSurrogate *HostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	uint32_t *ioWantReconnect);
int gs_aux_aux_aux_connection_register_transfer_ownership(
	GsConnectionSurrogate valConnectionSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t *oAssignedId);
int gs_aux_aux_aux_cb_last_chance_t(
	struct ENetIntr *Intr,
	struct ENetIntrToken *IntrToken);
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

int gs_worker_dequeue_handling_double_notify(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerRequestData *outValRequest);
int gs_worker_reconnect_expend(
	struct GsWorkerData *WorkerDataRecv,
	struct GsExtraWorker **oExtraWorkerCond,
	struct ClntStateReconnect *ioStateReconnect,
	uint32_t *ioWantReconnect);
int gs_worker_service_wrap_want_reconnect(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker *ExtraWorker,
	uint32_t *ioWantReconnect);
int gs_worker_reconnecter(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreWorker> StoreWorker);
int gs_worker_thread_func(
	sp<GsWorkerData> WorkerDataRecv,
	sp<GsWorkerData> WorkerDataSend,
	sp<GsStoreWorker> StoreWorker,
	const char *optExtraThreadName);

int gs_net_full_create_connection(
	uint32_t ServPort,
	sp<GsExtraHostCreate> pExtraHostCreate,
	sp<GsStoreNtwk>       pStoreNtwk,
	sp<GsStoreWorker>     pStoreWorker,
	sp<GsFullConnection> *oConnection);

#endif /* _GITTEST_NET2_H_ */
