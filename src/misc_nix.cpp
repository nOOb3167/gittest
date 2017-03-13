#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <fcntl.h>

#include <gittest/misc.h>

#include <gittest/misc_nix.h>

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

int gs_nix_open_wrapper(
	const char *LogFileNameBuf, size_t LenLogFileName,
	int *oFdLogFile)
{
	int r = 0;

	int fdLogFile = -1;

	int OpenFlags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC;
	mode_t OpenMode = S_IRUSR | S_IWUSR; /* user read and write, add other access flags? */

	while (true) {
		errno = 0;

		/* http://man7.org/linux/man-pages/man2/open.2.html
		*    O_CREAT flag mandates use of the third (mode) argument to open */
		fdLogFile = open(LogFileNameBuf, OpenFlags, OpenMode);

		if (fdLogFile == -1 && (errno == EINTR))
			continue;
		else if (fdLogFile == -1)
			{ r = 1; goto clean; }
		else
			break;
	}

	if (oFdLogFile)
		*oFdLogFile = fdLogFile;

clean:
	if (!!r) {
		/* not much to do about a close error here */
		if (!!(r = gs_nix_close_wrapper(fdLogFile)))
			{ /* dummy */ }
	}

	return r;
}

int gs_nix_close_wrapper(int fd) {
	int r = 0;

	if (fd == -1)
		{ r = 0; goto noclean; }

	while (true) {
		int err = 0;

		errno = 0;

		err = close(fd);

		if (err == -1 && (errno == EINTR))
			continue;
		else if (err == -1)
			{ r = 1; goto clean; }
		else
			break;
	}

noclean:

clean:

	return r;
}

int gs_nix_write_wrapper(int fd, const char *Buf, size_t LenBuf) {
	/* non-reentrant (ex use of errno)
	*  http://stackoverflow.com/questions/1694164/is-errno-thread-safe/1694170#1694170
	*    even if thread local errno makes the function (sans side-effects) thread-safe
	*    receiving signal within signal on same thread would require it to also be reentrant
	*  http://man7.org/linux/man-pages/man7/signal.7.html
	*    async-signal-safe functions: write and fsync are listed */

	int r = 0;

	size_t count_total = 0;

	while (count_total < LenBuf) {
		ssize_t count = 0;

		errno = 0;

		count = write(fd, Buf + count_total, LenBuf - count_total);

		if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
			continue;
		else if (count == -1)
			{ r = 1; goto clean; }

		/* count >= 0 */

		count_total += count;

	}

	/* http://stackoverflow.com/questions/26257171/flush-kernel-buffers-for-stdout/26258312#26258312
	*    probably do not need to fsync the console
	*    but we may or may not be writing to the console */

	do {
		int ok = 0;

		errno = 0;

		ok = fsync(fd);

		/* EROFS / EINVAL expected for a console directed write - just continue */
		if (!!ok && (errno == EROFS || errno == EINVAL))
			break;
		else if (!!ok)
			{ r = 1; goto clean; }

	} while (0);

clean:

	return r;
}

int gs_nix_write_stdout_wrapper(const char *Buf, size_t LenBuf) {
	return gs_nix_write_wrapper(STDOUT_FILENO, Buf, LenBuf);
}

void gs_current_thread_name_set(
	const char *NameBuf,
	size_t LenName)
{
	/* http://stackoverflow.com/questions/778085/how-to-name-a-thread-in-linux/778124#778124 */
	int r = 0;

	/* the limit is magic, see prctl(2) for PR_SET_NAME */
	if (LenName >= 16)
		GS_ERR_CLEAN(1);

	if (!!(r = prctl(PR_SET_NAME, NameBuf, 0, 0, 0)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

void gs_debug_break() {
	/* NOTE: theoretically can fail with nonzero status */
	raise(SIGTRAP);
}
