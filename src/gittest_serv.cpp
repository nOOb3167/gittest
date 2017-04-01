#include <cstdlib>
#include <cassert>

#include <thread>  // std::this_thread
#include <chrono>

#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/net2.h>
#include <gittest/crank_serv.h>
#include <gittest/gittest_selfupdate.h>

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

int startserv() {
	int r = 0;

	confmap_t KeyVal;

	GsAuxConfigCommonVars CommonVars = {};

	sp<GsFullConnection> FcsServ;

	log_guard_t log(GS_LOG_GET("serv"));

	if (!!(r = aux_config_read_default_everything(&KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_get_common_vars(KeyVal, &CommonVars)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_net_full_create_connection_server(
		CommonVars.ServPort,
		CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain,
		CommonVars.RefNameSelfUpdateBuf, CommonVars.LenRefNameSelfUpdate,
		CommonVars.RepoMainPathBuf, CommonVars.LenRepoMainPath,
		CommonVars.RepoSelfUpdatePathBuf, CommonVars.LenRepoSelfUpdatePath,
		&FcsServ)))
	{
		GS_GOTO_CLEAN();
	}

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

clean:

	return r;
}

int setuplogging() {
	int r = 0;

	if (!!(r = gs_log_crash_handler_setup()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_create_common_logs()))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int main(int argc, char **argv) {
	int r = 0;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = setuplogging()))
		GS_GOTO_CLEAN();

	if (!!(r = startserv()))
		GS_GOTO_CLEAN();

clean:
	if (!!r) {
		GS_ASSERT(0);
	}

	return EXIT_SUCCESS;
}
