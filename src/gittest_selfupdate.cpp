#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstdint>

#include <string>

#include <gittest/misc.h>
#include <gittest/gittest.h>
#include <gittest/net.h>
#include <gittest/log.h>

#include <gittest/gittest_selfupdate.h>

int aux_config_read_interpret_relative_current_executable(
	const char *ExpectedLocation, const char *ExpectedName, std::map<std::string, std::string> *oKeyVal)
{
	size_t string_len_arbitrary_max = 2048;

	size_t LenExpectedLocation = 0;

	size_t LenPath = 0;
	char PathBuf[512];

	if ((LenExpectedLocation = strnlen(ExpectedLocation, 2048)) == string_len_arbitrary_max)
		return 1;

	if (!!(gs_build_path_interpret_relative_current_executable(
		ExpectedLocation, LenExpectedLocation, PathBuf, sizeof PathBuf, &LenPath)))
	{
		return 1;
	}

	return aux_config_read(PathBuf, ExpectedName, oKeyVal);
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
		oVal->swap(std::string(PathBuf, LenPath));

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

	if (!!(r = aux_selfupdate_basic("localhost", FileNameCurrentBuf, &HaveUpdate, &BufferUpdate)))
		GS_GOTO_CLEAN_L(E, S, "failure obtaining the update");

	GS_LOG(I, PF, "have_update=[%d]", (int)HaveUpdate);

	if (HaveUpdate) {
		if (!!(r = aux_selfupdate_create_child(FileNameChildBuf, LenFileNameChild, (uint8_t *)BufferUpdate.data(), BufferUpdate.size())))
			GS_GOTO_CLEAN();

		if (!!(r = aux_selfupdate_fork_and_quit(FileNameChildBuf, LenFileNameChild)))
			GS_GOTO_CLEAN();
	}

	if (oHaveUpdateShouldQuit)
		*oHaveUpdateShouldQuit = HaveUpdate;

clean:

	return r;
}

int aux_selfupdate_main_mode_child(
	const char *ArgvHandleSerialized, size_t ArgvHandleSerializedSize,
	const char *ArgvParentFileName, size_t ArgvParentFileNameSize,
	const char *ArgvChildFileName, size_t ArgvChildFileNameSize)
{
	int r = 0;

	if (!!(r = aux_selfupdate_overwrite_parent(
		ArgvHandleSerialized, ArgvHandleSerializedSize,
		ArgvParentFileName, ArgvParentFileNameSize,
		ArgvChildFileName, ArgvChildFileNameSize)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_selfupdate_main_mode_main() {
	int r = 0;

	confmap_t KeyVal;

	GsAuxConfigCommonVars CommonVars = {};

	std::string ConfServHostName;
	uint32_t ConfServPort = 0;
	std::string ConfRefNameSelfUpdate;
	std::string ConfRefNameMain;
	std::string ConfRepoMainOpenPath;

	sp<FullConnectionClient> FcsClnt;

	if (!!(r = aux_config_read_interpret_relative_current_executable("../data/", "gittest_config_serv.conf", &KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_get_common_vars(KeyVal, &CommonVars)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_full_create_connection_client(
		CommonVars.ServPort,
		CommonVars.ServHostNameBuf, CommonVars.LenServHostName,
		CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain,
		/* MasterUpdate is the Main repository for client */
		CommonVars.RepoMasterUpdatePathBuf, CommonVars.LenRepoMasterUpdatePath,
		&FcsClnt)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_selfupdate_main(int argc, char **argv, const char *DefVerSub, uint32_t *oHaveUpdateShouldQuit) {
	int r = 0;

	uint32_t HaveUpdateShouldQuit = 0;

	GS_LOG(I, S, "start");

	if (argc < 2)
		GS_ERR_NO_CLEAN_L(0, I, PF, "no update done ([argc=%d])", argc);

	if (strcmp(argv[1], GS_SELFUPDATE_ARG_UPDATEMODE) != 0)
		GS_ERR_NO_CLEAN_L(0, I, PF, "no update done ([arg=%s])", argv[1]);

	if (argc < 3)
		GS_ERR_CLEAN_L(1, I, PF, "args ([argc=%d])", argc);

	if (strcmp(argv[2], GS_SELFUPDATE_ARG_PARENT) == 0) {
		GS_LOG(I, S, "parent start");
		if (argc != 3)
			GS_ERR_CLEAN(1);
		if (!!(r = aux_selfupdate_main_mode_parent(&HaveUpdateShouldQuit)))
			GS_GOTO_CLEAN();
		GS_LOG(I, PF, "parent end [HaveUpdateShouldQuit = %d]", (int)HaveUpdateShouldQuit);
		if (HaveUpdateShouldQuit)
			GS_ERR_NO_CLEAN(0);
	} else if (strcmp(argv[2], GS_SELFUPDATE_ARG_CHILD) == 0) {
		GS_LOG(I, S, "chld start");
		if (argc != 6)
			GS_ERR_CLEAN_L(1, I, PF, "args ([argc=%d])", argc);
		const size_t ArgvHandleSerializedSize = strlen(argv[3]) + 1;
		const size_t ArgvParentFileNameSize = strlen(argv[4]) + 1;
		const size_t ArgvChildFileNameSize = strlen(argv[5]) + 1;
		if (!!(r = aux_selfupdate_main_mode_child(
			argv[3], ArgvHandleSerializedSize,
			argv[4], ArgvParentFileNameSize,
			argv[5], ArgvChildFileNameSize)))
		{
			GS_GOTO_CLEAN();
		}
		GS_LOG(I, S, "chld end");
	} else if (strcmp(argv[2], GS_SELFUPDATE_ARG_MAIN) == 0) {
		GS_LOG(I, S, "main start");
		if (argc != 3)
			GS_ERR_CLEAN(1);
		if (!!(r = aux_selfupdate_main_mode_main()))
			GS_GOTO_CLEAN();
	} else if (strcmp(argv[2], GS_SELFUPDATE_ARG_VERSUB) == 0) {
		GS_LOG(I, S, "versub start");
		if (argc != 3)
			GS_ERR_CLEAN(1);
		printf("%s\n", DefVerSub);
	} else {
		GS_LOG(I, PF, "unrecognized argument [%.s]", argv[2]);
		GS_ERR_CLEAN(1);
	}

noclean:
	if (oHaveUpdateShouldQuit)
		*oHaveUpdateShouldQuit = HaveUpdateShouldQuit;

clean:

	return r;
}
