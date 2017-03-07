#ifndef _GITTEST_NET_H_
#define _GITTEST_NET_H_

#include <cstdint>

#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <vector>
#include <map>

#include <enet/enet.h>
#include <git2.h>

#include <gittest/misc.h>
#include <gittest/frame.h>

#define GS_PORT 3756

#define GS_SERV_AUX_ARBITRARY_TIMEOUT_MS 5000
#define GS_CONNECT_NUMRETRY   5
#define GS_CONNECT_TIMEOUT_MS 1000
#define GS_CONNECT_NUMRECONNECT 5
#define GS_RECEIVE_TIMEOUT_MS 500000

/* is this really neccessary? */
#define GS_CLNT_STATE_CODE_SET_ENSURE_NONUCF(PTR_VARNAME_CLNTSTATE, CODE, VARNAME_TMPSTATE, STATEMENTBLOCK) \
	{ ClntState VARNAME_TMPSTATE;                                                                       \
      if (!!clnt_state_cpy(& (VARNAME_TMPSTATE), (PTR_VARNAME_CLNTSTATE)))                              \
        GS_ERR_CLEAN(9998);                                                                             \
	  { STATEMENTBLOCK }                                                                                \
	  if (!!clnt_state_code_ensure(& (VARNAME_TMPSTATE), (CODE)))                                       \
	    GS_ERR_CLEAN(9999);                                                                             \
	  if (!!clnt_state_cpy((PTR_VARNAME_CLNTSTATE), & (VARNAME_TMPSTATE)))                              \
	    GS_ERR_CLEAN(9998); }

enum gs_clnt_state_code_t {
	GS_CLNT_STATE_CODE_NEED_REPOSITORY = 0,
	GS_CLNT_STATE_CODE_NEED_TREE_HEAD = 1,
	GS_CLNT_STATE_CODE_NEED_TREELIST = 2,
	GS_CLNT_STATE_CODE_NEED_BLOBLIST = 3,
	GS_CLNT_STATE_CODE_NEED_WRITTEN_BLOB_AND_TREE = 4,
	GS_CLNT_STATE_CODE_NEED_NOTHING = 5,
	GS_CLNT_STATE_CODE_MAX_ENUM = 0x7FFFFFFF,
};

/* GsBypartCbDataOidVector */
GS_BYPART_DATA_DECL(OidVector, std::vector<git_oid> *m0OidVec;);
#define GS_BYPART_TRIPWIRE_OidVector 0x23132358
#define GS_BYPART_DATA_INIT_OidVector(VARNAME, POIDVEC) (VARNAME).m0OidVec = POIDVEC;
int gs_bypart_cb_OidVector(void *ctx, const char *d, int64_t l);

class GsConnectionSurrogate;

struct gs_packet_unique_t_deleter {
	void operator()(ENetPacket **xpacket) const;
};

typedef ::std::shared_ptr<ENetPacket *> gs_packet_t;
typedef ::std::unique_ptr<ENetPacket *, gs_packet_unique_t_deleter> gs_packet_unique_t;
typedef std::pair<ENetHost *, ENetPeer *> gs_host_peer_pair_t;

typedef uint64_t gs_connection_surrogate_id_t;
typedef ::std::map<gs_connection_surrogate_id_t, sp<GsConnectionSurrogate> > gs_connection_surrogate_map_t;

GS_BYPART_DATA_DECL(GsConnectionSurrogateId, gs_connection_surrogate_id_t m0Id;);
#define GS_BYPART_TRIPWIRE_GsConnectionSurrogateId 0x68347232
#define GS_BYPART_DATA_INIT_GsConnectionSurrogateId(VARNAME, ID) (VARNAME).m0Id = ID;

class GsHostSurrogate {
public:
	GsHostSurrogate(ENetHost *host);

public:
	ENetHost *mHost;
};

class GsConnectionSurrogate {
public:
	GsConnectionSurrogate(ENetHost *host, ENetPeer *peer, uint32_t IsPrincipalClientConnection);

	void Invalidate();
	void IsPrincipalClientConnection();

private:
	std::atomic_uint32_t mIsValid;
	std::atomic_uint32_t mIsPrincipalClientConnection;

public:
	ENetHost *mHost;
	ENetPeer *mPeer;
};

struct GsConnectionSurrogateMap {
	std::atomic_uint64_t mAtomicCount;
	sp<gs_connection_surrogate_map_t> mConnectionSurrogateMap;
};

struct PacketWithOffset {
	gs_packet_t mPacket;
	uint32_t mOffsetSize;
	uint32_t mOffsetObject;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

struct PacketUniqueWithOffset {
	gs_packet_unique_t mPacket;
	uint32_t mOffsetSize;
	uint32_t mOffsetObject;

	/* choosing unique_ptr member variable was a bad idea as evidenced by 6(!) functions below */

	PacketUniqueWithOffset() {}
	~PacketUniqueWithOffset() {}

	PacketUniqueWithOffset(const PacketUniqueWithOffset &other) = delete;
	PacketUniqueWithOffset & operator=(const PacketUniqueWithOffset &other) = delete;

	PacketUniqueWithOffset(PacketUniqueWithOffset &&other);
	PacketUniqueWithOffset & operator=(PacketUniqueWithOffset &&other);
};

class ServWorkerRequestData {
public:
	ServWorkerRequestData(
		gs_packet_unique_t *ioPacket,
		uint32_t IsPrepare,
		uint32_t IsWithId,
		gs_connection_surrogate_id_t Id);

	bool isReconnectRequest();
	bool isReconnectRequestPrepare();
	bool isReconnectRequestRegular();
	bool isReconnectRequestRegularWithId();
	bool isReconnectRequestRegularNoId();

public:
	gs_packet_unique_t mPacket;

private:
	uint32_t mIsPrepare;
	uint32_t mIsWithId;
	gs_connection_surrogate_id_t mId;

private:
	friend int aux_make_serv_worker_request_data_for_response(
		ServWorkerRequestData *RequestBeingResponded, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oServWorkerRequestData);
	friend void aux_get_serv_worker_request_private(ServWorkerRequestData *Request, gs_connection_surrogate_id_t *oId);
};

class ServWorkerData {
public:
	ServWorkerData();

	void RequestEnqueue(const sp<ServWorkerRequestData> &RequestData);
	void RequestDequeue(sp<ServWorkerRequestData> *oRequestData);
	void RequestDequeueAllOpt(std::deque<sp<ServWorkerRequestData> > *oRequestData);

private:
	sp<std::deque<sp<ServWorkerRequestData> > > mWorkerQueue;
	sp<std::mutex> mWorkerDataMutex;
	sp<std::condition_variable> mWorkerDataCond;
};

class ServAuxRequestData {
public:
	ServAuxRequestData();
	ServAuxRequestData(
		uint32_t IsReconnect,
		uint32_t IsPrepare,
		ENetAddress *AddressIfReconnectOpt);

	bool IsReconnectRequest();
	bool isReconnectRequestPrepare();
	bool isReconnectRequestRegular();

public:
	ENetAddress mAddress;

private:
	uint32_t mIsReconnect;
	uint32_t mIsPrepare;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

class ServAuxData {
public:
	ServAuxData();

	void InterruptRequestedEnqueue();
	void InterruptRequestedEnqueueData(ServAuxRequestData RequestData);
	void InterruptRequestedDequeue(ServAuxRequestData *oRequestData);
	bool InterruptRequestedDequeueTimeout(
		const std::chrono::milliseconds &WaitForMillis,
		ServAuxRequestData *oRequestData);

private:

	ServAuxRequestData InterruptRequestedDequeueMT_();

private:
	sp<std::deque<ServAuxRequestData> > mAuxQueue;
	sp<std::mutex> mAuxDataMutex;
	sp<std::condition_variable> mAuxDataCond;
};

struct ClntStateReconnect {
	uint32_t NumReconnections;
	uint32_t NumReconnectionsLeft;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

struct ClntState {
	sp<git_repository *> mRepositoryT;

	sp<git_oid> mTreeHeadOid;

	sp<std::vector<git_oid> > mTreelist;
	sp<std::vector<git_oid> > mMissingTreelist;

	sp<std::vector<git_oid> >  mMissingBloblist;
	sp<PacketUniqueWithOffset> mTreePacketWithOffset;

	sp<std::vector<git_oid> > mWrittenBlob;
	sp<std::vector<git_oid> > mWrittenTree;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

class FullConnectionClient {
public:
	FullConnectionClient(const sp<std::thread> &ThreadWorker, const sp<std::thread> &ThreadAux, const sp<std::thread> &Thread);

private:
	sp<std::thread> ThreadWorker;
	sp<std::thread> ThreadAux;
	sp<std::thread> Thread;
};

int gs_connection_surrogate_map_clear(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int gs_connection_surrogate_map_insert_id(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	const sp<GsConnectionSurrogate> &ConnectionSurrogate);
int gs_connection_surrogate_map_insert(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	const sp<GsConnectionSurrogate> &ConnectionSurrogate,
	gs_connection_surrogate_id_t *oConnectionSurrogateId);
int gs_connection_surrogate_map_get_try(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	sp<GsConnectionSurrogate> *oConnectionSurrogate);
int gs_connection_surrogate_map_get(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId,
	sp<GsConnectionSurrogate> *oConnectionSurrogate);
int gs_connection_surrogate_map_erase(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	gs_connection_surrogate_id_t ConnectionSurrogateId);

gs_packet_t aux_gs_make_packet(ENetPacket *packet);
gs_packet_unique_t aux_gs_make_packet_unique(ENetPacket *packet);
gs_packet_unique_t gs_packet_unique_t_null();

int aux_host_peer_pair_reset(sp<gs_host_peer_pair_t> *ioConnection);

int aux_make_packet_with_offset(gs_packet_t Packet, uint32_t OffsetSize, uint32_t OffsetObject, PacketWithOffset *oPacketWithOffset);
int aux_make_packet_unique_with_offset(gs_packet_unique_t *ioPacket, uint32_t OffsetSize, uint32_t OffsetObject, PacketUniqueWithOffset *oPacketWithOffset);

int aux_make_serv_worker_request_data(gs_connection_surrogate_id_t Id, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oRequestWorker);
int aux_make_serv_worker_request_data_reconnect_prepare(
	sp<ServWorkerRequestData> *oRequestWorker);
int aux_make_serv_worker_request_data_reconnect_regular_no_id(
	sp<ServWorkerRequestData> *oRequestWorker);
int aux_make_serv_worker_request_data_reconnect_regular_with_id(
	gs_connection_surrogate_id_t Id,
	sp<ServWorkerRequestData> *oRequestWorker);
int aux_make_serv_worker_request_data_for_response(
	ServWorkerRequestData *RequestBeingResponded, gs_packet_unique_t *ioPacket, sp<ServWorkerRequestData> *oRequestWorker);
void aux_get_serv_worker_request_private(ServWorkerRequestData *Request, gs_connection_surrogate_id_t *oId);

int aux_serv_worker_thread_service_request_blobs(
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataSend, ServWorkerRequestData *Request,
	ENetPacket *Packet, uint32_t OffsetSize, git_repository *Repository, const GsFrameType &FrameTypeResponse);
int aux_worker_enqueue_reconnect_prepare(
	ServWorkerData *WorkerDataRecv);
int aux_worker_enqueue_reconnect_regular_no_id(
	ServWorkerData *WorkerDataRecv);
int aux_worker_enqueue_reconnect_regular_with_id(
	ServWorkerData *WorkerDataRecv,
	gs_connection_surrogate_id_t Id);
int aux_worker_enqueue_reconnect_double_notify_no_id(
	ServWorkerData *WorkerDataRecv);
int aux_worker_enqueue_reconnect_double_notify_with_id(
	ServWorkerData *WorkerDataRecv,
	gs_connection_surrogate_id_t Id);
int aux_worker_dequeue_handling_double_notify(
	ServWorkerData *WorkerDataRecv,
	sp<ServWorkerRequestData> *oRequest);
int aux_serv_worker_reconnect_expend_reconnect_discard_request_for_send(
	ServWorkerData *WorkerDataRecv,
	ClntStateReconnect *ioStateReconnect,
	uint32_t *ioWantReconnect);
int serv_worker_thread_func_reconnecter(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const char *RepoSelfUpdateOpenPathBuf, size_t LenRepoSelfUpdateOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend);
int serv_state_crank(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const char *RepoSelfUpdateOpenPathBuf, size_t LenRepoSelfUpdateOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend);
int serv_worker_thread_func(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const char *RepoSelfUpdateOpenPathBuf, size_t LenRepoSelfUpdateOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	uint32_t *oWantReconnect);
int aux_clnt_worker_reconnect_expend_reconnect_receive_request_for_send(
	ServWorkerData *WorkerDataRecv,
	ClntStateReconnect *ioStateReconnect,
	sp<ServWorkerRequestData> *oRequestForSend,
	uint32_t *ioWantReconnect);
int clnt_worker_thread_func_reconnecter(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend);
int clnt_worker_thread_func(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServWorkerRequestData> RequestForSend,
	sp<ClntState> State,
	uint32_t *oWantReconnect);

int aux_enet_host_create_serv(uint32_t EnetAddressPort, ENetHost **oServer);
int aux_enet_host_server_create_addr_extra_for_serv_aux(
	uint32_t ServPort,
	ENetHost **oHost,
	ENetAddress *oAddressForServAux);
int aux_enet_host_client_create_addr(ENetHost **oHost, ENetAddress *oAddressHost);
int aux_enet_host_connect_addr(ENetHost *host, ENetAddress *address, ENetPeer **oPeer);
int aux_enet_host_create_connect_addr(
	ENetAddress *address,
	ENetHost **oHost, ENetPeer **oPeer);

int aux_enet_address_create_ip(
	uint32_t EnetAddressPort, uint32_t EnetAddressHostNetworkByteOrder,
	ENetAddress *oAddress);
int aux_enet_address_create_hostname(
	uint32_t EnetAddressPort, const char *EnetHostName,
	ENetAddress *oAddress);

int aux_packet_bare_send(ENetHost *host, ENetPeer *peer, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags);
int aux_packet_full_send(ENetHost *host, ENetPeer *peer, ServAuxData *ServAuxData, const char *Data, uint32_t DataSize, uint32_t EnetPacketFlags);
int aux_packet_response_queue_interrupt_request_reliable(ServAuxData *ServAuxData, ServWorkerData *WorkerDataSend, ServWorkerRequestData *Request, const char *Data, uint32_t DataSize);
int aux_packet_request_dequeue(ServWorkerData *WorkerDataRecv, sp<ServWorkerRequestData> *oRequestForRecv);
int aux_packet_request_dequeue_packet(ServWorkerData *WorkerDataRecv, gs_packet_unique_t *oPacket);

int aux_host_service_one_type_receive(ENetHost *host, uint32_t TimeoutMs, gs_packet_t *oPacket);
int aux_host_service(ENetHost *host, uint32_t TimeoutMs, std::vector<ENetEvent> *oEvents);
int aux_host_connect_ensure_timeout(ENetHost *client, uint32_t TimeoutMs, uint32_t *oHasTimedOut);
int aux_host_connect(
	ENetAddress *address,
	uint32_t NumRetry, uint32_t RetryTimeoutMs,
	ENetHost **oClient, ENetPeer **oPeer);

int aux_selfupdate_basic(const char *HostName, const char *FileNameAbsoluteSelfUpdate, uint32_t *oHaveUpdate, std::string *oBufferUpdate);

int aux_serv_aux_wait_reconnect(ServAuxData *AuxData, ENetAddress *oAddress);
int aux_serv_aux_wait_reconnect_and_connect(ServAuxData *AuxData, sp<GsConnectionSurrogate> *oConnectionSurrogate);
int aux_serv_aux_reconnect_expend_cond_interrupt_perform(
	ServAuxData *AuxData,
	ClntStateReconnect *ioStateReconnect,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	uint32_t *ioWantReconnect);
int aux_serv_aux_thread_func_reconnecter(sp<ServAuxData> ServAuxData);
int aux_serv_aux_make_premade_frame_interrupt_requested(std::string *oBufferFrameInterruptRequested);
int aux_serv_aux_enqueue_reconnect_double_notify(ServAuxData *AuxData, ENetAddress *address);
int aux_serv_aux_dequeue_handling_double_notify(
	ServAuxData *AuxData,
	ServAuxRequestData *oRequest);
int aux_serv_aux_interrupt_perform(
	GsConnectionSurrogate *ConnectionSurrogate);
int aux_serv_aux_host_service_sub(ENetHost *client);
int aux_serv_aux_host_service(
	sp<ServAuxData> ServAuxData,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate);
int aux_serv_aux_thread_func(
	sp<ServAuxData> ServAuxData,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate);
int serv_aux_thread_func(
	sp<ServAuxData> ServAuxData,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	uint32_t *oWantReconnect);

int aux_serv_host_service(
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	const sp<ServWorkerData> &WorkerDataRecv,
	const sp<ServWorkerData> &WorkerDataSend,
	sp<GsHostSurrogate> *ioHostSurrogate);
int aux_serv_thread_func(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<GsHostSurrogate> *ioHostSurrogate,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap);
int aux_serv_serv_connect_immediately(
	uint32_t ServPort,
	sp<GsHostSurrogate> *ioHostSurrogate,
	ENetAddress *oAddressForServAux);
int aux_serv_serv_reconnect_expend_reconnect_cond_notify_serv_aux_notify_worker(
	ServAuxData *AuxData,
	ServWorkerData *WorkerDataRecv,
	uint32_t ServPort,
	ClntStateReconnect *ioStateReconnect,
	sp<GsHostSurrogate> *ioHostSurrogate,
	uint32_t *ioWantReconnect);
int serv_serv_thread_func_reconnecter(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	uint32_t ServPort);
int serv_serv_thread_func(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	sp<GsHostSurrogate> *ioHostSurrogate,
	uint32_t *oWantReconnect);
int aux_clnt_serv_connect_immediately(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	ENetAddress *oAddressClnt);
int aux_clnt_serv_reconnect_expend_reconnect_cond_insert_map_notify_serv_aux_notify_worker(
	ServAuxData *AuxData,
	ServWorkerData *WorkerDataRecv,
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	ClntStateReconnect *ioStateReconnect,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	uint32_t *ioWantReconnect);
int clnt_serv_thread_func_reconnecter(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName);
int clnt_serv_thread_func(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	sp<GsConnectionSurrogate> *ioConnectionSurrogate,
	uint32_t *oWantReconnect);

int clnt_state_reconnect_make_default(ClntStateReconnect *oStateReconnect);
bool clnt_state_reconnect_have_remaining(ClntStateReconnect *StateReconnect);
int clnt_state_reconnect_expend(ClntStateReconnect *ioStateReconnect);
int clnt_state_make_default(ClntState *oState);
int clnt_state_cpy(ClntState *dst, const ClntState *src);
int clnt_state_code(ClntState *State, uint32_t *oCode);
int clnt_state_code_ensure(ClntState *State, uint32_t WantedCode);
int clnt_state_connection_remake(const confmap_t &ClntKeyVal, sp<gs_host_peer_pair_t> *ioConnection);

int clnt_state_crank(
	const sp<ClntState> &State,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const sp<ServAuxData> &ServAuxData,
	ServWorkerData *WorkerDataRecv,
	ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend);
int clnt_state_crank_reconnecter(
	const sp<ClntState> &State, ClntStateReconnect *ioStateReconnect,
	const confmap_t &ClntKeyVal, const sp<ServAuxData> &ServAuxData,
	ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend);

int clnt_state_need_repository_noown(
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	git_repository **oRepositoryT);
int clnt_state_need_tree_head_noown(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	git_repository *RepositoryT,
	ServAuxData *ServAuxData,
	ServWorkerData *WorkerDataRecv,
	ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	git_oid *oTreeHeadOid);
int clnt_state_need_treelist_noown(
	git_repository *RepositoryT,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	git_oid *TreeHeadOid, std::vector<git_oid> *oTreelist, std::vector<git_oid> *oMissingTreelist);
int clnt_state_need_bloblist_noown(
	git_repository *RepositoryT,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	std::vector<git_oid> *MissingTreelist,
	std::vector<git_oid> *oMissingBloblist,
	gs_packet_unique_t *oPacketTree, uint32_t *oOffsetSizeBufferTree, uint32_t *oOffsetObjectBufferTree);
int clnt_state_need_written_blob_and_tree_noown(
	git_repository *RepositoryT,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend,
	std::vector<git_oid> *MissingTreelist, std::vector<git_oid> *MissingBloblist,
	const gs_packet_unique_t &GsPacketTree, uint32_t OffsetSizeBufferTree, uint32_t OffsetObjectBufferTree,
	std::vector<git_oid> *oWrittenBlob, std::vector<git_oid> *oWrittenTree);
int clnt_state_need_repository_setup(
	const sp<ClntState> &State,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const sp<ServAuxData> &ServAuxData);
int clnt_state_need_tree_head_setup(
	const sp<ClntState> &State,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	ServAuxData *ServAuxData,
	ServWorkerData *WorkerDataRecv,
	ServWorkerData *WorkerDataSend,
	ServWorkerRequestData *RequestForSend);
int clnt_state_need_treelist_setup(const sp<ClntState> &State,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend, ServWorkerRequestData *RequestForSend);
int clnt_state_need_bloblist_setup(const sp<ClntState> &State,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend, ServWorkerRequestData *RequestForSend);
int clnt_state_need_written_blob_and_tree_setup(const sp<ClntState> &State,
	ServAuxData *ServAuxData, ServWorkerData *WorkerDataRecv, ServWorkerData *WorkerDataSend, ServWorkerRequestData *RequestForSend);

void serv_worker_thread_func_f(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	const char *RepoSelfUpdateOpenPathBuf, size_t LenRepoSelfUpdateOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend);
void serv_serv_aux_thread_func_f(sp<ServAuxData> ServAuxData);
void serv_thread_func_f(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	uint32_t ServPort);
void clnt_worker_thread_func_f(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainOpenPathBuf, size_t LenRepoMainOpenPath,
	sp<ServAuxData> ServAuxData,
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend);
void clnt_serv_aux_thread_func_f(sp<ServAuxData> ServAuxData);
void clnt_thread_func_f(
	sp<ServWorkerData> WorkerDataRecv,
	sp<ServWorkerData> WorkerDataSend,
	sp<ServAuxData> AuxData,
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName);

int aux_full_create_connection_server(
	uint32_t ServPort,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	sp<FullConnectionClient> *oConnectionClient);
int aux_full_create_connection_client(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	sp<FullConnectionClient> *oConnectionClient);

int stuff2();

#endif /* _GITTEST_NET_H_ */
