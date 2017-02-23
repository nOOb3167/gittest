#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstdint>

#include <string>

#include <gittest/net.h>
#include <gittest/log.h>

#include <gittest/gittest_selfupdate.h>

int aux_selfupdate_main_mode_main(uint32_t *oHaveUpdateShouldQuit) {
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

int aux_selfupdate_main(int argc, char **argv, uint32_t *oHaveUpdateShouldQuit) {
	int r = 0;

	uint32_t HaveUpdateShouldQuit = 0;

	GS_LOG(I, S, "start");

	if (argc < 2)
		GS_ERR_NO_CLEAN_L(0, I, PF, "no update done ([argc=%d])", argc);

	if (strcmp(argv[1], GS_SELFUPDATE_ARG_UPDATEMODE) != 0)
		GS_ERR_NO_CLEAN_L(0, I, PF, "no update done ([arg=%s])", argv[1]);

	if (argc < 3)
		GS_ERR_CLEAN_L(1, I, PF, "args ([argc=%d])", argc);

	if (strcmp(argv[2], GS_SELFUPDATE_ARG_MAIN) == 0) {
		GS_LOG(I, S, "main start");
		if (argc != 3)
			GS_ERR_CLEAN(1);
		if (!!(r = aux_selfupdate_main_mode_main(&HaveUpdateShouldQuit)))
			GS_GOTO_CLEAN();
		GS_LOG(I, PF, "main end [HaveUpdateShouldQuit = %d]", (int)HaveUpdateShouldQuit);
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
	}

noclean:
	if (oHaveUpdateShouldQuit)
		*oHaveUpdateShouldQuit = HaveUpdateShouldQuit;

clean:

	return r;
}