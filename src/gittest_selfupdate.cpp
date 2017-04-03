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
#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/net2.h>
#include <gittest/crank_clnt.h>
#include <gittest/crank_selfupdate_basic.h>

#include <gittest/gittest_selfupdate.h>

int aux_config_read_default_everything(std::map<std::string, std::string> *oKeyVal) {
	int r = 0;

	const char LocBuf[] = GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_PATHNAME;
	size_t LenLoc = (sizeof(GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_PATHNAME)) - 1;
	const char NameBuf[] = GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_FILENAME;
	size_t LenName = (sizeof(GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_FILENAME)) - 1;

	if (!!(r = aux_config_read_builtin_or_relative_current_executable(
		LocBuf, LenLoc,
		NameBuf, LenName,
		oKeyVal)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_config_read_builtin(std::map<std::string, std::string> *oKeyVal) {
	int r = 0;

	std::string BufferBuiltinConfig(GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING);
	std::string DecodedConfig;

	if (!!(r = aux_config_decode_hex_pairwise_swapped(BufferBuiltinConfig, &DecodedConfig)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_parse(
		DecodedConfig.data(), DecodedConfig.size(),
		oKeyVal)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_config_read_builtin_or_relative_current_executable(
	const char *ExpectedLocationBuf, size_t LenExpectedLocation,
	const char *ExpectedNameBuf, size_t LenExpectedName,
	std::map<std::string, std::string> *oKeyVal)
{
	int r = 0;

	size_t LenPath = 0;
	char PathBuf[512];

	size_t LenPathFull = 0;
	char PathFullBuf[512];

	size_t PathIsExist = 0;

	if (!!(r = gs_build_path_interpret_relative_current_executable(
		ExpectedLocationBuf, LenExpectedLocation,
		PathBuf, sizeof PathBuf, &LenPath)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_path_append_abs_rel(
		PathBuf, LenPath,
		ExpectedNameBuf, LenExpectedName,
		PathFullBuf, sizeof PathFullBuf, &LenPathFull)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_file_exist(PathFullBuf, LenPathFull, &PathIsExist)))
		GS_GOTO_CLEAN();

	if (PathIsExist) {
		/* read from the file system */

		if (!!(r = aux_config_read_fullpath(
			PathFullBuf, LenPathFull,
			oKeyVal)))
		{
			GS_GOTO_CLEAN();
		}
	}
	else {
		/* use the builtin config (preprocessor definition) */

		if (!!(r = aux_config_read_builtin(oKeyVal)))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_config_key_ex_interpret_relative_current_executable(
	const confmap_t &KeyVal, const char *Key, std::string *oVal)
{

	const confmap_t::const_iterator &it = KeyVal.find(Key);

	size_t LenPath = 0;
	char PathBuf[512];

	if (it == KeyVal.end())
		return 1;

	{
		std::string RawVal = it->second;

		if (!!(gs_build_path_interpret_relative_current_executable(
			RawVal.c_str(), RawVal.size(), PathBuf, sizeof PathBuf, &LenPath)))
		{
			return 1;
		}
	}

	if (oVal)
		*oVal = std::string(PathBuf, LenPath);

	return 0;
}

int aux_config_get_common_vars(
	const confmap_t &KeyVal,
	GsAuxConfigCommonVars *oCommonVars)
{
	int r = 0;

	GsAuxConfigCommonVars CommonVars = {};

	GS_AUX_CONFIG_COMMON_VAR_UINT32_NONUCF(KeyVal, CommonVars, ServPort);
	GS_AUX_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, ServHostName);
	GS_AUX_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, RefNameMain);
	GS_AUX_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, RefNameSelfUpdate);
	GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoMainPath);
	GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoSelfUpdatePath);
	GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoMasterUpdatePath);

	if (oCommonVars)
		*oCommonVars = CommonVars;

clean:

	return r;
}

int gs_build_path_interpret_relative_current_executable(
	const char *PossiblyRelativePathBuf, size_t LenPossiblyRelativePath,
	char *ioPathBuf, size_t PathBufSize, size_t *oLenPathBuf)
{
	int r = 0;

	size_t PossiblyRelativePathIsAbsolute = 0;

	if (!!(r = gs_path_is_absolute(
		PossiblyRelativePathBuf, LenPossiblyRelativePath,
		&PossiblyRelativePathIsAbsolute)))
	{
		GS_GOTO_CLEAN();
	}

	if (PossiblyRelativePathIsAbsolute) {

		if (!!(r = gs_buf_copy_zero_terminate(
			PossiblyRelativePathBuf, LenPossiblyRelativePath,
			ioPathBuf, PathBufSize, oLenPathBuf)))
		{
			GS_GOTO_CLEAN();
		}

	} else {

		if (!!(r = gs_build_current_executable_relative_filename(
			PossiblyRelativePathBuf, LenPossiblyRelativePath,
			ioPathBuf, PathBufSize, oLenPathBuf)))
		{
			GS_GOTO_CLEAN();
		}

	}

clean:

	return r;
}

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

	const size_t OffsetStartOfCheck = LenParentFileName - LenExpectedSuffix;
	const size_t OffsetStartOfChange = LenParentFileName - LenExpectedExtension;
	const size_t LenChildFileName = OffsetStartOfChange + LenExtraSuffix + LenExpectedExtension;

	if (LenParentFileName < LenExpectedSuffix)
		GS_ERR_CLEAN(1);
	if (LenExpectedSuffix < LenExpectedExtension)
		GS_ERR_CLEAN(1);

	if (strcmp(ExpectedSuffix, ParentFileNameBuf + OffsetStartOfCheck) != 0)
		GS_ERR_CLEAN(1);
	if (strcmp(ExpectedExtension, ParentFileNameBuf + OffsetStartOfChange) != 0)
		GS_ERR_CLEAN(1);

	if (ChildFileNameSize < OffsetStartOfChange + LenExtraSuffix + LenExpectedExtension + 1 /*zero terminator*/)
		GS_ERR_CLEAN(1);

	memcpy(ioChildFileNameBuf, ParentFileNameBuf, OffsetStartOfChange);
	memcpy(ioChildFileNameBuf + OffsetStartOfChange, ExtraSuffix, LenExtraSuffix);
	memcpy(ioChildFileNameBuf + OffsetStartOfChange + LenExtraSuffix, ExpectedExtension, LenExpectedExtension);
	memset(ioChildFileNameBuf + OffsetStartOfChange + LenExtraSuffix + LenExpectedExtension, '\0', 1);

	if (oLenChildFileName)
		*oLenChildFileName = LenChildFileName;

clean:

	return r;
}

int aux_selfupdate_main_mode_parent(uint32_t *oHaveUpdateShouldQuit) {
	int r = 0;

	size_t LenFileNameCurrent = 0;
	char FileNameCurrentBuf[512] = {};

	size_t LenFileNameChild = 0;
	char FileNameChildBuf[512] = {};

	uint32_t HaveUpdate = 0;
	std::string BufferUpdate;

	if (!!(r = gs_get_current_executable_filename(FileNameCurrentBuf, sizeof FileNameCurrentBuf, &LenFileNameCurrent)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "executable_filename=[%.*s]", LenFileNameCurrent, FileNameCurrentBuf);

	if (!!(r = gs_build_child_filename(
		FileNameCurrentBuf, LenFileNameCurrent, /* updating the current executable */
		GS_STR_PARENT_EXPECTED_SUFFIX, strlen(GS_STR_PARENT_EXPECTED_SUFFIX),
		GS_STR_PARENT_EXPECTED_EXTENSION, strlen(GS_STR_PARENT_EXPECTED_EXTENSION),
		GS_STR_PARENT_EXTRA_SUFFIX, strlen(GS_STR_PARENT_EXTRA_SUFFIX),
		FileNameChildBuf, sizeof FileNameChildBuf, &LenFileNameChild)))
	{
		GS_GOTO_CLEAN();
	}

	GS_LOG(I, PF, "child_filename=[%.*s]", LenFileNameChild, FileNameChildBuf);

	if (!!(r = gs_net_full_create_connection_selfupdate_basic(
		GS_PORT,    // FIXME: crutch - refactor passing from config
		"localhost", strlen("localhost"),    // FIXME: crutch - refactor passing from config
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

	return r;
}

int aux_selfupdate_main_mode_main() {
	int r = 0;

	confmap_t KeyVal;

	GsAuxConfigCommonVars CommonVars = {};

	sp<GsFullConnection> FcsClnt;

	if (!!(r = aux_config_read_default_everything(&KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_get_common_vars(KeyVal, &CommonVars)))
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

	if (!!(r = gs_ctrl_con_wait_exited(FcsClnt->mCtrlCon.get())))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "connection exit success");

clean:

	return r;
}

int aux_selfupdate_main(int argc, char **argv, const char *DefVerSub, uint32_t *oHaveUpdateShouldQuit) {
	int r = 0;

	uint32_t HaveUpdateShouldQuit = 0;

	GS_LOG(I, S, "start");

	if (argc < 2)
		GS_ERR_NO_CLEAN_L(1, I, PF, "no update done ([argc=%d])", argc);

	if (strcmp(argv[1], GS_SELFUPDATE_ARG_UPDATEMODE) != 0)
		GS_ERR_NO_CLEAN_L(1, I, PF, "no update done ([arg=%s])", argv[1]);

	if (argc < 3)
		GS_ERR_CLEAN_L(1, I, PF, "args ([argc=%d])", argc);

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
	if (!!r) {
		/* always dump logs. not much to do about errors here though */
		gs_log_crash_handler_dump_global_log_list();
	}

	return r;
}
