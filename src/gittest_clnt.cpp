#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <cstring>

#include <string>

//#include <gittest/config_defs.h>
#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/net2.h>
#include <gittest/gittest_selfupdate.h>

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

int startselfupdate(int argc, char **argv) {
	int r = 0;

	log_guard_t log(GS_LOG_GET("selfup"));

	uint32_t HaveUpdateShouldQuit = 0;

	if (!!(r = aux_selfupdate_main(
		argc, argv,
		GS_CONFIG_DEFS_GITTEST_CLNT_VERSUB,
		&HaveUpdateShouldQuit)))
	{
		GS_GOTO_CLEAN();
	}

	if (HaveUpdateShouldQuit)
		GS_ERR_NO_CLEAN(0);

noclean:

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

int testlog() {
	int r = 0;

	GS_LOG_ADD(gs_log_create_ret("testprefix1"));
	GS_LOG_ADD(gs_log_create_ret("testprefix2"));

	{
		log_guard_t log(GS_LOG_GET("testprefix1"));

		GS_LOG(I,S, "hello_insidescope");
		GS_LOG(I,SZ, "hello", strlen("hello"));
		GS_LOG(I,PF, "hello [%s]", "world");

		log_guard_t log2(GS_LOG_GET("testprefix2"));

		GS_LOG(I, S, "hello_insidescope");
		GS_LOG(I, SZ, "hello", strlen("hello"));
		GS_LOG(I, PF, "hello [%s]", "world");
	}

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

	//if (!!(r = testlog()))
	//	GS_GOTO_CLEAN();

	if (!!(r = startselfupdate(argc, argv)))
		GS_GOTO_CLEAN();

clean:
	if (!!r) {
		GS_ASSERT(0);
	}

	return EXIT_SUCCESS;
}
