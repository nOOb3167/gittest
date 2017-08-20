#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cstddef>
#include <cstdint>

#include <vector>
#include <string>

#include <git2.h>

#include <gittest/gittest.h>
#include <gittest/bypart_git.h>
#include <gittest/frame.h>
#include <gittest/net2.h>

#include <gittest/crank_selfupdate_basic.h>

int gs_extra_host_create_selfupdate_basic_create(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	struct GsExtraHostCreateSelfUpdateBasic **oExtraHostCreate)
{
	int r = 0;

	struct GsExtraHostCreateSelfUpdateBasic *ExtraHostCreate = new GsExtraHostCreateSelfUpdateBasic();

	if (!!(r = gs_extra_host_create_init(
		GS_EXTRA_HOST_CREATE_SELFUPDATE_BASIC_MAGIC,
		gs_extra_host_create_cb_create_t_selfupdate_basic,
		gs_extra_host_create_cb_destroy_host_t_enet_host_destroy,
		gs_extra_host_create_cb_destroy_t_delete,
		&ExtraHostCreate->base)))
	{
		GS_GOTO_CLEAN();
	}

	ExtraHostCreate->mServPort = ServPort;
	ExtraHostCreate->mServHostNameBuf = ServHostNameBuf;
	ExtraHostCreate->mLenServHostName = LenServHostName;

	if (oExtraHostCreate)
		*oExtraHostCreate = ExtraHostCreate;

clean:
	if (!!r) {
		GS_DELETE(&ExtraHostCreate, GsExtraHostCreateSelfUpdateBasic);
	}

	return r;
}

int gs_store_ntwk_selfupdate_basic_create(
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreNtwkSelfUpdateBasic **oStoreNtwk)
{
	int r = 0;

	struct GsStoreNtwkSelfUpdateBasic *StoreNtwk = new GsStoreNtwkSelfUpdateBasic();

	if (!!(r = gs_store_ntwk_init(
		GS_STORE_NTWK_SELFUPDATE_BASIC_MAGIC,
		gs_store_ntwk_cb_destroy_t_selfupdate_basic,
		ConnectionCommon,
		&StoreNtwk->base)))
	{
		GS_GOTO_CLEAN();
	}

	if (oStoreNtwk)
		*oStoreNtwk = StoreNtwk;

clean:
	if (!!r) {
		GS_DELETE(&StoreNtwk, GsStoreNtwkSelfUpdateBasic);
	}

	return r;
}

int gs_store_ntwk_cb_destroy_t_selfupdate_basic(struct GsStoreNtwk *StoreNtwk)
{
	struct GsStoreNtwkSelfUpdateBasic *pThis = (struct GsStoreNtwkSelfUpdateBasic *) StoreNtwk;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_STORE_NTWK_SELFUPDATE_BASIC_MAGIC);

	GS_DELETE_F(&pThis->base.mConnectionSurrogateMap, gs_connection_surrogate_map_destroy);

	GS_DELETE(&StoreNtwk, GsStoreNtwk);

	return 0;
}

int gs_store_worker_selfupdate_basic_create(
	const char *FileNameAbsoluteSelfUpdateBuf, size_t LenFileNameAbsoluteSelfUpdate,
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreWorkerSelfUpdateBasic **oStoreWorker)
{
	int r = 0;

	struct GsStoreWorkerSelfUpdateBasic *StoreWorker = new GsStoreWorkerSelfUpdateBasic();

	uint32_t NumWorkers = 0;

	if (!!(r = gs_ctrl_con_get_num_workers(ConnectionCommon->mCtrlCon, &NumWorkers)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_worker_init(
		GS_STORE_WORKER_SELFUPDATE_BASIC_MAGIC,
		gs_store_worker_cb_crank_t_selfupdate_basic,
		gs_store_worker_cb_destroy_t_selfupdate_basic,
		NumWorkers,
		ConnectionCommon,
		&StoreWorker->base)))
	{
		GS_GOTO_CLEAN();
	}

	StoreWorker->FileNameAbsoluteSelfUpdateBuf = FileNameAbsoluteSelfUpdateBuf;
	StoreWorker->LenFileNameAbsoluteSelfUpdate = LenFileNameAbsoluteSelfUpdate;
	StoreWorker->resultHaveUpdate = false;
	StoreWorker->resultBufferUpdate = std::string();

	if (oStoreWorker)
		*oStoreWorker = StoreWorker;

clean:
	if (!!r) {
		GS_DELETE(&StoreWorker, GsStoreWorkerSelfUpdateBasic);
	}

	return r;
}

int gs_store_worker_cb_destroy_t_selfupdate_basic(struct GsStoreWorker *StoreWorker)
{
	struct GsStoreWorkerSelfUpdateBasic *pThis = (struct GsStoreWorkerSelfUpdateBasic *) StoreWorker;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_STORE_WORKER_SELFUPDATE_BASIC_MAGIC);

	GS_DELETE(&StoreWorker, GsStoreWorker);

	return 0;
}

int crank_selfupdate_basic(struct GsCrankData *CrankData)
{
	int r = 0;

	GsStoreWorkerSelfUpdateBasic *pStoreWorker = (GsStoreWorkerSelfUpdateBasic *) CrankData->mStoreWorker;
	GsExtraWorkerSelfUpdateBasic *pIoExtraWorker = (GsExtraWorkerSelfUpdateBasic *) CrankData->mExtraWorker;

	uint32_t HaveUpdate = 0;
	std::string BufferUpdate;

	git_repository *RepositoryMemory = NULL;

	std::string BufferLatest;
	std::string BufferBlobs;
	GsPacket *PacketBlobOid = NULL;
	GsPacket *PacketBlob = NULL;
	uint32_t Offset = 0;
	uint32_t DataLengthLimit = 0;

	git_oid BlobSelfUpdateOidT = {};

	std::vector<git_oid> BlobSelfUpdateOidVec(1);
	GsStrided BlobSelfUpdateOidVecStrided = {};

	uint32_t BlobPairedVecLen = 0;
	uint32_t BlobOffsetSizeBuffer = 0;
	uint32_t BlobOffsetObjectBuffer = 0;

	struct GsAffinityToken AffinityToken = {};

	GS_BYPART_DATA_VAR(String, BysizeBufferLatest);
	GS_BYPART_DATA_INIT(String, BysizeBufferLatest, &BufferLatest);

	GS_BYPART_DATA_VAR(String, BysizeBufferBlobs);
	GS_BYPART_DATA_INIT(String, BysizeBufferBlobs, &BufferBlobs);

	if (pStoreWorker->base.magic != GS_STORE_WORKER_SELFUPDATE_BASIC_MAGIC)
		GS_ERR_CLEAN(1);

	if (pIoExtraWorker->base.magic != GS_EXTRA_WORKER_SELFUPDATE_BASIC_MAGIC)
		GS_ERR_CLEAN(1);

	if (!!(r = gs_strided_for_oid_vec_cpp(&BlobSelfUpdateOidVec, &BlobSelfUpdateOidVecStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_memory_repository_new(&RepositoryMemory)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_latest_selfupdate_blob(gs_bysize_cb_String, &BysizeBufferLatest)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		CrankData->mWorkerDataSend,
		&CrankData->mStoreWorker->mIntrToken,
		pIoExtraWorker->mId,
		BufferLatest.data(), BufferLatest.size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects2(
		CrankData,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		&AffinityToken,
		&PacketBlobOid,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

	Offset = 0;

	if (!!(r = aux_frame_ensure_frametype(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_LATEST_SELFUPDATE_BLOB))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_ensure(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, GS_PAYLOAD_OID_LEN)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_oid(PacketBlobOid->data, PacketBlobOid->dataLength, Offset, &Offset, (uint8_t *)&BlobSelfUpdateOidVec.at(0).id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	/* empty as_path parameter means no filters applied */
	if (!!(r = git_repository_hashfile(&BlobSelfUpdateOidT, RepositoryMemory, pStoreWorker->FileNameAbsoluteSelfUpdateBuf, GIT_OBJ_BLOB, "")))
		GS_GOTO_CLEAN_L(E, PF, "failure hashing [filename=[%.*s]]", pStoreWorker->LenFileNameAbsoluteSelfUpdate, pStoreWorker->FileNameAbsoluteSelfUpdateBuf);

	if (git_oid_cmp(&BlobSelfUpdateOidT, &BlobSelfUpdateOidVec.at(0)) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &BlobSelfUpdateOidT);
		GS_LOG(I, PF, "have latest [oid=[%.*s]]", GIT_OID_HEXSZ, buf);
	}

	if (!!(r = aux_frame_full_write_request_blobs_selfupdate(BlobSelfUpdateOidVecStrided, gs_bysize_cb_String, &BysizeBufferBlobs)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		CrankData->mWorkerDataSend,
		&CrankData->mStoreWorker->mIntrToken,
		pIoExtraWorker->mId,
		BufferBlobs.data(), BufferBlobs.size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_dequeue_timeout_reconnects2(
		CrankData,
		GS_SERV_AUX_ARBITRARY_TIMEOUT_MS,
		&AffinityToken,
		&PacketBlob,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

	Offset = 0;

	if (!!(r = aux_frame_ensure_frametype(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_TYPE_DECL(RESPONSE_BLOBS_SELFUPDATE))))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_read_size_limit(PacketBlob->data, PacketBlob->dataLength, Offset, &Offset, GS_FRAME_SIZE_LEN, &DataLengthLimit)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_aux_read_paired_vec_noalloc(
		PacketBlob->data, DataLengthLimit, Offset, &Offset,
		&BlobPairedVecLen, &BlobOffsetSizeBuffer, &BlobOffsetObjectBuffer)))
	{
		GS_GOTO_CLEAN();
	}

	if (BlobPairedVecLen != 1)
		GS_ERR_CLEAN(1);

	{
		uint32_t BlobZeroSize = 0;
		git_oid BlobZeroOid = {};
		
		git_blob *BlobZero = NULL;

		aux_LE_to_uint32(&BlobZeroSize, (char *)(PacketBlob->data + BlobOffsetSizeBuffer), GS_FRAME_SIZE_LEN);

		if (!!(r = git_blob_create_frombuffer(&BlobZeroOid, RepositoryMemory, PacketBlob->data + BlobOffsetObjectBuffer, BlobZeroSize)))
			GS_GOTO_CLEANSUB();

		if (!!(r = git_blob_lookup(&BlobZero, RepositoryMemory, &BlobZeroOid)))
			GS_GOTO_CLEANSUB();

		/* wtf? was the wrong blob sent? */
		if (git_oid_cmp(&BlobZeroOid, &BlobSelfUpdateOidVec.at(0)) != 0)
			GS_ERR_CLEANSUB(1);

		HaveUpdate = 1;
		BufferUpdate = std::string((char *)git_blob_rawcontent(BlobZero), git_blob_rawsize(BlobZero));

	cleansub:

		if (BlobZero)
			git_blob_free(BlobZero);

		if (!!r)
			GS_GOTO_CLEAN();
	}

	GS_ERR_NO_CLEAN(GS_ERRCODE_EXIT);

noclean:

	pStoreWorker->resultHaveUpdate = HaveUpdate;
	pStoreWorker->resultBufferUpdate.swap(BufferUpdate);

clean :
	GS_RELEASE_F(&AffinityToken, gs_affinity_token_release);

	if (RepositoryMemory)
		git_repository_free(RepositoryMemory);

	return r;
}

int gs_net_full_create_connection_selfupdate_basic(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	const char *FileNameAbsoluteSelfUpdateBuf, size_t LenFileNameAbsoluteSelfUpdate,
	uint32_t *oHaveUpdate,
	std::string *oBufferUpdate)
{
	int r = 0;

	struct GsFullConnection *ConnectionSelfUpdateBasic = NULL;
	struct GsFullConnectionCommonData *ConnectionCommon = NULL;

	struct GsExtraHostCreateSelfUpdateBasic *ExtraHostCreate = NULL;
	struct GsStoreNtwkSelfUpdateBasic       *StoreNtwk       = NULL;
	struct GsStoreWorkerSelfUpdateBasic     *StoreWorker     = NULL;

	if (!!(r = gs_full_connection_common_data_create(GS_MAGIC_NUM_WORKER_THREADS, &ConnectionCommon)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_extra_host_create_selfupdate_basic_create(ServPort, ServHostNameBuf, LenServHostName, &ExtraHostCreate)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_ntwk_selfupdate_basic_create(ConnectionCommon, &StoreNtwk)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_store_worker_selfupdate_basic_create(
		FileNameAbsoluteSelfUpdateBuf, LenFileNameAbsoluteSelfUpdate,
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
		ConnectionCommon,
		&ConnectionSelfUpdateBasic,
		"selfup")))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_ctrl_con_wait_exited(ConnectionSelfUpdateBasic->mCtrlCon)))
		GS_GOTO_CLEAN();

	if (oHaveUpdate)
		*oHaveUpdate = StoreWorker->resultHaveUpdate;

	if (oBufferUpdate)
		*oBufferUpdate = StoreWorker->resultBufferUpdate;

	// FIXME: since this function performs full wait on CtrlCon and extract results protocol
	//   (instead of returning the connection), it must also destroy the connection
	GS_DELETE_F(&ConnectionSelfUpdateBasic, gs_full_connection_destroy);

clean:
	if (!!r) {
		GS_DELETE_F(&ConnectionSelfUpdateBasic, gs_full_connection_destroy);
		GS_DELETE_BASE_VF(&StoreWorker, cb_destroy_t);
		GS_DELETE_BASE_VF(&StoreNtwk, cb_destroy_t);
		GS_DELETE_BASE_VF(&ExtraHostCreate, cb_destroy_t);
		GS_DELETE_F(&ConnectionCommon, gs_full_connection_common_data_destroy);
	}

	return r;
}

int gs_store_worker_cb_crank_t_selfupdate_basic(struct GsCrankData *CrankData)
{
	int r = 0;

	/* oneshot - not ran inside a loop */
	if (!!(r = crank_selfupdate_basic(CrankData)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_extra_host_create_cb_create_t_selfupdate_basic(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	size_t LenExtraWorker,
	struct GsExtraWorker **oExtraWorkerArr)
{
	int r = 0;

	struct GsExtraHostCreateSelfUpdateBasic *pThis = (struct GsExtraHostCreateSelfUpdateBasic *) ExtraHostCreate;

	struct GsHostSurrogate Host = {};

	gs_connection_surrogate_id_t AssignedId = 0;

	int errService = 0;

	if (pThis->base.magic != GS_EXTRA_HOST_CREATE_SELFUPDATE_BASIC_MAGIC)
		GS_ERR_CLEAN(1);

	/* create host */

	// FIXME: 128 peerCount, 1 channelLimit
	if (!!(r = gs_host_surrogate_setup_host_nobind(128, &Host)))
		GS_GOTO_CLEAN();

	/* create and register connection */

	if (!!(r = gs_host_surrogate_connect_wait_blocking_register(
		&Host,
		pThis->mServPort,
		pThis->mServHostNameBuf, pThis->mLenServHostName,
		ioConnectionSurrogateMap,
		&AssignedId)))
	{
		GS_GOTO_CLEAN();
	}

	/* output */

	for (size_t i = 0; i < LenExtraWorker; i++)
		if (!!(r = gs_extra_worker_selfupdate_basic_create(oExtraWorkerArr + i, AssignedId)))
			GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		*ioHostSurrogate = Host;

clean:

	return r;
}

int gs_extra_worker_selfupdate_basic_create(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id)
{
	struct GsExtraWorkerSelfUpdateBasic * pThis = new GsExtraWorkerSelfUpdateBasic();

	pThis->base.magic = GS_EXTRA_WORKER_SELFUPDATE_BASIC_MAGIC;
	pThis->base.cb_destroy_t = gs_extra_worker_cb_destroy_t_selfupdate_basic;

	pThis->mId = Id;

	if (oExtraWorker)
		*oExtraWorker = &pThis->base;

	return 0;
}

int gs_extra_worker_cb_destroy_t_selfupdate_basic(struct GsExtraWorker *ExtraWorker)
{
	struct GsExtraWorkerSelfUpdateBasic *pThis = (struct GsExtraWorkerSelfUpdateBasic *) ExtraWorker;

	if (!pThis)
		return 0;

	GS_ASSERT(pThis->base.magic == GS_EXTRA_WORKER_SELFUPDATE_BASIC_MAGIC);

	GS_DELETE(&ExtraWorker, GsExtraWorker);

	return 0;
}
