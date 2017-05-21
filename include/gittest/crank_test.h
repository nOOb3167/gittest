#ifndef _CRANK_TEST_H_
#define _CRANK_TEST_H_

#include <gittest/net2.h>

/** @sa
       ::gs_extra_host_create_cb_create_t_test
*/
struct GsExtraHostCreateTest
{
	struct GsExtraHostCreate base;

	void *Ctx; /**< notowned */

	uint32_t mServPort;
};

/** @sa
       ::gs_extra_worker_test_create
*/
struct GsExtraWorkerTest
{
	struct GsExtraWorker base;
};

struct GsStoreNtwkTest
{
	struct GsStoreNtwk base;

	void *Ctx; /**< notowned */
};

struct GsStoreWorkerTest
{
	struct GsStoreWorker base;

	void *Ctx; /**< notowned */
};

int gs_extra_worker_test_create(
	struct GsExtraWorker **oExtraWorker);

int gs_net_full_create_connection_test(
	uint32_t NumWorkers,
	uint32_t ServPort,
	const char *ExtraThreadName,
	int (*CbCrank)(struct GsCrankData *CrankData),
	void *Ctx,
	struct GsFullConnection **oConnection);

int gs_extra_host_create_cb_create_t_test(
	struct GsExtraHostCreate *ExtraHostCreate,
	struct GsHostSurrogate *ioHostSurrogate,
	struct GsConnectionSurrogateMap *ioConnectionSurrogateMap,
	size_t LenExtraWorker,
	struct GsExtraWorker **oExtraWorkerArr);

#endif /* _CRANK_TEST_H_ */
