#ifndef _GITTEST_CRANK_SELFUPDATE_BASIC_H_
#define _GITTEST_CRANK_SELFUPDATE_BASIC_H_

#include <stddef.h>
#include <stdint.h>

#include <gittest/net2.h>

/** @sa
       ::gs_extra_host_create_selfupdate_basic_create
	   ::gs_extra_host_create_cb_destroy_host_t_enet_host_destroy
	   ::gs_extra_host_create_cb_destroy_t_delete
*/
struct GsExtraHostCreateSelfUpdateBasic
{
	struct GsExtraHostCreate base;

	uint32_t mServPort;
	const char *mServHostNameBuf; size_t mLenServHostName;
};

/** @sa
       ::gs_extra_worker_selfupdate_basic_create
	   ::gs_extra_worker_cb_destroy_t_selfupdate_basic
*/
struct GsExtraWorkerSelfUpdateBasic
{
	struct GsExtraWorker base;

	gs_connection_surrogate_id_t mId;
};

/** @sa
       ::gs_store_ntwk_selfupdate_basic_create
	   ::gs_store_ntwk_cb_destroy_t_selfupdate_basic
*/
struct GsStoreNtwkSelfUpdateBasic
{
	struct GsStoreNtwk base;
};

/** @sa
       ::gs_store_worker_selfupdate_basic_create
	   ::gs_store_worker_cb_destroy_t_selfupdate_basic
*/
struct GsStoreWorkerSelfUpdateBasic
{
	struct GsStoreWorker base;

	const char *FileNameAbsoluteSelfUpdateBuf; size_t LenFileNameAbsoluteSelfUpdate;

	uint32_t    resultHaveUpdate;
	std::string resultBufferUpdate;
};

int gs_extra_host_create_selfupdate_basic_create(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	struct GsExtraHostCreateSelfUpdateBasic **oExtraHostCreate);

int gs_store_ntwk_selfupdate_basic_create(
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreNtwkSelfUpdateBasic **oStoreNtwk);
int gs_store_ntwk_cb_destroy_t_selfupdate_basic(struct GsStoreNtwk *StoreNtwk);

int gs_store_worker_selfupdate_basic_create(
	const char *FileNameAbsoluteSelfUpdateBuf, size_t LenFileNameAbsoluteSelfUpdate,
	struct GsFullConnectionCommonData *ConnectionCommon,
	struct GsStoreWorkerSelfUpdateBasic **oStoreWorker);
int gs_store_worker_cb_destroy_t_selfupdate_basic(struct GsStoreWorker *StoreWorker);

int crank_selfupdate_basic(struct GsCrankData *CrankData);

int gs_net_full_create_connection_selfupdate_basic(
	uint32_t ServPort,
	const char *ServHostNameBuf, size_t LenServHostName,
	const char *FileNameAbsoluteSelfUpdateBuf, size_t LenFileNameAbsoluteSelfUpdate,
	uint32_t *oHaveUpdate,
	std::string *BufferUpdate);

int gs_store_worker_cb_crank_t_selfupdate_basic(struct GsCrankData *CrankData);

int gs_extra_host_create_cb_create_t_selfupdate_basic(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	size_t LenExtraWorker,
	struct GsExtraWorker **oExtraWorkerArr);

int gs_extra_worker_selfupdate_basic_create(
	struct GsExtraWorker **oExtraWorker,
	gs_connection_surrogate_id_t Id);

int gs_extra_worker_cb_destroy_t_selfupdate_basic(struct GsExtraWorker *ExtraWorker);

#endif /* _GITTEST_CRANK_SELFUPDATE_BASIC_H_ */
