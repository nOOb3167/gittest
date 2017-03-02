#include <cstdlib>
#include <cassert>

#include <gittest/gittest.h>
#include <gittest/net.h>

int startserv() {
	int r = 0;

	confmap_t KeyVal;

	uint32_t ConfServPort = 0;
	std::string ConfRefNameSelfUpdate;
	std::string ConfRefNameMain;
	std::string ConfRepoMainOpenPath;
	std::string ConfRepoSelfUpdateOpenPath;

	sp<FullConnectionClient> FcsServ;

	if (!!(r = aux_config_read("../data/", "gittest_config_serv.conf", &KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_uint32(KeyVal, "ConfServPort", &ConfServPort)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(KeyVal, "ConfRefNameMain", &ConfRefNameMain)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(KeyVal, "ConfRefNameSelfUpdate", &ConfRefNameSelfUpdate)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(KeyVal, "ConfRepoMainPath", &ConfRepoMainOpenPath)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(KeyVal, "ConfRepoSelfUpdatePath", &ConfRepoSelfUpdateOpenPath)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_full_create_connection_server(
		ConfServPort,
		ConfRefNameMain.c_str(), ConfRefNameMain.size(),
		ConfRefNameSelfUpdate.c_str(), ConfRefNameSelfUpdate.size(),
		ConfRepoMainOpenPath.c_str(), ConfRepoMainOpenPath.size(),
		ConfRepoSelfUpdateOpenPath.c_str(), ConfRepoSelfUpdateOpenPath.size(),
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
