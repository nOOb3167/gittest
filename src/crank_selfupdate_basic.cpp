#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cstddef>
#include <cstdint>

#include <vector>
#include <string>

#include <git2.h>

#include <gittest/gittest.h>
#include <gittest/frame.h>
#include <gittest/net2.h>

#include <gittest/crank_selfupdate_basic.h>

int crank_selfupdate_basic(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	const char *FileNameAbsoluteSelfUpdateBuf, size_t LenFileNameAbsoluteSelfUpdate,
	uint32_t *oHaveUpdate,
	std::string *oBufferUpdate)
{
	int r = 0;

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

	GS_BYPART_DATA_VAR(String, BysizeBufferLatest);
	GS_BYPART_DATA_INIT(String, BysizeBufferLatest, &BufferLatest);

	GS_BYPART_DATA_VAR(String, BysizeBufferBlobs);
	GS_BYPART_DATA_INIT(String, BysizeBufferBlobs, &BufferBlobs);

	if (!!(r = gs_strided_for_oid_vec_cpp(&BlobSelfUpdateOidVec, &BlobSelfUpdateOidVecStrided)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_memory_repository_new(&RepositoryMemory)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_frame_full_write_request_latest_selfupdate_blob(gs_bysize_cb_String, &BysizeBufferLatest)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		WorkerDataSend,
		IntrToken,
		IdForSend,
		BufferLatest.data(), BufferLatest.size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_dequeue(
		WorkerDataRecv,
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
	if (!!(r = git_repository_hashfile(&BlobSelfUpdateOidT, RepositoryMemory, FileNameAbsoluteSelfUpdateBuf, GIT_OBJ_BLOB, "")))
		GS_GOTO_CLEAN_L(E, PF, "failure hashing [filename=[%.*s]]", LenFileNameAbsoluteSelfUpdate, FileNameAbsoluteSelfUpdateBuf);

	if (git_oid_cmp(&BlobSelfUpdateOidT, &BlobSelfUpdateOidVec.at(0)) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &BlobSelfUpdateOidT);
		GS_LOG(I, PF, "have latest [oid=[%.*s]]", GIT_OID_HEXSZ, buf);
	}

	if (!!(r = aux_frame_full_write_request_blobs_selfupdate(BlobSelfUpdateOidVecStrided, gs_bysize_cb_String, &BysizeBufferBlobs)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_worker_packet_enqueue(
		WorkerDataSend,
		IntrToken,
		IdForSend,
		BufferBlobs.data(), BufferBlobs.size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_worker_packet_dequeue(
		WorkerDataRecv,
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

	if (oHaveUpdate)
		*oHaveUpdate = HaveUpdate;

	if (oBufferUpdate)
		oBufferUpdate->swap(BufferUpdate);

	GS_ERR_NO_CLEAN(GS_ERRCODE_EXIT);

noclean:

clean:
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

	sp<GsFullConnection> ConnectionSelfUpdateBasic;

	ENetIntrTokenCreateFlags *IntrTokenFlags = NULL;
	GsIntrTokenSurrogate      IntrTokenSurrogate = {};

	GsCtrlCon                        *CtrlCon = NULL;

	GsExtraHostCreateSelfUpdateBasic *ExtraHostCreate = new GsExtraHostCreateSelfUpdateBasic();
	GsStoreNtwkSelfUpdateBasic       *StoreNtwk       = new GsStoreNtwkSelfUpdateBasic();
	GsStoreWorkerSelfUpdateBasic     *StoreWorker     = new GsStoreWorkerSelfUpdateBasic();

	sp<GsExtraHostCreate> pExtraHostCreate(&ExtraHostCreate->base);
	sp<GsStoreNtwk>       pStoreNtwk(&StoreNtwk->base);
	sp<GsStoreWorker>     pStoreWorker(&StoreWorker->base);

	if (!(IntrTokenFlags = enet_intr_token_create_flags_create(ENET_INTR_DATA_TYPE_NONE)))
		GS_GOTO_CLEAN();

	if (!(IntrTokenSurrogate.mIntrToken = enet_intr_token_create(IntrTokenFlags)))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_ctrl_con_create(&CtrlCon, 2)))
		GS_GOTO_CLEAN();

	ExtraHostCreate->base.magic = GS_EXTRA_HOST_CREATE_SELFUPDATE_BASIC_MAGIC;
	ExtraHostCreate->base.cb_create_t = gs_extra_host_create_cb_create_t_selfupdate_basic;
	ExtraHostCreate->mServPort = ServPort;
	ExtraHostCreate->mServHostNameBuf = ServHostNameBuf;
	ExtraHostCreate->mLenServHostName = LenServHostName;

	StoreNtwk->base.magic = GS_STORE_NTWK_SELFUPDATE_BASIC_MAGIC;
	StoreNtwk->base.mIntrTokenSurrogate = IntrTokenSurrogate;
	StoreNtwk->base.mCtrlCon = CtrlCon;

	StoreWorker->base.magic = GS_STORE_WORKER_SELFUPDATE_BASIC_MAGIC;
	StoreWorker->base.cb_crank_t = gs_store_worker_cb_crank_t_selfupdate_basic;
	StoreWorker->base.mCtrlCon = CtrlCon;
	StoreWorker->FileNameAbsoluteSelfUpdateBuf = FileNameAbsoluteSelfUpdateBuf;
	StoreWorker->LenFileNameAbsoluteSelfUpdate = LenFileNameAbsoluteSelfUpdate;
	StoreWorker->resultHaveUpdate = false;
	StoreWorker->resultBufferUpdate = std::string();
	StoreWorker->mIntrToken = IntrTokenSurrogate;

	if (!!(r = gs_net_full_create_connection(
		ServPort,
		pExtraHostCreate,
		pStoreNtwk,
		pStoreWorker,
		&ConnectionSelfUpdateBasic,
		"selfup")))
	{
		GS_GOTO_CLEAN();
	}

	GS_SP_SET_RAW_NULLING(ConnectionSelfUpdateBasic->mCtrlCon, CtrlCon, GsCtrlCon);

	if (!!(r = gs_ctrl_con_wait_exited(ConnectionSelfUpdateBasic->mCtrlCon.get())))
		GS_GOTO_CLEAN();

	if (oHaveUpdate)
		*oHaveUpdate = StoreWorker->resultHaveUpdate;

	if (oBufferUpdate)
		*oBufferUpdate = StoreWorker->resultBufferUpdate;

clean:

	return r;
}

int gs_store_worker_cb_crank_t_selfupdate_basic(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker *ExtraWorker)
{
	int r = 0;

	GsStoreWorkerSelfUpdateBasic *pStoreWorker = (GsStoreWorkerSelfUpdateBasic *)StoreWorker;
	GsExtraWorkerSelfUpdateBasic *pExtraWorker = (GsExtraWorkerSelfUpdateBasic *)ExtraWorker;

	uint32_t HaveUpdate = false;
	std::string BufferUpdate;

	if (pStoreWorker->base.magic != GS_STORE_WORKER_SELFUPDATE_BASIC_MAGIC)
		GS_ERR_CLEAN(1);

	if (pExtraWorker->base.magic != GS_EXTRA_WORKER_SELFUPDATE_BASIC_MAGIC)
		GS_ERR_CLEAN(1);

	/* oneshot - not ran inside a loop */
	if (!!(r = crank_selfupdate_basic(
		WorkerDataRecv,
		WorkerDataSend,
		pExtraWorker->mId,
		&pStoreWorker->mIntrToken,
		pStoreWorker->FileNameAbsoluteSelfUpdateBuf, pStoreWorker->LenFileNameAbsoluteSelfUpdate,
		&HaveUpdate,
		&BufferUpdate)))
	{
		GS_ERR_NO_CLEAN(r);
	}

noclean:

	pStoreWorker->resultHaveUpdate = HaveUpdate;
	pStoreWorker->resultBufferUpdate.swap(BufferUpdate);

clean:

	return r;
}

int gs_extra_host_create_cb_create_t_selfupdate_basic(
	GsExtraHostCreate *ExtraHostCreate,
	GsHostSurrogate *ioHostSurrogate,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsExtraWorker **oExtraWorker)
{
	int r = 0;

	struct GsExtraHostCreateSelfUpdateBasic *pThis = (struct GsExtraHostCreateSelfUpdateBasic *) ExtraHostCreate;

	ENetIntrHostCreateFlags FlagsHost = {};
	ENetHost *host = NULL;
	ENetAddress addr = {};
	ENetPeer *peer = NULL;
	ENetEvent event = {};

	GsConnectionSurrogate ConnectionSurrogate = {};

	gs_connection_surrogate_id_t AssignedId = 0;

	GsExtraWorker *ExtraWorker = NULL;

	int errService = 0;

	if (pThis->base.magic != GS_EXTRA_HOST_CREATE_SELFUPDATE_BASIC_MAGIC)
		GS_ERR_CLEAN(1);

	/* create host */

	// FIXME: 128 peerCount, 1 channelLimit
	if (!(host = enet_host_create_interruptible(NULL, 128, 1, 0, 0, &FlagsHost)))
		GS_ERR_CLEAN(1);

	/* connect host */

	if (!!(r = enet_address_set_host(&addr, pThis->mServHostNameBuf)))
		GS_ERR_CLEAN(1);
	addr.port = pThis->mServPort;

	if (!(peer = enet_host_connect(host, &addr, 1, 0)))
		GS_ERR_CLEAN(1);

	/* connect host - wait for completion */

	while (0 <= (errService = enet_host_service(host, &event, GS_TIMEOUT_1SEC))) {
		if (errService > 0 && event.peer == peer && event.type == ENET_EVENT_TYPE_CONNECT)
			break;
	}

	/* a connection event must have been setup above */
	if (errService < 0 ||
		event.type != ENET_EVENT_TYPE_CONNECT ||
		event.peer == NULL)
	{
		GS_ERR_CLEAN(1);
	}

	/* register connection and prepare extra worker data */

	ConnectionSurrogate.mHost = host;
	ConnectionSurrogate.mPeer = peer;
	ConnectionSurrogate.mIsPrincipalClientConnection = true;

	// FIXME: pretty ugly initialization of GsConnectionSurrogate
	if (!!(r = gs_aux_aux_aux_connection_register_transfer_ownership(
		ConnectionSurrogate,
		ioConnectionSurrogateMap,
		&AssignedId)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_extra_worker_cb_create_t_selfupdate_basic(&ExtraWorker, AssignedId)))
		GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		ioHostSurrogate->mHost = host;

	if (oExtraWorker)
		*oExtraWorker = ExtraWorker;

clean:

	return r;
}

int gs_extra_worker_cb_create_t_selfupdate_basic(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id)
{
	struct GsExtraWorkerSelfUpdateBasic * pThis = new GsExtraWorkerSelfUpdateBasic();

	pThis->base.magic = GS_EXTRA_WORKER_SELFUPDATE_BASIC_MAGIC;

	pThis->base.cb_create_t = gs_extra_worker_cb_create_t_selfupdate_basic;
	pThis->base.cb_destroy_t = gs_extra_worker_cb_destroy_t_selfupdate_basic;

	pThis->mId = Id;

	if (oExtraWorker)
		*oExtraWorker = &pThis->base;

	return 0;
}

int gs_extra_worker_cb_destroy_t_selfupdate_basic(struct GsExtraWorker *ExtraWorker)
{
	if (ExtraWorker->magic != GS_EXTRA_WORKER_SELFUPDATE_BASIC_MAGIC)
		return -1;

	delete ExtraWorker;

	return 0;
}
