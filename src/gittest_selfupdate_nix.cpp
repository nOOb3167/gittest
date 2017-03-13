#include <stddef.h>

#include <unistd.h>

#include <gittest/gittest_selfupdate.h>

int gs_nix_path_ensure_absolute(const char *PathBuf, size_t LenPath);

int gs_nix_readlink_wrapper(
	const char *InputPathBuf, size_t LenInputPathBuf,
	char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName);

int gs_nix_path_ensure_absolute(const char *PathBuf, size_t LenPath) {
	if (LenPath < 1)
		return 1;
	return PathBuf[0] != '/';
}

int gs_nix_readlink_wrapper(
	const char *InputPathBuf, size_t LenInputPathBuf,
	char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName)
{
	int r = 0;

	size_t LenFileName = 0;
	ssize_t count = 0;

	errno = 0;

	count = readlink(MAGIC_PROC_PATH_NAME, ioFileNameBuf, FileNameSize);

	// FIXME: should ignore ENOENT ? see proc(5) for /proc/[pid]/exe deleted behaviour.
	if (count == -1 && (errno == ENOENT))
		{ r = 1; goto clean; }
	else if (count == -1)
		{ r = 1; goto clean; }
	else if (count >= FileNameSize)
		{ r = 1; goto clean; }
	/* count >= 0 && count < FileNameSize */

	/* readlink does not zero terminate */
	ioFileNameBuf[count] = '\0';
	LenFileName = count;

	if (oLenFileName)
		*oLenFileName = LenFileName;

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

	/* http://man7.org/linux/man-pages/man7/signal.7.html
	*    async-signal-safe functions: readlink is listed
	*  realpath is readlink's competitor for this task but not listed */

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
