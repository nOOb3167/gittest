#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h>
#include <gittest/frame.h>
#include <gittest/net2.h>

#include <gittest/crank_serv.h>

int serv_state_service_request_blobs2(
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	GsPacket *Packet,
	uint32_t OffsetSize,
	git_repository *Repository,
	const GsFrameType &FrameTypeResponse)
{
	int r = 0;

	std::string ResponseBuffer;
	uint32_t Offset = OffsetSize;
	uint32_t LengthLimit = 0;
	std::vector<git_oid> BloblistRequested;
	std::string SizeBufferBlob;
	std::string ObjectBufferBlob;

	GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
	GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

	GS_BYPART_DATA_VAR(OidVector, BypartBloblistRequested);
	GS_BYPART_DATA_INIT(OidVector, BypartBloblistRequested, &BloblistRequested);

	if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartBloblistRequested, gs_bypart_cb_OidVector)))
		GS_GOTO_CLEAN();

	if (!!(r = serv_serialize_blobs(Repository, &BloblistRequested, &SizeBufferBlob, &ObjectBufferBlob)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_response_blobs(
		FrameTypeResponse, BloblistRequested.size(),
		(uint8_t *)SizeBufferBlob.data(), SizeBufferBlob.size(),
		(uint8_t *)ObjectBufferBlob.data(), ObjectBufferBlob.size(),
		gs_bysize_cb_String, &BysizeResponseBuffer)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int serv_state_crank2(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsIntrTokenSurrogate *IntrToken,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath)
{
		int r = 0;

	git_repository *Repository = NULL;
	git_repository *RepositorySelfUpdate = NULL;

	if (!!(r = aux_repository_open(RepoMainPathBuf, &Repository)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoSelfUpdatePathBuf, &RepositorySelfUpdate)))
		GS_GOTO_CLEAN();

	while (true) {
		GsPacket *Packet = NULL;
		gs_connection_surrogate_id_t IdForSend = 0;

		GS_LOG(I, S, "waiting for request");

		if (!!(r = gs_worker_packet_dequeue(WorkerDataRecv, &Packet, &IdForSend)))
			GS_GOTO_CLEAN();

		uint32_t OffsetStart = 0;
		uint32_t OffsetSize = 0;

		GsFrameType FoundFrameType = {};

		if (!!(r = aux_frame_read_frametype(Packet->data, Packet->dataLength, OffsetStart, &OffsetSize, &FoundFrameType)))
			GS_GOTO_CLEAN();

		GS_LOG(I, PF, "servicing request [%.*s]", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);

		switch (FoundFrameType.mTypeNum)
		{
		case GS_FRAME_TYPE_REQUEST_LATEST_COMMIT_TREE:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid CommitHeadOid = {};
			git_oid TreeHeadOid = {};
			GS_OID_STR_VAR(TreeHeadOid);

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(Repository, RefNameMainBuf, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			GS_OID_STR_MAKE(TreeHeadOid);
			GS_LOG(I, PF, "latest commit tree [%s]", TreeHeadOidStr);

			if (!!(r = aux_frame_full_write_response_latest_commit_tree(TreeHeadOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		case GS_FRAME_TYPE_REQUEST_TREELIST:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid TreeOid = {};
			std::vector<git_oid> Treelist;
			GsStrided TreelistStrided = {};

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid(Packet->data, Packet->dataLength, Offset, &Offset, TreeOid.id, GIT_OID_RAWSZ)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_oid_treelist(Repository, &TreeOid, &Treelist)))
				GS_GOTO_CLEAN();

			GS_LOG(I, PF, "listing trees [num=%d]", (int)Treelist.size());

			if (!!(r = gs_strided_for_oid_vec_cpp(&Treelist, &TreelistStrided)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_treelist(TreelistStrided, gs_bysize_cb_String, &BysizeResponseBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		case GS_FRAME_TYPE_REQUEST_TREES:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			uint32_t LengthLimit = 0;
			std::vector<git_oid> TreelistRequested;
			std::string SizeBufferTree;
			std::string ObjectBufferTree;

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			GS_BYPART_DATA_VAR(OidVector, BypartTreelistRequested);
			GS_BYPART_DATA_INIT(OidVector, BypartTreelistRequested, &TreelistRequested);

			if (!!(r = aux_frame_read_size_limit(Packet->data, Packet->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &LengthLimit)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_read_oid_vec(Packet->data, LengthLimit, Offset, &Offset, &BypartTreelistRequested, gs_bypart_cb_OidVector)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_serialize_trees(Repository, &TreelistRequested, &SizeBufferTree, &ObjectBufferTree)))
				GS_GOTO_CLEAN();

			GS_LOG(I, PF, "serializing trees [num=%d]", (int)TreelistRequested.size());

			if (!!(r = aux_frame_full_write_response_trees(
				TreelistRequested.size(),
				(uint8_t *)SizeBufferTree.data(), SizeBufferTree.size(),
				(uint8_t *)ObjectBufferTree.data(), ObjectBufferTree.size(),
				gs_bysize_cb_String, &BysizeResponseBuffer)))
			{
				GS_GOTO_CLEAN();
			}

			if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		case GS_FRAME_TYPE_REQUEST_BLOBS:
		{
			if (!!(r = serv_state_service_request_blobs2(
				WorkerDataSend,
				IdForSend,
				IntrToken,
				Packet,
				OffsetSize,
				Repository,
				GS_FRAME_TYPE_DECL(RESPONSE_BLOBS))))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_BLOBS_SELFUPDATE:
		{
			if (!!(r = serv_state_service_request_blobs2(
				WorkerDataSend,
				IdForSend,
				IntrToken,
				Packet,
				OffsetSize,
				RepositorySelfUpdate,
				GS_FRAME_TYPE_DECL(RESPONSE_BLOBS_SELFUPDATE))))
			{
				GS_GOTO_CLEAN();
			}
		}
		break;

		case GS_FRAME_TYPE_REQUEST_LATEST_SELFUPDATE_BLOB:
		{
			std::string ResponseBuffer;
			uint32_t Offset = OffsetSize;
			git_oid CommitHeadOid = {};
			git_oid TreeHeadOid = {};
			git_oid BlobSelfUpdateOid = {};

			GS_BYPART_DATA_VAR(String, BysizeResponseBuffer);
			GS_BYPART_DATA_INIT(String, BysizeResponseBuffer, &ResponseBuffer);

			if (!!(r = aux_frame_read_size_ensure(Packet->data, Packet->dataLength, Offset, &Offset, 0)))
				GS_GOTO_CLEAN();

			if (!!(r = serv_latest_commit_tree_oid(RepositorySelfUpdate, RefNameSelfUpdateBuf, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_oid_tree_blob_byname(RepositorySelfUpdate, &TreeHeadOid, GS_STR_PARENT_EXPECTED_SUFFIX, &BlobSelfUpdateOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_latest_selfupdate_blob(BlobSelfUpdateOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_packet_enqueue(WorkerDataSend, IntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		default:
		{
			GS_LOG(E, PF, "[worker] unknown frametype received [%.*s]", (int)GS_FRAME_HEADER_STR_LEN, FoundFrameType.mTypeName);
			if (1)
				GS_ERR_CLEAN(1);
		}
		break;
		}

	}

clean:
	if (RepositorySelfUpdate)
		git_repository_free(RepositorySelfUpdate);

	if (Repository)
		git_repository_free(Repository);

	return r;
}

int gs_net_full_create_connection_server(
	uint32_t ServPort,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	sp<GsFullConnection> *oConnectionServer)
{
	int r = 0;

	sp<GsFullConnection> ConnectionServer;

	ENetIntrTokenCreateFlags *IntrTokenFlags = NULL;
	GsIntrTokenSurrogate      IntrTokenSurrogate = {};

	GsExtraHostCreateServer *ExtraHostCreate = new GsExtraHostCreateServer();
	GsStoreNtwkServer       *StoreNtwk       = new GsStoreNtwkServer();
	GsStoreWorkerServer     *StoreWorker     = new GsStoreWorkerServer();

	sp<GsExtraHostCreate> pExtraHostCreate(&ExtraHostCreate->base);
	sp<GsStoreNtwk>       pStoreNtwk(&StoreNtwk->base);
	sp<GsStoreWorker>     pStoreWorker(&StoreWorker->base);

	if (!(IntrTokenFlags = enet_intr_token_create_flags_create(ENET_INTR_DATA_TYPE_NONE)))
		GS_GOTO_CLEAN();

	if (!(IntrTokenSurrogate.mIntrToken = enet_intr_token_create(IntrTokenFlags)))
		GS_ERR_CLEAN(1);

	ExtraHostCreate->base.magic = GS_EXTRA_HOST_CREATE_SERVER_MAGIC;
	ExtraHostCreate->base.cb_create_t = gs_extra_host_create_cb_create_t_server;
	ExtraHostCreate->mServPort = ServPort;

	StoreNtwk->base.magic = GS_STORE_NTWK_SERVER_MAGIC;
	StoreNtwk->base.mIntrTokenSurrogate = IntrTokenSurrogate;
	StoreNtwk->base.mCtrlCon = NULL; // FIXME: implement

	StoreWorker->base.magic = GS_STORE_WORKER_SERVER_MAGIC;
	StoreWorker->base.cb_crank_t = gs_store_worker_cb_crank_t_server;
	StoreWorker->mRefNameMainBuf = RefNameMainBuf;
	StoreWorker->mLenRefNameMain = LenRefNameMain;
	StoreWorker->mRefNameSelfUpdateBuf = RefNameSelfUpdateBuf;
	StoreWorker->mLenRefNameSelfUpdate = LenRefNameSelfUpdate;
	StoreWorker->mRepoMainPathBuf = RepoMainPathBuf;
	StoreWorker->mLenRepoMainPath = LenRepoMainPath;
	StoreWorker->mRepoSelfUpdatePathBuf = RepoSelfUpdatePathBuf;
	StoreWorker->mLenRepoSelfUpdatePath = LenRepoSelfUpdatePath;
	StoreWorker->mIntrToken = IntrTokenSurrogate;

	if (!!(r = gs_net_full_create_connection(
		ServPort,
		pExtraHostCreate,
		pStoreNtwk,
		pStoreWorker,
		&ConnectionServer,
		"serv")))
	{
		GS_GOTO_CLEAN();
	}

	if (oConnectionServer)
		*oConnectionServer = ConnectionServer;

clean:

	return r;
}

int gs_store_worker_cb_crank_t_server(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker *ExtraWorker)
{
	int r = 0;

	GsStoreWorkerServer *pStoreWorker = (GsStoreWorkerServer *) StoreWorker;
	GsExtraWorkerServer *pExtraWorker = (GsExtraWorkerServer *) ExtraWorker;

	if (pStoreWorker->base.magic != GS_STORE_WORKER_SERVER_MAGIC)
		GS_ERR_CLEAN(1);

	if (pExtraWorker->base.magic != GS_EXTRA_WORKER_SERVER_MAGIC)
		GS_ERR_CLEAN(1);

	while (true) {

		if (!!(r = serv_state_crank2(
			WorkerDataRecv,
			WorkerDataSend,
			&pStoreWorker->mIntrToken,
			pStoreWorker->mRefNameMainBuf, pStoreWorker->mLenRefNameMain,
			pStoreWorker->mRefNameSelfUpdateBuf, pStoreWorker->mLenRefNameSelfUpdate,
			pStoreWorker->mRepoMainPathBuf, pStoreWorker->mLenRepoMainPath,
			pStoreWorker->mRepoSelfUpdatePathBuf, pStoreWorker->mLenRepoSelfUpdatePath)))
		{
			GS_GOTO_CLEAN();
		}

	}

clean:

	return r;
}

int gs_extra_host_create_cb_create_t_server(
	GsExtraHostCreate *ExtraHostCreate,
	GsHostSurrogate *ioHostSurrogate,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsExtraWorker **oExtraWorker)
{
	int r = 0;

	struct GsExtraHostCreateServer *pThis = (struct GsExtraHostCreateServer *) ExtraHostCreate;

	ENetAddress addr = {};
	ENetIntrHostCreateFlags FlagsHost = {};
	ENetHost *host = NULL;

	GsExtraWorker *ExtraWorker = NULL;

	int errService = 0;

	if (pThis->base.magic != GS_EXTRA_HOST_CREATE_SERVER_MAGIC)
		GS_ERR_CLEAN(1);

	/* create host */

	/* NOTE: ENET_HOST_ANY (0) binds to all interfaces but will also cause host->address to have 0 as host */
	addr.host = ENET_HOST_ANY;
	addr.port = pThis->mServPort;

	// FIXME: 128 peerCount, 1 channelLimit
	if (!(host = enet_host_create_interruptible(&addr, 128, 1, 0, 0, &FlagsHost)))
		GS_ERR_CLEAN(1);

	// FIXME: dummy zero Id
	if (!!(r = gs_extra_worker_cb_create_t_server(&ExtraWorker, 0)))
		GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		ioHostSurrogate->mHost = host;

	if (oExtraWorker)
		*oExtraWorker = ExtraWorker;

clean:

	return r;
}

/** @param Id ignored - refactor (remove struct specific params aka make function freestanding)
*/
int gs_extra_worker_cb_create_t_server(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id)
{
	struct GsExtraWorkerServer * pThis = new GsExtraWorkerServer();

	pThis->base.magic = GS_EXTRA_WORKER_SERVER_MAGIC;

	pThis->base.cb_create_t = gs_extra_worker_cb_create_t_server;
	pThis->base.cb_destroy_t = gs_extra_worker_cb_destroy_t_server;

	if (oExtraWorker)
		*oExtraWorker = &pThis->base;

	return 0;
}

int gs_extra_worker_cb_destroy_t_server(struct GsExtraWorker *ExtraWorker)
{
	if (ExtraWorker->magic != GS_EXTRA_WORKER_SERVER_MAGIC)
		return -1;

	delete ExtraWorker;

	return 0;
}
