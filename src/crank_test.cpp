#include <gittest/net2.h>

#include <gittest/crank_test.h>

int gs_extra_worker_test_create(
	struct GsExtraWorker **oExtraWorker)
{
	struct GsExtraWorkerTest * pThis = new GsExtraWorkerTest();

	pThis->base.magic = GS_EXTRA_WORKER_TEST_MAGIC;

	pThis->base.cb_destroy_t = gs_extra_worker_cb_destroy_t_delete;

	if (oExtraWorker)
		*oExtraWorker = &pThis->base;

	return 0;
}

int gs_net_full_create_connection_test(
	uint32_t NumWorkers,
	uint32_t ServPort,
	const char *ExtraThreadName,
	int (*CbCrank)(struct GsCrankData *CrankData),
	void *Ctx,
	struct GsFullConnection **oConnection)
{
	int r = 0;

	struct GsFullConnection *Connection = NULL;
	struct GsFullConnectionCommonData *ConnectionCommon = NULL;

	struct GsExtraHostCreateTest *ExtraHostCreate = new GsExtraHostCreateTest();
	struct GsStoreNtwkTest       *StoreNtwk       = new GsStoreNtwkTest();
	struct GsStoreWorkerTest     *StoreWorker     = new GsStoreWorkerTest();

	if (!!(r = gs_full_connection_common_data_create(NumWorkers, &ConnectionCommon)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_extra_host_create_init(
		GS_EXTRA_HOST_CREATE_TEST_MAGIC,
		gs_extra_host_create_cb_create_t_test,
		gs_extra_host_create_cb_destroy_host_t_enet_host_destroy,
		gs_extra_host_create_cb_destroy_t_delete,
		&ExtraHostCreate->base)))
	{
		GS_GOTO_CLEAN();
	}

	ExtraHostCreate->Ctx = Ctx;
	ExtraHostCreate->mServPort = ServPort;


	if (!!(r = gs_store_ntwk_init(
		GS_STORE_NTWK_TEST_MAGIC,
		gs_store_ntwk_cb_destroy_t_delete,
		ConnectionCommon,
		&StoreNtwk->base)))
	{
		GS_GOTO_CLEAN();
	}

	StoreNtwk->Ctx = Ctx;


	if (!!(r = gs_store_worker_init(
		GS_STORE_WORKER_TEST_MAGIC,
		CbCrank,
		gs_store_worker_cb_destroy_t_delete,
		NumWorkers,
		ConnectionCommon,
		&StoreWorker->base)))
	{
		GS_GOTO_CLEAN();
	}

	StoreWorker->Ctx = Ctx;


	if (!!(r = gs_net_full_create_connection(
		ServPort,
		GS_BASE_ARGOWN(&ExtraHostCreate),
		GS_BASE_ARGOWN(&StoreNtwk),
		GS_BASE_ARGOWN(&StoreWorker),
		GS_ARGOWN(&ConnectionCommon),
		&Connection,
		ExtraThreadName)))
	{
		GS_GOTO_CLEAN();
	}

	if (oConnection)
		*oConnection = Connection;

clean:
	if (!!r) {
		GS_DELETE_F(&Connection, gs_full_connection_destroy);
		GS_DELETE_BASE_VF(&StoreWorker, cb_destroy_t);
		GS_DELETE_BASE_VF(&StoreNtwk, cb_destroy_t);
		GS_DELETE_BASE_VF(&ExtraHostCreate, cb_destroy_t);
		GS_DELETE_F(&ConnectionCommon, gs_full_connection_common_data_destroy);
	}

	return r;
}

int gs_extra_host_create_cb_create_t_test(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	size_t LenExtraWorker,
	struct GsExtraWorker **oExtraWorkerArr)
{
	int r = 0;

	struct GsExtraHostCreateTest *pThis = (struct GsExtraHostCreateTest *) ExtraHostCreate;

	struct GsHostSurrogate Host = {};

	if (pThis->base.magic != GS_EXTRA_HOST_CREATE_TEST_MAGIC)
		GS_ERR_CLEAN(1);

	/* create host */

	// FIXME: 128 peerCount, 1 channelLimit
	if (!!(r = gs_host_surrogate_setup_host_bind_port(pThis->mServPort, 128, &Host)))
		GS_GOTO_CLEAN();

	/* output */

	for (uint32_t i = 0; i < LenExtraWorker; i++)
		if (!!(r = gs_extra_worker_test_create(oExtraWorkerArr + i)))
			GS_GOTO_CLEAN();

	if (ioHostSurrogate)
		*ioHostSurrogate = Host;

clean:

	return r;
}
