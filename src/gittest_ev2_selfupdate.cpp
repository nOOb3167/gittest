#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <thread>
#include <sstream>

#include <git2.h>

#include <enet/enet.h>

#include <gittest/misc.h>
#include <gittest/config.h>
#include <gittest/log.h>
#include <gittest/filesys.h>
#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h> // GS_STR_PARENT_EXTRA_SUFFIX, GS_SELFUPDATE_ARG_VERSUB
#include <gittest/crank_clnt.h>

#include <gittest/gittest_ev2_test.h>

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

int gs_ev2_selfupdate_reexec()
{
	int r = 0;

	char CurExeBuf[512] = {};
	size_t LenCurExe = 0;

	if (!!(r = gs_get_current_executable_filename(CurExeBuf, sizeof CurExeBuf, &LenCurExe)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_process_start(
		CurExeBuf, LenCurExe,
		CurExeBuf, LenCurExe)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_ev2_selfupdate_dryrun(
	char *RunFileNameBuf, size_t LenRunFileName)
{
	int r = 0;

	std::string RunFileName(RunFileNameBuf, LenRunFileName);

	std::stringstream ss;
	std::string out;

	ss << "\"" << RunFileName << "\"" << " " << std::string(GS_SELFUPDATE_ARG_VERSUB);
	out = ss.str();

	if (!!(r = gs_process_start(
		RunFileName.data(), RunFileName.size(),
		out.data(), out.size())))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_ev2_selfupdate_full(
	struct GsAuxConfigCommonVars CommonVars,
	uint32_t *oHaveUpdateShouldQuit)
{
	int r = 0;

	uint32_t HaveUpdateShouldQuit = 0;

	struct GsEvCtxSelfUpdate *Ctx = NULL;

	uint32_t Code = 0;

	char CurExeBuf[512] = {};
	size_t LenCurExe = 0;

	char TempFileNameBuf[512] = {};
	size_t LenTempFileName = 0;

	char OldFileNameBuf[512] = {};
	size_t LenOldFileName = 0;

	GS_ASSERT(sizeof TempFileNameBuf >= MAX_PATH);

	GS_LOG(I, S, "start");

	if (!!(r = gs_ev2_test_selfupdatemain(CommonVars, &Ctx)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_selfupdate_state_code(Ctx->mState, &Code)))
		GS_GOTO_CLEAN();

	if (Code != GS_SELFUPDATE_STATE_CODE_NEED_NOTHING)
		GS_ERR_NO_CLEAN(0);

	if (!!(r = gs_get_current_executable_filename(CurExeBuf, sizeof CurExeBuf, &LenCurExe)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "executable_filename=[%.*s]", LenCurExe, CurExeBuf);

	if (!!(r = gs_build_modified_filename(
		CurExeBuf, LenCurExe,
		"", 0,
		GS_STR_EXECUTABLE_EXPECTED_EXTENSION, strlen(GS_STR_EXECUTABLE_EXPECTED_EXTENSION),
		GS_STR_PARENT_EXTRA_SUFFIX, strlen(GS_STR_PARENT_EXTRA_SUFFIX),
		GS_STR_EXECUTABLE_EXPECTED_EXTENSION, strlen(GS_STR_EXECUTABLE_EXPECTED_EXTENSION),
		TempFileNameBuf, sizeof TempFileNameBuf, &LenTempFileName)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_build_modified_filename(
		CurExeBuf, LenCurExe,
		"", 0,
		GS_STR_EXECUTABLE_EXPECTED_EXTENSION, strlen(GS_STR_EXECUTABLE_EXPECTED_EXTENSION),
		GS_STR_PARENT_EXTRA_SUFFIX_OLD, strlen(GS_STR_PARENT_EXTRA_SUFFIX_OLD),
		GS_STR_EXECUTABLE_EXPECTED_EXTENSION, strlen(GS_STR_EXECUTABLE_EXPECTED_EXTENSION),
		OldFileNameBuf, sizeof OldFileNameBuf, &LenOldFileName)))
	{
		GS_GOTO_CLEAN();
	}

	GS_LOG(I, PF, "temp_filename=[%.*s]", LenTempFileName, TempFileNameBuf);

	GS_LOG(I, PF, "old_filename=[%.*s]", LenOldFileName, OldFileNameBuf);

	if (!!(r = gs_file_write_frombuffer(
		TempFileNameBuf, LenTempFileName,
		(uint8_t *) Ctx->mState->mBufferUpdate->data(), Ctx->mState->mBufferUpdate->size())))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_ev2_selfupdate_dryrun(TempFileNameBuf, LenTempFileName)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_rename_wrapper(
		CurExeBuf, LenCurExe,
		OldFileNameBuf, LenOldFileName)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_rename_wrapper(
		TempFileNameBuf, LenTempFileName,
		CurExeBuf, LenCurExe)))
	{
		GS_GOTO_CLEAN();
	}

	HaveUpdateShouldQuit = 1;

noclean:
	if (oHaveUpdateShouldQuit)
		*oHaveUpdateShouldQuit = HaveUpdateShouldQuit;

clean:
	GS_DELETE_F(&Ctx, gs_ev_ctx_selfupdate_destroy);

	return r;
}

int gs_ev2_mainupdate_full(
	struct GsAuxConfigCommonVars CommonVars)
{
	int r = 0;

	struct GsEvCtxClnt *Ctx = NULL;

	uint32_t Code = 0;

	if (!!(r = gs_ev2_test_clntmain(CommonVars, &Ctx)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_state_code(Ctx->mClntState, &Code)))
		GS_GOTO_CLEAN();

	if (Code != GS_CLNT_STATE_CODE_NEED_NOTHING)
		GS_ERR_NO_CLEAN(0);

noclean:

clean:
	GS_DELETE_F(&Ctx, gs_ev_ctx_clnt_destroy);

	return r;
}


int main(int argc, char **argv)
{
	int r = 0;

	struct GsConfMap *ConfMap = NULL;
	struct GsAuxConfigCommonVars CommonVars = {};

	std::thread ThreadServ;

	uint32_t HaveUpdateShouldQuit = 0;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	/* NOTE: enet_initialize takes care of calling WSAStartup (needed for LibEvent) */
	if (!!(r = enet_initialize()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_crash_handler_setup()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_create_common_logs()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_read_default_everything(&ConfMap)))
		GS_GOTO_CLEAN();
	gs_debug_break();
	if (!!(r = gs_config_get_common_vars(ConfMap, &CommonVars)))
		GS_GOTO_CLEAN();

	ThreadServ.swap(std::thread(gs_ev2_test_servmain, CommonVars));

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	if (argc == 2 && strcmp(argv[1], GS_SELFUPDATE_ARG_VERSUB))
		GS_ERR_NO_CLEAN(0);

	if (!!(r = gs_ev2_selfupdate_full(CommonVars, &HaveUpdateShouldQuit)))
		GS_GOTO_CLEAN();

	if (HaveUpdateShouldQuit) {
		if (!!(r = gs_ev2_selfupdate_reexec()))
			GS_GOTO_CLEAN();
	}
	else {
		if (!!(r = gs_ev2_mainupdate_full(CommonVars)))
			GS_GOTO_CLEAN();
	}

noclean:

clean:
	if (!!r)
		GS_ASSERT(0);

	GS_DELETE_F(&ConfMap, gs_conf_map_destroy);

	return EXIT_SUCCESS;
}
