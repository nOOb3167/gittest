#include <cstdlib>
#include <cassert>

#include <gittest/gittest.h>
#include <gittest/net.h>
#include <gittest/gittest_selfupdate.h>

int startserv() {
	int r = 0;

	confmap_t KeyVal;

	GsAuxConfigCommonVars CommonVars = {};

	uint32_t ConfServPort = 0;
	std::string ConfRefNameSelfUpdate;
	std::string ConfRefNameMain;
	std::string ConfRepoMainOpenPath;
	std::string ConfRepoSelfUpdateOpenPath;

	sp<FullConnectionClient> FcsServ;

	if (!!(r = aux_config_read_interpret_relative_current_executable("../data/", "gittest_config_serv.conf", &KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_get_common_vars(KeyVal, &CommonVars)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_full_create_connection_server(
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

int main(int argc, char **argv) {
	int r = 0;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = startserv()))
		GS_GOTO_CLEAN();

clean:
	if (!!r) {
		assert(0);
	}

	return EXIT_SUCCESS;
}
