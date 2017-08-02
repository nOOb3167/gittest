#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include <thread>  // std::this_thread
#include <string>

#include <gittest/misc.h>
#include <gittest/config.h>
#include <gittest/filesys.h>
#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/net2.h>
#include <gittest/crank_clnt.h>
#include <gittest/crank_selfupdate_basic.h>

#include <gittest/gittest_selfupdate.h>

int gs_build_child_filename(
	const char *ParentFileNameBuf, size_t LenParentFileName,
	const char *ExpectedSuffix, size_t LenExpectedSuffix,
	const char *ExpectedExtension, size_t LenExpectedExtension,
	const char *ExtraSuffix, size_t LenExtraSuffix,
	char *ioChildFileNameBuf, size_t ChildFileNameSize, size_t *oLenChildFileName)
{
	// modify example ${path}${expectedsuffix}${expectedextension}
	// into           ${path}${expectedsuffix}${extrasuffix}${expectedextension}'\0'
	// aka c:/blah/gittest.exe -> c:/blah/gittest_helper.exe

	int r = 0;

	if (!!(r = gs_build_modified_filename(
		ParentFileNameBuf, LenParentFileName,
		ExpectedSuffix, LenExpectedSuffix,
		ExpectedExtension, LenExpectedExtension,
		ExtraSuffix, LenExtraSuffix,
		ExpectedExtension, LenExpectedExtension,
		ioChildFileNameBuf, ChildFileNameSize, oLenChildFileName)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_selfupdate_crash_handler_dump_global_log_list(
	const char *ArgStrBuf, size_t LenArgStr)
{
	bool argstrok = false;
	const char *argprefix = "";
	const char *rsuffix = "_log";

	size_t lencombined = 0;
	char combinedbuf[512] = {};

	/* default suspected malformed ArgStr */
	argstrok = (!gs_buf_ensure_haszero(ArgStrBuf, LenArgStr + 1)) &&
		(LenArgStr >= 2 && ArgStrBuf[0] == '-' && ArgStrBuf[1] == '-');
	argprefix = argstrok ? &ArgStrBuf[2] : "unk";

	lencombined = 1 + strlen(argprefix) + strlen(rsuffix);

	memset(combinedbuf, '_', 1);
	memmove(combinedbuf + 1, argprefix, strlen(argprefix));
	memmove(combinedbuf + 1 + strlen(argprefix), rsuffix, strlen(rsuffix));
	memset(combinedbuf + lencombined, '\0', 1);

	return gs_log_crash_handler_dump_global_log_list_suffix(combinedbuf, lencombined);
}

int aux_selfupdate_main_mode_parent(uint32_t *oHaveUpdateShouldQuit) {
	int r = 0;

	struct GsConfMap *ConfMap = NULL;

	GsAuxConfigCommonVars CommonVars = {};

	size_t LenFileNameCurrent = 0;
	char FileNameCurrentBuf[512] = {};

	size_t LenFileNameChild = 0;
	char FileNameChildBuf[512] = {};

	uint32_t HaveUpdate = 0;
	std::string BufferUpdate;

	if (!!(r = gs_config_read_default_everything(&ConfMap)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_get_common_vars(ConfMap, &CommonVars)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_get_current_executable_filename(FileNameCurrentBuf, sizeof FileNameCurrentBuf, &LenFileNameCurrent)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "executable_filename=[%.*s]", LenFileNameCurrent, FileNameCurrentBuf);

	if (!!(r = gs_build_child_filename(
		FileNameCurrentBuf, LenFileNameCurrent,
		"", 0,
		GS_STR_PARENT_EXPECTED_EXTENSION, strlen(GS_STR_PARENT_EXPECTED_EXTENSION),
		GS_STR_PARENT_EXTRA_SUFFIX, strlen(GS_STR_PARENT_EXTRA_SUFFIX),
		FileNameChildBuf, sizeof FileNameChildBuf, &LenFileNameChild)))
	{
		GS_GOTO_CLEAN();
	}

	GS_LOG(I, PF, "child_filename=[%.*s]", LenFileNameChild, FileNameChildBuf);

	if (!!(r = gs_net_full_create_connection_selfupdate_basic(
		CommonVars.ServPort,
		CommonVars.ServHostNameBuf, CommonVars.LenServHostName,
		FileNameCurrentBuf, LenFileNameCurrent,
		&HaveUpdate,
		&BufferUpdate)))
	{
		GS_GOTO_CLEAN_L(E, S, "failure obtaining the update");
	}

	GS_LOG(I, PF, "have_update=[%d]", (int)HaveUpdate);

	if (HaveUpdate) {
		if (!!(r = aux_selfupdate_create_child(FileNameChildBuf, LenFileNameChild, (uint8_t *)BufferUpdate.data(), BufferUpdate.size())))
			GS_GOTO_CLEAN();

		if (!!(r = aux_selfupdate_fork_child_and_quit(FileNameChildBuf, LenFileNameChild)))
			GS_GOTO_CLEAN();
	}

	if (oHaveUpdateShouldQuit)
		*oHaveUpdateShouldQuit = HaveUpdate;

clean:
	GS_DELETE_F(&ConfMap, gs_conf_map_destroy);

	return r;
}

int aux_selfupdate_main_mode_main() {
	int r = 0;

	struct GsConfMap *ConfMap = NULL;

	GsAuxConfigCommonVars CommonVars = {};

	struct GsFullConnection *FcsClnt = NULL;

	if (!!(r = gs_config_read_default_everything(&ConfMap)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_get_common_vars(ConfMap, &CommonVars)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_net_full_create_connection_client(
		CommonVars.ServPort,
		CommonVars.ServHostNameBuf, CommonVars.LenServHostName,
		CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain,
		/* MasterUpdate is the Main repository for client */
		CommonVars.RepoMasterUpdatePathBuf, CommonVars.LenRepoMasterUpdatePath,
		&FcsClnt)))
	{
		GS_GOTO_CLEAN();
	}

	GS_LOG(I, S, "connection exit waiting");

	if (!!(r = gs_ctrl_con_wait_exited(FcsClnt->mCtrlCon)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "connection exit success");

	if (!!(r = aux_repository_checkout(
		CommonVars.RepoMasterUpdatePathBuf, CommonVars.LenRepoMasterUpdatePath,
		CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain,
		CommonVars.RepoMasterUpdateCheckoutPathBuf, CommonVars.LenRepoMasterUpdateCheckoutPath)))
	{
		GS_GOTO_CLEAN();
	}

	GS_LOG(I, S, "checkout success");

clean:
	if (!!r) {
		GS_DELETE(&FcsClnt, GsFullConnection);
	}

	GS_DELETE_F(&ConfMap, gs_conf_map_destroy);

	return r;
}

int aux_selfupdate_main(int argc, char **argv, const char *DefVerSub, uint32_t *oHaveUpdateShouldQuit) {
	int r = 0;

	const char *argstr = "";

	uint32_t HaveUpdateShouldQuit = 0;

	GS_LOG(I, S, "start");

	if (argc < 2)
		GS_ERR_NO_CLEAN_L(1, I, PF, "no update done ([argc=%d])", argc);

	if (strcmp(argv[1], GS_SELFUPDATE_ARG_UPDATEMODE) != 0)
		GS_ERR_NO_CLEAN_L(1, I, PF, "no update done ([arg=%s])", argv[1]);

	if (argc < 3)
		GS_ERR_CLEAN_L(1, I, PF, "args ([argc=%d])", argc);

	argstr = argv[2];

	if (strcmp(argv[2], GS_SELFUPDATE_ARG_PARENT) == 0)
	{
		GS_LOG(I, S, "parent start");

		if (argc != 3)
			GS_ERR_CLEAN(1);

		if (!!(r = aux_selfupdate_main_mode_parent(&HaveUpdateShouldQuit)))
			GS_GOTO_CLEAN();

		GS_LOG(I, PF, "parent end [HaveUpdateShouldQuit = %d]", (int)HaveUpdateShouldQuit);

		if (HaveUpdateShouldQuit)
			GS_ERR_NO_CLEAN(0);
	}
	else if (strcmp(argv[2], GS_SELFUPDATE_ARG_CHILD) == 0)
	{
		GS_LOG(I, S, "chld start");

		if (!!(r = aux_selfupdate_main_prepare_mode_child(argc, argv)))
			GS_GOTO_CLEAN();

		GS_LOG(I, S, "chld end");
	}
	else if (strcmp(argv[2], GS_SELFUPDATE_ARG_MAIN) == 0)
	{
		GS_LOG(I, S, "main start");

		if (argc != 3)
			GS_ERR_CLEAN(1);

		if (!!(r = aux_selfupdate_main_mode_main()))
			GS_GOTO_CLEAN();

		GS_LOG(I, S, "main end");
	}
	else if (strcmp(argv[2], GS_SELFUPDATE_ARG_VERSUB) == 0)
	{
		GS_LOG(I, S, "versub start");

		if (argc != 3)
			GS_ERR_CLEAN(1);

		printf("%s\n", DefVerSub);
	}
	else
	{
		GS_LOG(I, PF, "unrecognized argument [%.s]", argv[2]);

		GS_ERR_CLEAN(1);
	}

noclean:
	if (oHaveUpdateShouldQuit)
		*oHaveUpdateShouldQuit = HaveUpdateShouldQuit;

clean:
	/* always dump logs. not much to do about errors here though */
	gs_selfupdate_crash_handler_dump_global_log_list(argstr, strlen(argstr));

	return r;
}
