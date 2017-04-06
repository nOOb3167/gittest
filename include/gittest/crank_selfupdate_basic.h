#ifndef _GITTEST_CRANK_SELFUPDATE_BASIC_H_
#define _GITTEST_CRANK_SELFUPDATE_BASIC_H_

#include <stddef.h>
#include <stdint.h>

#include <gittest/net2.h>

struct GsExtraHostCreateSelfUpdateBasic
{
	struct GsExtraHostCreate base;

	uint32_t mServPort;
	const char *mServHostNameBuf; size_t mLenServHostName;
};

struct GsExtraWorkerSelfUpdateBasic
{
	struct GsExtraWorker base;

	gs_connection_surrogate_id_t mId;
};

struct GsStoreNtwkSelfUpdateBasic
{
	struct GsStoreNtwk base;
};

struct GsStoreWorkerSelfUpdateBasic
{
	struct GsStoreWorker base;

	const char *FileNameAbsoluteSelfUpdateBuf; size_t LenFileNameAbsoluteSelfUpdate;

	struct GsIntrTokenSurrogate mIntrToken;

	uint32_t    resultHaveUpdate;
	std::string resultBufferUpdate;
};

int crank_selfupdate_basic(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	gs_connection_surrogate_id_t IdForSend,
	struct GsIntrTokenSurrogate *IntrToken,
	const char *FileNameAbsoluteSelfUpdateBuf, size_t LenFileNameAbsoluteSelfUpdate,
	uint32_t *oHaveUpdate,
	std::string *oBufferUpdate);

int gs_net_full_create_connection_selfupdate_basic(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	const char *FileNameAbsoluteSelfUpdateBuf, size_t LenFileNameAbsoluteSelfUpdate,
	uint32_t *oHaveUpdate,
	std::string *BufferUpdate);

int gs_store_worker_cb_crank_t_selfupdate_basic(
	struct GsWorkerData *WorkerDataRecv,
	struct GsWorkerData *WorkerDataSend,
	struct GsStoreWorker *StoreWorker,
	struct GsExtraWorker *ExtraWorker);

int gs_extra_host_create_cb_create_t_selfupdate_basic(
	GsExtraHostCreate *ExtraHostCreate,
	GsHostSurrogate *ioHostSurrogate,
	GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	GsExtraWorker **oExtraWorker);

int gs_extra_worker_cb_create_t_selfupdate_basic(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id);

int gs_extra_worker_cb_destroy_t_selfupdate_basic(struct GsExtraWorker *ExtraWorker);

#endif /* _GITTEST_CRANK_SELFUPDATE_BASIC_H_ */