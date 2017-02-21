#include <cstdlib>
#include <cassert>
#include <cstdint>

#include <string>

#include <gittest/log.h>
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

	log_guard<GsLog> log(GsLog::Create());

	uint32_t HaveUpdateShouldQuit = 0;

	if (!!(r = aux_selfupdate_main(argc, argv, &HaveUpdateShouldQuit)))
		GS_GOTO_CLEAN();

	if (HaveUpdateShouldQuit)
		GS_ERR_NO_CLEAN(0);

	//if (!!(r = startclnt()))
	//	GS_GOTO_CLEAN();

noclean:

clean:

	return r;
}

int testlog() {
	int r = 0;

	log_guard<GsLog> log(GsLog::Create());

	GS_LOG(I,S, "hello_outsidescope");
	GS_LOG(I,SZ, "hello", strlen("hello"));
	GS_LOG(I,PF, "hello [%s]", "world");

clean:

	return r;
}

int main(int argc, char **argv) {
	int r = 0;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = testlog()))
		GS_GOTO_CLEAN();

	if (!!(r = startselfupdate(argc, argv)))
		GS_GOTO_CLEAN();

	printf("dummy003\n");

clean:
	if (!!r) {
		assert(0);
	}

	return EXIT_SUCCESS;
}
