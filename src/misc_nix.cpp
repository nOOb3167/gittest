#include <assert.h>
#include <string.h>

#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <fcntl.h>

#include <gittest/misc.h>

#include <gittest/misc_nix.h>

/* http://pubs.opengroup.org/onlinepubs/7908799/xsh/systypes.h.html 
*    pid_t is an intergral type */

int gs_nix_open_wrapper(
	const char *LogFileNameBuf, size_t LenLogFileName,
	int OpenFlags, mode_t OpenMode,
	int *oFdLogFile);

int gs_nix_open_wrapper(
	const char *FileNameBuf, size_t LenFileName,
	int OpenFlags, mode_t OpenMode,
	int *oFdFile)
{
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: open is listed */

	/* http://man7.org/linux/man-pages/man2/open.2.html
	*    O_CREAT and O_TMPFILE flags mandate use of the third (mode) argument to open */

	int r = 0;

	int fdFile = -1;

	while (-1 == (fdFile = open(FileNameBuf, OpenFlags, OpenMode))) {
		if (errno == EINTR)
			continue;
		else
			{ r = 1; goto clean; }
	}

	if (oFdFile)
		*oFdFile = fdFile;

clean:
	if (!!r) {
		gs_nix_close_wrapper_noerr(fdFile);
	}

	return r;
}

int gs_nix_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute) {
	int r = 0;

	size_t IsAbsolute = 0;

	if (LenPath < 1)
		{ r = 1; goto clean; }

	IsAbsolute = PathBuf[0] == '/';

	if (oIsAbsolute)
		*oIsAbsolute = IsAbsolute;

clean:

	return r;
}

int gs_nix_path_ensure_absolute(const char *PathBuf, size_t LenPath) {
	int r = 0;

	size_t IsAbsolute = 0;

	if (!!(r = gs_nix_path_is_absolute(PathBuf, LenPath, &IsAbsolute)))
		goto clean;

	if (! IsAbsolute)
		{ r = 1; goto clean; }

clean:

	return r;
}

int gs_nix_path_eat_trailing_slashes(
	const char *InputPathBuf, size_t LenInputPath,
	size_t *oNewLen)
{
	int r = 0;

	if (LenInputPath < 1)
		{ r = 1; goto clean; }

	while ((LenInputPath - 1) && InputPathBuf[(LenInputPath - 1)] == '/')
		LenInputPath--;

	if (oNewLen)
		*oNewLen = LenInputPath;

clean:

	return r;
}

int gs_nix_path_eat_trailing_nonslashes(
	const char *InputPathBuf, size_t LenInputPath,
	size_t *oNewLen)
{
	int r = 0;

	if (LenInputPath < 1)
	{ r = 1; goto clean; }

	while ((LenInputPath - 1) && InputPathBuf[(LenInputPath - 1)] != '/')
		LenInputPath--;

	if (oNewLen)
		*oNewLen = LenInputPath;

clean:

	return r;
}

int gs_nix_path_ensure_starts_with_lump(
	const char *InputPathBuf, size_t LenInputPath)
{
	int r = 0;

	size_t CurPos = 0;
	size_t CurPosMarker = 0;

	while (CurPos < LenInputPath && InputPathBuf[CurPos] == '/')
		CurPos++;

	/* no advance? */
	if ((CurPos - CurPosMarker) == 0)
		{ r = 1; goto clean; }

	CurPos = CurPos;
	CurPosMarker = CurPos;

	while (CurPos < LenInputPath && InputPathBuf[CurPos] != '/')
		CurPos++;

	/* no advance? */
	if ((CurPos - CurPosMarker) == 0)
		{ r = 1; goto clean; }

clean:

	return r;
}

int gs_nix_path_add_trailing_slash_cond_inplace(
	char *DataStart, size_t DataLength, size_t OffsetOnePastEnd, size_t *OffsetOnePastEndNew)
{
	int r = 0;

	size_t Offset = OffsetOnePastEnd - 1;

	/* already ending with slash */
	if (DataStart[Offset++] == '/')
		{ r = 0; goto clean; }

	if (DataLength - Offset < 1 + 1)
		{ r = 1; goto clean; }

	DataStart[Offset++] = '/';  /* 1 */
	DataStart[Offset++] = '\0'; /* 1 */

	Offset--; /* we want to end at the inserted zero */

	if (OffsetOnePastEndNew)
		*OffsetOnePastEndNew = Offset;

clean:

	return r;
}

int gs_nix_path_append_midslashing_inplace(
	const char *ToAddBuf, size_t LenToAdd,
	char *DataStart, size_t DataLength, size_t OffsetOnePastEnd, size_t *OffsetOnePastEndNew)
{
	int r = 0;

	size_t Offset = OffsetOnePastEnd;

	if (!!(r = gs_nix_path_add_trailing_slash_cond_inplace(DataStart, DataLength, Offset, &Offset)))
		goto clean;

	if (DataLength - Offset < LenToAdd + 1)
		{ r = 1; goto clean; }

	memmove(DataStart + Offset, ToAddBuf, LenToAdd); /* LenToAdd */
	Offset += LenToAdd;
	DataStart[Offset++] = '\0'; /* 1 */

	Offset--; /* want to end up at the inserted zero */

	if (OffsetOnePastEndNew)
		*OffsetOnePastEndNew = Offset;

clean:

	return r;
}

int gs_nix_absolute_path_directory(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath)
{
	/* async-signal-safe functions: safe */
	int r = 0;

	size_t LenOutputPath = 0;

	const char OnlySlash[] = "/";

	const char *ToOutputPtr = NULL;
	size_t ToOutputLen = 0;

	/* absolute aka starts with a slash */
	if (!!(r = gs_nix_path_ensure_absolute(InputPathBuf, LenInputPath)))
		goto clean;

	/* eat trailing slashes */

	if (!!(r = gs_nix_path_eat_trailing_slashes(InputPathBuf, LenInputPath, &LenInputPath)))
		goto clean;

	if (LenInputPath > 0) {
		/* because of ensure absolute we know it starts with a slash.
		*  since eat_trailing_slashes did not eat the whole path,
		*  what remains must be of the form "/XXX(/XXX)*" (regex).
		*  there might be redundant slashes. */
		if (!!(r = gs_nix_path_ensure_starts_with_lump(InputPathBuf, LenInputPath)))
			goto clean;
		/* eat an XXX part */
		if (!!(r = gs_nix_path_eat_trailing_nonslashes(InputPathBuf, LenInputPath, &LenInputPath)))
			goto clean;
		/* eat an / part */
		if (!!(r = gs_nix_path_eat_trailing_slashes(InputPathBuf, LenInputPath, &LenInputPath)))
			goto clean;
		/* two possibilities: we were on the last /XXX part or not.
		*  path is now empty or of the form /XXX */
	}

	if (LenInputPath == 0) {
		/* handle the 'path is now empty' possibility: output just /, as per dirname(3) */
		ToOutputPtr = OnlySlash;
		ToOutputLen = (sizeof OnlySlash) - 1;
	} else {
		/* handle the 'path is now of the form /XXX' possibility: output verbatim */
		ToOutputPtr = InputPathBuf;
		ToOutputLen = LenInputPath;
	}

	if (OutputPathBufSize < ToOutputLen + 1)
		{ r = 1; goto clean; }

	memmove(ioOutputPathBuf, ToOutputPtr, ToOutputLen);
	memset(ioOutputPathBuf + ToOutputLen, '\0', 1);

	if (oLenOutputPath)
		*oLenOutputPath = ToOutputLen;

clean:

	return r;
}

int gs_nix_access_wrapper(
	const char *InputPathBuf, size_t LenInpuPath,
	int mode)
{
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: access is listed */

	int r = 0;

	if (!!access(InputPathBuf, mode))
		{ r = 1; goto clean; }

clean:

	return r;
}

int gs_nix_readlink_wrapper(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName)
{
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: readlink is listed
	*  realpath is readlink's competitor for this task but not listed */

	int r = 0;

	size_t LenFileName = 0;
	ssize_t count = 0;

	if (-1 == (count = readlink(InputPathBuf, ioFileNameBuf, FileNameSize)))
		{ r = 1; goto clean; }

	if (count >= FileNameSize)
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

int gs_nix_close_wrapper(int fd) {
	/* http://man7.org/linux/man-pages/man7/signal-safety.7.html
	*    async-signal-safe functions: close is listed */

	int r = 0;

	if (fd == -1)
		{ r = 0; goto noclean; }

	while (!!close(fd)) {
		if (errno == EINTR)
			continue;
		else
			{ r = 1; goto clean; }
	}

noclean:

clean:

	return r;
}

int gs_nix_close_wrapper_noerr(int fd) {
	if (!!gs_nix_close_wrapper(fd))
		{ /* dummy */ }
}

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

int gs_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute) {
	return gs_nix_path_is_absolute(PathBuf, LenPath, oIsAbsolute);
}

int gs_path_append_abs_rel(
	const char *AbsoluteBuf, size_t LenAbsolute,
	const char *RelativeBuf, size_t LenRelative,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath)
{
	int r = 0;

	size_t OutputPathEndOffset = 0;

	size_t AbsoluteIsAbsolute = 0;
	size_t RelativeIsAbsolute = 0;

	if (!!(r = gs_nix_path_is_absolute(AbsoluteBuf, LenAbsolute, &AbsoluteIsAbsolute)))
		goto clean;

	if (!!(r = gs_nix_path_is_absolute(RelativeBuf, LenRelative, &RelativeIsAbsolute)))
		goto clean;

	if ((! AbsoluteIsAbsolute) || (RelativeIsAbsolute))
	{ r = 1; goto clean; }

	/* prep output buffer with absolute path */

	if (!!(r = gs_buf_copy_zero_terminate(
		AbsoluteBuf, LenAbsolute,
		ioOutputPathBuf, OutputPathBufSize, &OutputPathEndOffset)))
	{
		goto clean;
	}

	/* append */

	if (!!(r = gs_nix_path_append_midslashing_inplace(
		RelativeBuf, LenRelative,
		ioOutputPathBuf, OutputPathBufSize, OutputPathEndOffset, &OutputPathEndOffset)))
	{
		goto clean;
	}

	if (!!(r = gs_buf_strnlen(ioOutputPathBuf, OutputPathBufSize, oLenOutputPath)))
		goto clean;

clean:

	return r;
}
