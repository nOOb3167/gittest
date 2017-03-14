#include <stddef.h>

#include <unistd.h>

#include <gittest/misc_nix.h>
#include <gittest/gittest_selfupdate.h>

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
