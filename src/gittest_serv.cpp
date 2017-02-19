#include <cstdlib>
#include <cassert>

#include <gittest/gittest.h>
#include <gittest/net.h>

int startserv() {
	int r = 0;

	confmap_t ServKeyVal;

	sp<FullConnectionClient> FcsServ;

	if (!!(r = aux_config_read("../data/", "gittest_config_serv.conf", &ServKeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_full_create_connection_server(ServKeyVal, &FcsServ)))
		GS_GOTO_CLEAN();

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
