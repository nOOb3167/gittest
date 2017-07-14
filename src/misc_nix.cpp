#include <assert.h>
#include <string.h>

#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <fcntl.h>

#include <gittest/misc.h>

#include <gittest/misc_nix.h>

int gs_nix_write_wrapper(int fd, const char *Buf, size_t LenBuf) {
	/* non-reentrant (ex use of errno)
	*  http://stackoverflow.com/questions/1694164/is-errno-thread-safe/1694170#1694170
	*    even if thread local errno makes the function (sans side-effects) thread-safe
	*    receiving signal within signal on same thread would require it to also be reentrant
	*  http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: write and fsync are listed */

	int r = 0;

	size_t count_total = 0;

	if (fd == -1)
		{ r = 1; goto clean; }

	while (count_total < LenBuf) {
		ssize_t count = 0;

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

	while (!!fsync(fd)) {
		/* EROFS / EINVAL expected for a console directed write - just continue */
		if (errno == EROFS || errno == EINVAL)
			break;
		else
			{ r = 1; goto clean; }
	}

clean:

	return r;
}

int gs_nix_write_stdout_wrapper(const char *Buf, size_t LenBuf) {
	return gs_nix_write_wrapper(STDOUT_FILENO, Buf, LenBuf);
}

int gs_nix_unlink_wrapper(const char *FileNameBuf, size_t LenFileName)
{
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: unlink is listed */

	int r = 0;

	if (!!(r = gs_buf_ensure_haszero(FileNameBuf, LenFileName + 1)))
		goto clean;

	if (!!unlink(FileNameBuf))
		{ r = 1; goto clean; }

clean:

	return r;
}

int gs_nix_rename_wrapper(
	const char *SrcFileNameBuf, size_t LenSrcFileName,
	const char *DstFileNameBuf, size_t LenDstFileName)
{
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: rename is listed */

	int r = 0;

	if (!!(r = gs_buf_ensure_haszero(SrcFileNameBuf, LenSrcFileName + 1)))
		goto clean;

	if (!!(r = gs_buf_ensure_haszero(DstFileNameBuf, LenDstFileName + 1)))
		goto clean;

	if (!!rename(SrcFileNameBuf, DstFileNameBuf))
		{ r = 1; goto clean; }

clean:

	return r;
}

int gs_nix_open_tmp_mask_rwx(int *oFdTmpFile) {
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: open is listed */

	int r = 0;

	// FIXME: O_TMPFILE is super magic

	// FIXME: O_TMPFILE introduced as late as Linux 3.11 kernel release
	//   https://kernelnewbies.org/Linux_3.11#head-8be09d59438b31c2a724547838f234cb33c40357
	// FIXME: even worse, O_TMPFILE requires support by the filesystem (as per open(2))

	// FIXME: besides all these O_TMPFILE problems, the way to link it into filesystem is magic
	//   therefore just do not use this function please.

	const char MagicOTmpFileName[] = ".";
	size_t LenMagicOTmpFileName = (sizeof MagicOTmpFileName) - 1;

	/* user read write and execute, add other access flags? */
	if (!!(r = gs_nix_open_wrapper(
		MagicOTmpFileName, LenMagicOTmpFileName,
		O_WRONLY | O_TMPFILE | O_CLOEXEC,
		S_IRUSR | S_IWUSR | S_IXUSR,
		oFdTmpFile)))
	{
		goto clean;
	}

clean:

	return r;
}

int gs_nix_open_mask_rw(
	const char *LogFileNameBuf, size_t LenLogFileName,
	int *oFdLogFile)
{
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: open is listed */

	int r = 0;

	/* user read and write, add other access flags? */
	if (!!(r = gs_nix_open_wrapper(
		LogFileNameBuf, LenLogFileName,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
		S_IRUSR | S_IWUSR,
		oFdLogFile)))
	{
		goto clean;
	}

clean:

	return r;
}

int gs_nix_open_mask_rwx(
	const char *LogFileNameBuf, size_t LenLogFileName,
	int *oFdLogFile)
{
	/* http://man7.org/linux/man-pages/man7/signal.7.html
	*    async-signal-safe functions: open is listed */

	int r = 0;

	/* user read write and execute, add other access flags? */
	if (!!(r = gs_nix_open_wrapper(
		LogFileNameBuf, LenLogFileName,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
		S_IRUSR | S_IWUSR | S_IXUSR,
		oFdLogFile)))
	{
		goto clean;
	}

clean:

	return r;
}

int gs_nix_fork_exec(
	char *ParentArgvUnifiedBuf, size_t LenParentArgvUnified,
	char **ArgvPtrs, size_t *LenArgvPtrs)
{
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: fork is listed.
	*    execvp is actually not but happens in child.
	*    fork actually mentioned to be candidate for de-listing */

	int r = 0;

	pid_t Pid = 0;
	int errExec = -1;

	/* fork can result in EAGAIN, but does not seem resumable */
	if (-1 == (Pid = fork()))
		{ r = 1; goto clean; }

	if (Pid == 0) {
		/* child */
		if (-1 == (errExec = execvp(ArgvPtrs[0], ArgvPtrs)))
			{ r = 1; goto clean; }
	} 
	else {
		/* parent */
		/* nothing - just return */
	}

clean:

	return r;
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
	/* just ignore any errors - return void */

	return;
}

void gs_debug_break() {
	/* NOTE: theoretically can fail with nonzero status */
	raise(SIGTRAP);
}
