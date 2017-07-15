#include <stddef.h>
#include <string.h>

#include <sstream>

#include <unistd.h>
#include <sys/types.h>

#include <gittest/misc.h>
#include <gittest/filesys.h>
#include <gittest/filesys_nix.h>
#include <gittest/gittest_selfupdate.h>

int gs_nix_build_parent_command_line_mode_main(
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oParentArgvUnifiedBuf, size_t ParentArgvUnifiedBufSize, size_t *oLenParentArgvUnified,
	char **ioArgvPtrs, size_t ArgvPtrsSize, size_t *oLenArgvPtrs);

int gs_nix_build_child_command_line(
	const char *ChildFileNameBuf, size_t LenChildFileName,
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oChildArgvUnifiedBuf, size_t ChildArgvUnifiedBufSize, size_t *oLenChildArgvUnified,
	char **ioArgvPtrs, size_t ArgvPtrsSize, size_t *oLenArgvPtrs);

int aux_nix_selfupdate_overwrite_parent(
	const char *ArgvParentFileName, size_t LenArgvParentFileName,
	const char *ArgvChildFileName, size_t LenArgvChildFileName);

int aux_nix_selfupdate_main_mode_child(
	const char *ArgvParentFileName, size_t LenArgvParentFileName,
	const char *ArgvChildFileName, size_t LenArgvChildFileName);


int gs_nix_build_parent_command_line_mode_main(
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oParentArgvUnifiedBuf, size_t ParentArgvUnifiedBufSize, size_t *oLenParentArgvUnified,
	char **ioArgvPtrs, size_t ArgvPtrsSize, size_t *oLenArgvPtrs)
{
	/* construct the command line in execv(3) format. (argv with null sentinel)
	*  @param ArgvPtrsSize size in elements */

	int r = 0;

	std::string ParentFileName(ParentFileNameBuf, LenParentFileName);

	std::string ArgUpdateMode = GS_SELFUPDATE_ARG_UPDATEMODE;
	std::string ArgMain = GS_SELFUPDATE_ARG_MAIN;

	std::string zero("\0", 1);

	std::stringstream ss;
	std::string out;

	size_t LenArgvPtrs = 3 /*args*/ + 1 /*nullsentinel*/;

	if (ArgvPtrsSize < LenArgvPtrs)
		GS_ERR_CLEAN(1);

	ioArgvPtrs[0] = 0;
	ss << ParentFileName << zero;
	ioArgvPtrs[1] = oParentArgvUnifiedBuf + ss.str().size();
	ss << ArgUpdateMode << zero;
	ioArgvPtrs[2] = oParentArgvUnifiedBuf + ss.str().size();
	ss << ArgMain;
	ioArgvPtrs[3] = NULL;
	out = ss.str();

	if (!!(r = gs_buf_copy_zero_terminate_ex(
		out.c_str(), out.size(),
		oParentArgvUnifiedBuf, ParentArgvUnifiedBufSize, oLenParentArgvUnified)))
	{
		GS_GOTO_CLEAN();
	}

	if (oLenArgvPtrs)
		*oLenArgvPtrs = LenArgvPtrs;

clean:

	return r;
}

int gs_nix_build_child_command_line(
	const char *ChildFileNameBuf, size_t LenChildFileName,
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oChildArgvUnifiedBuf, size_t ChildArgvUnifiedBufSize, size_t *oLenChildArgvUnified,
	char **ioArgvPtrs, size_t ArgvPtrsSize, size_t *oLenArgvPtrs)
{
	/* @param ArgvPtrsSize size in elements */
	int r = 0;

	std::string ChildFileName(ChildFileNameBuf, LenChildFileName);
	std::string ParentFileName(ParentFileNameBuf, LenParentFileName);

	std::string ArgUpdateMode = GS_SELFUPDATE_ARG_UPDATEMODE;
	std::string ArgChild = GS_SELFUPDATE_ARG_CHILD;

	std::string zero("\0", 1);

	std::stringstream ss;
	std::string out;

	size_t LenArgvPtrs = 5 /*args*/ + 1 /*nullsentinel*/;

	if (ArgvPtrsSize < LenArgvPtrs)
		GS_ERR_CLEAN(1);

	ioArgvPtrs[0] = 0;
	ss << ChildFileName << zero;
	ioArgvPtrs[1] = oChildArgvUnifiedBuf + ss.str().size();
	ss << ArgUpdateMode << zero;
	ioArgvPtrs[2] = oChildArgvUnifiedBuf + ss.str().size();
	ss << ArgChild << zero;
	ioArgvPtrs[3] = oChildArgvUnifiedBuf + ss.str().size();
	ss << ParentFileName << zero;
	ioArgvPtrs[4] = oChildArgvUnifiedBuf + ss.str().size();
	ss << ChildFileName;
	ioArgvPtrs[5] = NULL;
	out = ss.str();

	if (!!(r = gs_buf_copy_zero_terminate_ex(
		out.c_str(), out.size(),
		oChildArgvUnifiedBuf, ChildArgvUnifiedBufSize, oLenChildArgvUnified)))
	{
		GS_GOTO_CLEAN();
	}

	if (oLenArgvPtrs)
		*oLenArgvPtrs = LenArgvPtrs;

clean:

	return r;
}

int aux_nix_selfupdate_overwrite_parent(
	const char *ArgvParentFileName, size_t LenArgvParentFileName,
	const char *ArgvChildFileName, size_t LenArgvChildFileName)
{
	/* http://stackoverflow.com/questions/1712033/replacing-a-running-executable-in-linux/1712109#1712109 */

	int r = 0;

	pid_t Pid = getppid();

	GS_LOG(I, PF,
		"NOT waiting on parent process id because WHY would we need that feature, eh [id=[%llX]]",
		(long long)Pid);

	GS_LOG(I, PF, "moving [src=[%.*s], dst=[%.*s]]",
		LenArgvChildFileName, ArgvChildFileName,
		LenArgvParentFileName, ArgvParentFileName);

	if (!!(r = gs_nix_rename_wrapper(
		ArgvChildFileName, LenArgvChildFileName,
		ArgvParentFileName, LenArgvParentFileName)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;

}

int aux_nix_selfupdate_main_mode_child(
	const char *ArgvParentFileName, size_t LenArgvParentFileName,
	const char *ArgvChildFileName, size_t LenArgvChildFileName)
{
	int r = 0;

	// FIXME: are overwriting the parent and forking it free of race conditions?

	if (!!(r = aux_nix_selfupdate_overwrite_parent(
		ArgvParentFileName, LenArgvParentFileName,
		ArgvChildFileName, LenArgvChildFileName)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_selfupdate_fork_parent_mode_main_and_quit(ArgvParentFileName, LenArgvParentFileName)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int aux_selfupdate_create_child(
	const char *FileNameChildBuf, size_t LenFileNameChild,
	uint8_t *BufferUpdateData, uint32_t BufferUpdateSize)
{
	int r = 0;

	char *BufferUpdateDataC = (char *)BufferUpdateData;

	int fdChildFile = -1;

	// FIXME: it would be nicer to create a temporary file and move it over
	//   instead of writing the file directly.

	if (!!(r = gs_nix_open_mask_rwx(FileNameChildBuf, LenFileNameChild, &fdChildFile)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_nix_write_wrapper(fdChildFile, BufferUpdateDataC, BufferUpdateSize)))
		GS_GOTO_CLEAN();

clean:
	gs_nix_close_wrapper_noerr(fdChildFile);

	return r;
}

int aux_selfupdate_fork_parent_mode_main_and_quit(
	const char *FileNameParentBuf, size_t LenFileNameParent)
{
	int r = 0;

	size_t LenParentArgvUnified = 0;
	char ParentArgvUnifiedBuf[1024];
	size_t LenArgvPtrs = 0;
	char *ArgvPtrs[64] = {};
	size_t ArgvPtrsSize = sizeof ArgvPtrs / sizeof *ArgvPtrs;

	GS_LOG(I, PF, "(re-)starting parent process [name=[%.*s]]", (int)LenFileNameParent, FileNameParentBuf);

	if (!!(r = gs_nix_build_parent_command_line_mode_main(
		FileNameParentBuf, LenFileNameParent,
		ParentArgvUnifiedBuf, sizeof ParentArgvUnifiedBuf, &LenParentArgvUnified,
		ArgvPtrs, ArgvPtrsSize, &LenArgvPtrs)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_nix_fork_exec(
		ParentArgvUnifiedBuf, LenParentArgvUnified,
		ArgvPtrs, &LenArgvPtrs)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_selfupdate_fork_child_and_quit(
	const char *FileNameChildBuf, size_t LenFileNameChild)
{
	int r = 0;

	size_t LenParentFileName = 0;
	char ParentFileNameBuf[512] = {};

	size_t LenChildArgvUnified = 0;
	char ChildArgvUnifiedBuf[1024] = {};

	size_t LenArgvPtrs = 0;
	char *ArgvPtrs[64] = {};
	size_t ArgvPtrsSize = sizeof ArgvPtrs / sizeof *ArgvPtrs;

	GS_LOG(I, PF, "starting child process [name=[%.*s]]", (int)LenFileNameChild, FileNameChildBuf);

	if (!!(r = gs_get_current_executable_filename(ParentFileNameBuf, sizeof ParentFileNameBuf, &LenParentFileName)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_nix_build_child_command_line(
		FileNameChildBuf, LenFileNameChild,
		ParentFileNameBuf, LenParentFileName,
		ChildArgvUnifiedBuf, sizeof ChildArgvUnifiedBuf, &LenChildArgvUnified,
		ArgvPtrs, ArgvPtrsSize, &LenArgvPtrs)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_nix_fork_exec(
		ChildArgvUnifiedBuf, LenChildArgvUnified,
		ArgvPtrs, &LenArgvPtrs)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_selfupdate_main_prepare_mode_child(int argc, char **argv) {
	int r = 0;

	if (argc != 5)
		GS_ERR_CLEAN_L(1, I, PF, "args ([argc=%d])", argc);

	if (!!(r = aux_nix_selfupdate_main_mode_child(
		argv[4], strlen(argv[4]),
		argv[5], strlen(argv[5]))))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}
