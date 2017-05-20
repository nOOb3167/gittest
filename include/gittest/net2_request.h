#ifndef _NET2_REQUEST_H_
#define _NET2_REQUEST_H_

#include <mutex>
#include <condition_variable>
#include <deque>

#include <gittest/misc.h>
#include <gittest/net2_fwd.h>

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
	   ::gs_worker_data_destroy
	   ::gs_worker_request_isempty
       ::gs_worker_request_enqueue
	   ::gs_worker_request_enqueue_double_notify
	   ::gs_worker_request_dequeue
	   ::gs_worker_packet_enqueue
	   ::gs_worker_packet_dequeue
	   ::gs_worker_packet_dequeue_timeout_reconnects
*/
struct GsWorkerData
{
	sp<std::deque<GsWorkerRequestData> > mWorkerQueue;
	sp<std::mutex> mWorkerDataMutex;
	sp<std::condition_variable> mWorkerDataCond;
};

/** value struct
    @sa:
        ::gs_worker_data_vec_create
		::gs_worker_data_vec_destroy
		::gs_worker_data_vec_id
*/
struct GsWorkerDataVec
{
	uint32_t mLen;
	struct GsWorkerData **mData;
};

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
bool gs_worker_request_isempty_nolock(
	struct GsWorkerData *pThis,
	std::unique_lock<std::mutex> *Lock);

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
int gs_worker_request_dequeue_timeout_nolock(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs,
	std::unique_lock<std::mutex> *Lock);
int gs_worker_request_dequeue_timeout_noprepare(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs);
int gs_worker_request_dequeue_timeout_noprepare_nolock(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData,
	uint32_t TimeoutMs,
	std::unique_lock<std::mutex> *Lock);
int gs_worker_request_dequeue(
	struct GsWorkerData *pThis,
	struct GsWorkerRequestData *oValRequestData);
int gs_worker_request_dequeue_all_opt_cpp(
	struct GsWorkerData *pThis,
	std::deque<struct GsWorkerRequestData> *oValRequestData);
int gs_worker_request_dequeue_discard_until_reconnect(
	struct GsWorkerData *pThis);
int gs_worker_request_dequeue_steal_except_nolock(
	struct GsAffinityQueue *AffinityQueue,
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t DstWorkerId,
	gs_worker_id_t SrcWorkerId,
	gs_connection_surrogate_id_t ExceptId,
	std::unique_lock<std::mutex> *LockQueue,
	std::unique_lock<std::mutex> LockWorker[2]);

int gs_worker_packet_enqueue(
	struct GsWorkerData *pThis,
	struct GsIntrTokenSurrogate *IntrToken,
	gs_connection_surrogate_id_t Id,
	const char *Data, uint32_t DataSize);
int gs_worker_packet_dequeue_(
	struct GsWorkerData *pThis,
	struct GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId);
int gs_worker_packet_dequeue_timeout_reconnects(
	struct GsWorkerDataVec *WorkerDataVec,
	struct GsWorkerData *WorkerDataSend,
	gs_worker_id_t WorkerId,
	uint32_t TimeoutMs,
	struct GsAffinityQueue *AffinityQueue,
	struct GsAffinityToken *ioAffinityToken,
	struct GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId,
	struct GsExtraWorker **ioExtraWorkerCond);
int gs_worker_packet_dequeue_timeout_reconnects2(
	struct GsCrankData *CrankData,
	uint32_t TimeoutMs,
	struct GsAffinityToken *ioAffinityToken,
	struct GsPacket **oPacket,
	gs_connection_surrogate_id_t *oId);

int gs_worker_data_create(struct GsWorkerData **oWorkerData);
int gs_worker_data_destroy(struct GsWorkerData *WorkerData);

int gs_worker_data_vec_create(
	uint32_t NumWorkers,
	struct GsWorkerDataVec **oWorkerDataVec);
int gs_worker_data_vec_destroy(
	struct GsWorkerDataVec *WorkerDataVec);
struct GsWorkerData * gs_worker_data_vec_id(
	struct GsWorkerDataVec *WorkerDataVec,
	gs_worker_id_t WorkerId);

#endif /* _NET2_REQUEST_H_ */
