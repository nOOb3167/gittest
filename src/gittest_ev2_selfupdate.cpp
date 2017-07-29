#include <cstdlib>

#include <thread>

#include <git2.h>

#include <enet/enet.h>

#include <gittest/misc.h>
#include <gittest/config.h>
#include <gittest/log.h>
#include <gittest/gittest.h>

#include <gittest/gittest_ev2_test.h>

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

int main(int argc, char **argv)
{
	int r = 0;

	struct GsConfMap *ConfMap = NULL;
	struct GsAuxConfigCommonVars CommonVars = {};
	struct GsEvCtxSelfUpdate *Ctx = NULL;

	std::thread ThreadServ;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	/* NOTE: enet_initialize takes care of calling WSAStartup (needed for LibEvent) */
	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_crash_handler_setup()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_create_common_logs()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_read_default_everything(&ConfMap)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_get_common_vars(ConfMap, &CommonVars)))
		GS_GOTO_CLEAN();

	ThreadServ.swap(std::thread(gs_ev2_test_servmain, CommonVars));

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	if (!!(r = gs_ev2_test_selfupdatemain(CommonVars, &Ctx)))
		GS_GOTO_CLEAN();

	GS_ASSERT(! gs_selfupdate_state_code_ensure(Ctx->mState, GS_SELFUPDATE_STATE_CODE_NEED_NOTHING));

clean:
	if (!!r)
		GS_ASSERT(0);

	GS_DELETE_F(&ConfMap, gs_conf_map_destroy);

	return EXIT_SUCCESS;
}
