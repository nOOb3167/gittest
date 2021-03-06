#include <cstdlib>
#include <cassert>

#include <thread>  // std::this_thread
#include <chrono>

#include <gittest/misc.h>
#include <gittest/config.h>
#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/net2.h>
#include <gittest/crank_serv.h>
#include <gittest/gittest_selfupdate.h>

GsLogList *g_gs_log_list_global = gs_log_list_global_create();

int startserv() {
	int r = 0;

	struct GsConfMap *ConfMap = NULL;

	GsAuxConfigCommonVars CommonVars = {};

	struct GsFullConnection *FcsServ = NULL;

	log_guard_t log(GS_LOG_GET("serv"));

	if (!!(r = gs_config_read_default_everything(&ConfMap)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_get_common_vars(ConfMap, &CommonVars)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_create_common_logs(ConfMap)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_net_full_create_connection_server(
		CommonVars.ServPort,
		CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain,
		CommonVars.RefNameSelfUpdateBuf, CommonVars.LenRefNameSelfUpdate,
		CommonVars.RepoMainPathBuf, CommonVars.LenRepoMainPath,
		CommonVars.RepoSelfUpdatePathBuf, CommonVars.LenRepoSelfUpdatePath,
		CommonVars.SelfUpdateBlobNameBuf, CommonVars.LenSelfUpdateBlobName,
		&FcsServ)))
	{
		GS_GOTO_CLEAN();
	}

	GS_LOG(I, S, "connection exit waiting");

	if (!!(r = gs_ctrl_con_wait_exited(FcsServ->mCtrlCon)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "connection exit success");

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

clean:
	if (!!r) {
		GS_DELETE(&FcsServ, GsFullConnection);
	}

	GS_DELETE_F(&ConfMap, gs_conf_map_destroy);

	return r;
}

int main(int argc, char **argv) {
	int r = 0;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_crash_handler_setup()))
		GS_GOTO_CLEAN();

	if (!!(r = startserv()))
		GS_GOTO_CLEAN();

clean:
	if (!!r) {
		GS_ASSERT(0);
	}

	return EXIT_SUCCESS;
}
