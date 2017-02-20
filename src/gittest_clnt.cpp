#include <cstdlib>
#include <cassert>
#include <cstdint>

#include <string>

#include <gittest/gittest.h>
#include <gittest/net.h>
#include <gittest/gittest_selfupdate.h>

int startclnt() {
	int r = 0;

	confmap_t ClntKeyVal;

	sp<FullConnectionClient> FcsClnt;

	if (!!(r = aux_config_read("../data/", "gittest_config_serv.conf", &ClntKeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_full_create_connection_client(ClntKeyVal, &FcsClnt)))
		GS_GOTO_CLEAN();

	for (;;)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

clean:

	return r;
}

int startselfupdate(int argc, char **argv) {
	int r = 0;

	uint32_t HaveUpdateShouldQuit = 0;

	if (!!(r = aux_selfupdate_main(argc, argv, &HaveUpdateShouldQuit)))
		GS_GOTO_CLEAN();

	if (HaveUpdateShouldQuit)
		GS_ERR_NO_CLEAN(0);

	printf("[selfupdate] HaveUpdateShouldQuit = false\n");

	//if (!!(r = startclnt()))
	//	GS_GOTO_CLEAN();

noclean:

clean:

	return r;
}

int main(int argc, char **argv) {
	int r = 0;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = startselfupdate(argc, argv)))
		GS_GOTO_CLEAN();

	/* dummy */

clean:
	if (!!r) {
		assert(0);
	}

	return EXIT_SUCCESS;
}
