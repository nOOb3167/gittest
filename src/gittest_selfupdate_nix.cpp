#include <stddef.h>

#include <unistd.h>
#include <sys/types.h>

#include <gittest/misc_nix.h>
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

	const char ArgUpdateModeBuf[] = GS_SELFUPDATE_ARG_UPDATEMODE;
	size_t LenArgUpdateMode = (sizeof ArgUpdateModeBuf) - 1;

	const char ArgMainBuf[] = GS_SELFUPDATE_ARG_MAIN;
	size_t LenArgMain = (sizeof ArgMainBuf) - 1;

	size_t LenParentArgvUnified =
		(LenParentFileName /*pathstr*/ + 1 /*zero*/ +
		 LenArgUpdateMode              + 1 /*zero*/ +
		 LenArgMain                    + 1 /*zero*/);

	size_t LenArgvPtrs = 3 /*args*/ + 1 /*nullsentinel*/;

	if (LenParentArgvUnified > ParentArgvUnifiedBufSize)
		GS_GOTO_CLEAN();

	if (LenArgvPtrs > ArgvPtrsSize)
		GS_GOTO_CLEAN();

	{
		char * const PtrArg0 = oParentArgvUnifiedBuf;
		char * const PtrArg1 = PtrArg0 + LenParentFileName + 1;
		char * const PtrArg2 = PtrArg1 + LenArgUpdateMode + 1;
		char * const PtrArg3 = PtrArg2 + LenArgMain + 1;

		GS_ASSERT(PtrArg3 - PtrArg0 == LenParentArgvUnified);

		memcpy(PtrArg0, ParentFileNameBuf, LenParentFileName);
		memset(PtrArg0 + LenParentFileName, '\0', 1);

		memcpy(PtrArg1, ArgUpdateModeBuf, LenArgUpdateMode);
		memset(PtrArg1 + LenArgUpdateMode, '\0', 1);

		memcpy(PtrArg2, ArgMainBuf, LenArgMain);
		memset(PtrArg2 + LenArgMain, '\0', 1);


		ioArgvPtrs[0] = PtrArg0;
		ioArgvPtrs[1] = PtrArg1;
		ioArgvPtrs[2] = PtrArg2;
		ioArgvPtrs[3] = NULL;
	}

	if (oLenArgvPtrs)
		*oLenArgvPtrs = LenArgvPtrs;

	if (oLenParentArgvUnified)
		*oLenParentArgvUnified = LenParentArgvUnified;

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

	const char ArgUpdateModeBuf[] = GS_SELFUPDATE_ARG_UPDATEMODE;
	size_t LenArgUpdateMode = (sizeof ArgUpdateModeBuf) - 1;

	const char ArgChildBuf[] = GS_SELFUPDATE_ARG_CHILD;
	size_t LenArgChild = (sizeof ArgChildBuf) - 1;

	size_t LenChildArgvUnified =
		(LenChildFileName /*pathstr*/ + 1 /*zero*/ +
		LenArgUpdateMode              + 1 /*zero*/ +
		LenArgChild                   + 1 /*zero*/ +
		LenParentFileName /*pathstrparent*/ + 1 /*zero*/ +
		LenChildFileName /*pathstrchild*/   + 1 /*zero*/);

	size_t LenArgvPtrs = 5 /*args*/ + 1 /*nullsentinel*/;

	if (LenChildArgvUnified >= ChildArgvUnifiedBufSize)
		GS_ERR_CLEAN(1);

	if (LenArgvPtrs > ArgvPtrsSize)
		GS_GOTO_CLEAN();

	{
		char * const PtrArg0 = oChildArgvUnifiedBuf;
		char * const PtrArg1 = PtrArg0 + LenChildFileName + 1;
		char * const PtrArg2 = PtrArg1 + LenArgUpdateMode + 1;
		char * const PtrArg3 = PtrArg2 + LenArgChild + 1;
		char * const PtrArg4 = PtrArg3 + LenParentFileName + 1;
		char * const PtrArg5 = PtrArg4 + LenChildFileName + 1;

		GS_ASSERT(PtrArg5 - PtrArg0 == LenChildArgvUnified);

		memcpy(PtrArg0, ChildFileNameBuf, LenChildFileName);
		memset(PtrArg0 + LenChildFileName, '\0', 1);

		memcpy(PtrArg1, ArgUpdateModeBuf, LenArgUpdateMode);
		memset(PtrArg1 + LenArgUpdateMode, '\0', 1);

		memcpy(PtrArg2, ArgChildBuf, LenArgChild);
		memset(PtrArg2 + LenArgChild, '\0', 1);

		memcpy(PtrArg3, ParentFileNameBuf, LenParentFileName);
		memset(PtrArg3 + LenParentFileName, '\0', 1);

		memcpy(PtrArg4, ChildFileNameBuf, LenChildFileName);
		memset(PtrArg4 + LenChildFileName, '\0', 1);


		ioArgvPtrs[0] = PtrArg0;
		ioArgvPtrs[1] = PtrArg1;
		ioArgvPtrs[2] = PtrArg2;
		ioArgvPtrs[3] = PtrArg3;
		ioArgvPtrs[4] = PtrArg4;
		ioArgvPtrs[5] = NULL;
	}

	if (oLenArgvPtrs)
		*oLenArgvPtrs = LenArgvPtrs;

	if (oLenChildArgvUnified)
		*oLenChildArgvUnified = LenChildArgvUnified;

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

int gs_file_exist_ensure(const char *FileNameBuf, size_t LenFileName) {
	int r = 0;

	if (!!(r = gs_nix_access_wrapper(FileNameBuf, LenFileName, F_OK)))
		goto clean;

clean:

	return r;
}

int gs_get_current_executable_filename(char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName) {
	/* http://man7.org/linux/man-pages/man5/proc.5.html
	*    /proc/[pid]/exe:
	*    If the pathname has been
	*         unlinked, the symbolic link will contain the string
	*         '(deleted)' appended to the original pathname. */
	// FIXME: does move count as unlinking? (probably so)
	//   so if the process has moved itself (during selfupdate)
	//   this call will basically fail (or at least return a weirder name

	int r = 0;

	const char MAGIC_PROC_PATH_NAME[] = "/proc/self/exe";

	if (!!(r = gs_nix_readlink_wrapper(
		MAGIC_PROC_PATH_NAME, (sizeof MAGIC_PROC_PATH_NAME) - 1,
		ioFileNameBuf, FileNameSize, oLenFileName)))
	{
		r = 1; goto clean;
	}

	if (!!(r = gs_nix_path_ensure_absolute(ioFileNameBuf, *oLenFileName)))
		goto clean;

clean:

	return r;
}

int gs_get_current_executable_directory(
	char *ioCurrentExecutableDirBuf, size_t CurrentExecutableDirSize, size_t *oLenCurrentExecutableDir)
{
	int r = 0;

	size_t LenCurrentExecutable = 0;
	char CurrentExecutableBuf[512] = {};

	if (!!(r = gs_get_current_executable_filename(
		CurrentExecutableBuf, sizeof CurrentExecutableBuf, &LenCurrentExecutable)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_nix_absolute_path_directory(
		CurrentExecutableBuf, LenCurrentExecutable,
		ioCurrentExecutableDirBuf, CurrentExecutableDirSize, oLenCurrentExecutableDir)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_build_current_executable_relative_filename(
	const char *RelativeBuf, size_t LenRelative,
	char *ioCombinedBuf, size_t CombinedBufSize, size_t *oLenCombined)
{
	int r = 0;

	size_t LenPathCurrentExecutableDir = 0;
	char PathCurrentExecutableDirBuf[512] = {};
	size_t LenPathModification = 0;
	char PathModificationBuf[512] = {};

	/* get directory */
	if (!!(r = gs_get_current_executable_directory(
		PathCurrentExecutableDirBuf, sizeof PathCurrentExecutableDirBuf, &LenPathCurrentExecutableDir)))
	{
		GS_ERR_CLEAN(1);
	}

	/* ensure relative and append */

	if (!!(r = gs_nix_path_append_abs_rel(
		PathCurrentExecutableDirBuf, LenPathCurrentExecutableDir,
		RelativeBuf, LenRelative,
		PathModificationBuf, sizeof PathModificationBuf, &LenPathModification)))
	{
		GS_GOTO_CLEAN();
	}

	/* SKIP canonicalize into output AND JUST COPY */
	/* no seriously it sucks that ex realpath(3) is not an
	*  async-signal-safe function. */

	if (!!(r = gs_buf_copy_zero_terminate(
		PathModificationBuf, LenPathModification,
		ioCombinedBuf, CombinedBufSize, oLenCombined)))
	{
		GS_GOTO_CLEAN();
	}

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

	const size_t LenArgvParentFileName = strlen(argv[4]);
	const size_t LenArgvChildFileName = strlen(argv[5]);

	if (!!(r = aux_nix_selfupdate_main_mode_child(
		argv[4], LenArgvParentFileName,
		argv[5], LenArgvChildFileName)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}
