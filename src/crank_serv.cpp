#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h>
#include <gittest/frame.h>
#include <gittest/net2.h>

#include <gittest/crank_serv.h>

int gs_extra_host_create_server_create(
	uint32_t ServPort,
	struct GsExtraHostCreateServer **oExtraHostCreate)
{
	int r = 0;

	struct GsExtraHostCreateServer *ExtraHostCreate = new GsExtraHostCreateServer();

	ExtraHostCreate->base.magic = GS_EXTRA_HOST_CREATE_SERVER_MAGIC;
	ExtraHostCreate->base.cb_create_batch_t = gs_extra_host_create_cb_create_t_server;
	ExtraHostCreate->base.cb_destroy_host_t = gs_extra_host_create_cb_destroy_host_t_enet_host_destroy;
	ExtraHostCreate->base.cb_destroy_t = gs_extra_host_create_cb_destroy_t_delete;

	ExtraHostCreate->mServPort = ServPort;

	if (oExtraHostCreate)
		*oExtraHostCreate = ExtraHostCreate;

clean:
	if (!!r) {
		GS_DELETE(&ExtraHostCreate, GsExtraHostCreateServer);
	}

	return r;
}

int gs_extra_host_create_cb_create_t_server(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	size_t LenExtraWorker,
	struct GsExtraWorker **oExtraWorkerArr)
{
	int r = 0;

	struct GsExtraHostCreateServer *pThis = (struct GsExtraHostCreateServer *) ExtraHostCreate;

	struct GsHostSurrogate Host = {};

	if (pThis->base.magic != GS_EXTRA_HOST_CREATE_SERVER_MAGIC)
		GS_ERR_CLEAN(1);

	/* create host */

	// FIXME: 128 peerCount, 1 channelLimit
	if (!!(r = gs_host_surrogate_setup_host_bind_port(pThis->mServPort, 128, &Host)))
		GS_GOTO_CLEAN();

	/* output */

	for (uint32_t i = 0; i < LenExtraWorker; i++)
		if (!!(r = gs_extra_worker_server_create(oExtraWorkerArr + i)))
			GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		*ioHostSurrogate = Host;

clean:

	return r;
}

int gs_extra_worker_server_create(
	struct GsExtraWorker **oExtraWorker)
{
	struct GsExtraWorkerServer * pThis = new GsExtraWorkerServer();

	pThis->base.magic = GS_EXTRA_WORKER_SERVER_MAGIC;

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

int gs_store_ntwk_server_create(
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreNtwkServer **oStoreNtwk)
{
	int r = 0;

	struct GsStoreNtwkServer *StoreNtwk = new GsStoreNtwkServer();

	if (!!(r = gs_store_ntwk_init(
		GS_STORE_NTWK_SERVER_MAGIC,
		gs_store_ntwk_cb_destroy_t_server,
		ConnectionCommon,
		&StoreNtwk->base)))
	{
		GS_GOTO_CLEAN();
	}

	if (oStoreNtwk)
		*oStoreNtwk = StoreNtwk;

clean:
	if (!!r) {
		GS_DELETE(&StoreNtwk, GsStoreNtwkServer);
	}

	return r;
}

int gs_store_ntwk_cb_destroy_t_server(struct GsStoreNtwk *StoreNtwk)
{
	struct GsStoreNtwkServer *pThis = (struct GsStoreNtwkServer *) StoreNtwk;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_STORE_NTWK_SERVER_MAGIC);

	GS_DELETE_F(&pThis->base.mConnectionSurrogateMap, gs_connection_surrogate_map_destroy);

	GS_DELETE(&StoreNtwk, GsStoreNtwk);

	return 0;
}

int gs_store_worker_server_create(
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreWorkerServer **oStoreWorker)
{
	int r = 0;

	struct GsStoreWorkerServer *StoreWorker = new GsStoreWorkerServer();

	uint32_t NumWorkers = 0;

	if (!!(r = gs_ctrl_con_get_num_workers(ConnectionCommon->mCtrlCon, &NumWorkers)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_worker_init(
		GS_STORE_WORKER_SERVER_MAGIC,
		gs_store_worker_cb_crank_t_server,
		gs_store_worker_cb_destroy_t_server,
		NumWorkers,
		ConnectionCommon,
		&StoreWorker->base)))
	{
		GS_GOTO_CLEAN();
	}

	StoreWorker->mRefNameMainBuf = RefNameMainBuf;
	StoreWorker->mLenRefNameMain = LenRefNameMain;
	StoreWorker->mRefNameSelfUpdateBuf = RefNameSelfUpdateBuf;
	StoreWorker->mLenRefNameSelfUpdate = LenRefNameSelfUpdate;
	StoreWorker->mRepoMainPathBuf = RepoMainPathBuf;
	StoreWorker->mLenRepoMainPath = LenRepoMainPath;
	StoreWorker->mRepoSelfUpdatePathBuf = RepoSelfUpdatePathBuf;
	StoreWorker->mLenRepoSelfUpdatePath = LenRepoSelfUpdatePath;

	if (oStoreWorker)
		*oStoreWorker = StoreWorker;

clean:
	if (!!r) {
		GS_DELETE(&StoreWorker, GsStoreWorkerServer);
	}

	return r;
}

int gs_store_worker_server_aux_state_service_request_blobs2(
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

int gs_store_worker_cb_crank_t_server(struct GsCrankData *CrankData)
{
	int r = 0;

	struct GsStoreWorkerServer *pStoreWorker = (struct GsStoreWorkerServer *) CrankData->mStoreWorker;

	git_repository *Repository = NULL;
	git_repository *RepositorySelfUpdate = NULL;

	struct GsAffinityToken AffinityToken = {};

	if (pStoreWorker->base.magic != GS_STORE_WORKER_SERVER_MAGIC)
		GS_ERR_CLEAN(1);

	if (!!(r = aux_repository_open(pStoreWorker->mRepoMainPathBuf, &Repository)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(pStoreWorker->mRepoSelfUpdatePathBuf, &RepositorySelfUpdate)))
		GS_GOTO_CLEAN();

	while (true) {
		struct GsPacket *Packet = NULL;
		gs_connection_surrogate_id_t IdForSend = 0;

		GS_LOG(I, S, "waiting for request");

		// FIXME: need some kind of infinite timeout mechanism or alt func
		if (!!(r = gs_worker_packet_dequeue_timeout_reconnects2(
			CrankData,
			GS_SERV_AUX_VERYHIGH_TIMEOUT_U32_MS,
			&AffinityToken,
			&Packet,
			&IdForSend)))
		{
			GS_GOTO_CLEAN();
		}

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

			if (!!(r = serv_latest_commit_tree_oid(Repository, pStoreWorker->mRefNameMainBuf, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			GS_OID_STR_MAKE(TreeHeadOid);
			GS_LOG(I, PF, "latest commit tree [%s]", TreeHeadOidStr);

			if (!!(r = aux_frame_full_write_response_latest_commit_tree(TreeHeadOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_packet_enqueue(CrankData->mWorkerDataSend, &CrankData->mStoreWorker->mIntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
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

			if (!!(r = gs_worker_packet_enqueue(CrankData->mWorkerDataSend, &CrankData->mStoreWorker->mIntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
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

			if (!!(r = gs_worker_packet_enqueue(CrankData->mWorkerDataSend, &CrankData->mStoreWorker->mIntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
				GS_GOTO_CLEAN();
		}
		break;

		case GS_FRAME_TYPE_REQUEST_BLOBS:
		{
			if (!!(r = gs_store_worker_server_aux_state_service_request_blobs2(
				CrankData->mWorkerDataSend,
				IdForSend,
				&CrankData->mStoreWorker->mIntrToken,
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
			if (!!(r = gs_store_worker_server_aux_state_service_request_blobs2(
				CrankData->mWorkerDataSend,
				IdForSend,
				&CrankData->mStoreWorker->mIntrToken,
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

			if (!!(r = serv_latest_commit_tree_oid(RepositorySelfUpdate, pStoreWorker->mRefNameSelfUpdateBuf, &CommitHeadOid, &TreeHeadOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_oid_tree_blob_byname(RepositorySelfUpdate, &TreeHeadOid, GS_STR_PARENT_EXPECTED_SUFFIX, &BlobSelfUpdateOid)))
				GS_GOTO_CLEAN();

			if (!!(r = aux_frame_full_write_response_latest_selfupdate_blob(BlobSelfUpdateOid.id, GIT_OID_RAWSZ, gs_bysize_cb_String, &BysizeResponseBuffer)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_worker_packet_enqueue(CrankData->mWorkerDataSend, &CrankData->mStoreWorker->mIntrToken, IdForSend, ResponseBuffer.data(), ResponseBuffer.size())))
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
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	if (RepositorySelfUpdate)
		git_repository_free(RepositorySelfUpdate);

	if (Repository)
		git_repository_free(Repository);

	return r;
}

int gs_store_worker_cb_destroy_t_server(struct GsStoreWorker *StoreWorker)
{
	struct GsStoreWorkerServer *pThis = (struct GsStoreWorkerServer *) StoreWorker;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_STORE_WORKER_SERVER_MAGIC);

	GS_DELETE(&StoreWorker, GsStoreWorker);

	return 0;
}

int gs_net_full_create_connection_server(
	uint32_t ServPort,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	struct GsFullConnection **oConnectionServer)
{
	int r = 0;

	struct GsFullConnection *ConnectionServer = NULL;
	struct GsFullConnectionCommonData *ConnectionCommon = NULL;

	struct GsExtraHostCreateServer *ExtraHostCreate = NULL;
	struct GsStoreNtwkServer       *StoreNtwk       = NULL;
	struct GsStoreWorkerServer     *StoreWorker     = NULL;

	if (!!(r = gs_full_connection_common_data_create(GS_MAGIC_NUM_WORKER_THREADS, &ConnectionCommon)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_extra_host_create_server_create(ServPort, &ExtraHostCreate)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_ntwk_server_create(ConnectionCommon, &StoreNtwk)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_worker_server_create(
		RefNameMainBuf, LenRefNameMain,
		RefNameSelfUpdateBuf, LenRefNameSelfUpdate,
		RepoMainPathBuf, LenRepoMainPath,
		RepoSelfUpdatePathBuf, LenRepoSelfUpdatePath,
		ConnectionCommon,
		&StoreWorker)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_net_full_create_connection(
		ServPort,
		GS_BASE_ARGOWN(&ExtraHostCreate),
		GS_BASE_ARGOWN(&StoreNtwk),
		GS_BASE_ARGOWN(&StoreWorker),
		GS_ARGOWN(&ConnectionCommon),
		&ConnectionServer,
		"serv")))
	{
		GS_GOTO_CLEAN();
	}

	if (oConnectionServer)
		*oConnectionServer = ConnectionServer;

clean:
	if (!!r) {
		GS_DELETE_F(&ConnectionServer, gs_full_connection_destroy);
		GS_DELETE_BASE_VF(&StoreWorker, cb_destroy_t);
		GS_DELETE_BASE_VF(&StoreNtwk, cb_destroy_t);
		GS_DELETE_BASE_VF(&ExtraHostCreate, cb_destroy_t);
		GS_DELETE_F(&ConnectionCommon, gs_full_connection_common_data_destroy);
	}

	return r;
}
