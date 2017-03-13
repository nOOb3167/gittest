#include <stddef.h>

#include <unistd.h>

#include <gittest/misc_nix.h>
#include <gittest/gittest_selfupdate.h>

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
