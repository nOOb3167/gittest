#include <stddef.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <gittest/gittest_selfupdate.h>
#include <gittest/misc.h>

#include <gittest/log.h>

#define GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_DATA 0x429d83ff

#define GS_ARBITRARY_LOG_DUMP_FILE_LIMIT_BYTES 10 * 1024 * 1024 /* 10MB */

struct GsLogCrashHandlerDumpData { uint32_t Tripwire; int fdLogFile; size_t MaxWritePos; size_t CurrentWritePos; };
int gs_log_nix_crash_handler_dump_cb(void *ctx, const char *d, int64_t l);

int gs_log_nix_open_dump_file(
	const char *LogFileNameBuf, size_t LenLogFileName,
	const char *ExpectedSuffixBuf, size_t LenExpectedSuffix,
	int *oFdLogFile);

int gs_nix_open_wrapper(
	const char *LogFileNameBuf, size_t LenLogFileName,
	int *oFdLogFile);
int gs_nix_write_wrapper(int fd, const char *Buf, size_t LenBuf);
int gs_nix_write_stdout_wrapper(const char *Buf, size_t LenBuf);

void gs_log_nix_crash_handler_sa_sigaction_SIGNAL_HANDLER_(int signo, siginfo_t *info, void *context);
int gs_log_nix_crash_handler_hijack_signal(int signum);
int gs_log_nix_crash_handler_hijack_signals();

int gs_log_nix_crash_handler_dump_cb(void *ctx, const char *d, int64_t l) {
	GsLogCrashHandlerDumpData *Data = (GsLogCrashHandlerDumpData *)ctx;

	int64_t NumToWrite = l;
	int64_t NumberOfBytesWritten = 0;

	if (Data->Tripwire != GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_DATA)
		return 1;

	Data->CurrentWritePos += NumToWrite;

	/* NOTE: return zero - if over limit, just avoid writing anything */
	if (Data->CurrentWritePos > Data->MaxWritePos)
		return 0;

	if (!!gs_nix_write_wrapper(Data->fdLogFile, d, l))
		return 1;

	return 0;
}

int gs_log_nix_open_dump_file(
	const char *LogFileNameBuf, size_t LenLogFileName,
	const char *ExpectedSuffixBuf, size_t LenExpectedSuffix,
	int *oFdLogFile)
{
	/* http://man7.org/linux/man-pages/man7/signal.7.html
	*    async-signal-safe functions: open is listed */

	int r = 0;

	if (LogFileNameBuf[LenLogFileName] != '\0')
		{ r = 1; goto clean; }

	if (ExpectedSuffixBuf[LenExpectedSuffix] != '\0')
		{ r = 1; goto clean; }

	if (LenLogFileName < LenExpectedSuffix)
		{ r = 1; goto clean; }

	if (strncmp(
		ExpectedSuffixBuf,
		LogFileNameBuf + LenLogFileName - LenExpectedSuffix,
		LenExpectedSuffix) != 0)
	{
		r = 1; goto clean;
	}

	if (!!(r = gs_nix_open_wrapper(LogFileNameBuf, LenLogFileName, oFdLogFile)))
		goto clean;

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
		if (fdLogFile != -1)
			close(fdLogFile);
	}

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

void gs_log_nix_crash_handler_sa_sigaction_SIGNAL_HANDLER_(int signo, siginfo_t *info, void *context) {
	/* NOTE: this is a signal handler ie special restricted code path */

	/* not much to do about errors here presumably */
	if (!!gs_log_crash_handler_dump_global_log_list()) {
		const char err[] = "[ERROR] inside crash handler gs_log_nix_crash_handler_sa_sigaction_SIGNAL_HANDLER_\n";
		if (!!gs_nix_write_stdout_wrapper(err, (sizeof err) - 1))
			{ /* dummy */ }
	}
}

int gs_log_nix_crash_handler_hijack_signal(int signum) {
	int r = 0;

	struct sigaction act = {};

	/* act.sa_mask initialized later */
	act.sa_sigaction = gs_log_nix_crash_handler_sa_sigaction;
	act.sa_flags = SA_SIGINFO;

	/* sigfillset aka we request to block all signals during execution of this signal */

	/* https://www.gnu.org/software/libc/manual/html_node/Program-Error-Signals.html
     *   on blocking stop signals
     * If you block or ignore these signals or establish handlers for them that return normally,
     * your program will probably break horribly when such signals happen
     * http://man7.org/linux/man-pages/man2/sigprocmask.2.html
	 *   on blocking stop signals (cont)
     * If SIGBUS, SIGFPE, SIGILL, or SIGSEGV are generated while they are
     * blocked, the result is undefined (unless ...) */

	if (!!(r = sigfillset(&act.sa_mask)))
		goto clean;

	if (!!(r = sigaction(signum, &act, NULL)))
		goto clean;

clean:

	return r;
}

int gs_log_nix_crash_handler_hijack_signals() {
	int r = 0;

	/* https://www.gnu.org/software/libc/manual/html_node/Program-Error-Signals.html */

	if (!!(r = gs_log_nix_crash_handler_hijack_signal(SIGFPE)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_hijack_signal(SIGILL)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_hijack_signal(SIGSEGV)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_hijack_signal(SIGBUS)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_hijack_signal(SIGABRT)))
		goto clean;

	/* https://www.gnu.org/software/libc/manual/html_node/Termination-Signals.html */

	if (!!(r = gs_log_nix_crash_handler_hijack_signal(SIGTERM)))
		goto clean;

	/** should also handle SIGINT ? */

	if (!!(r = gs_log_nix_crash_handler_hijack_signal(SIGQUIT)))
		goto clean;

	/** cannot handle SIGKILL */

	/** should also handle SIGHUP ? */

	/* does not seem other signals are relevant to crash handling */

clean:

	return r;
}

int gs_log_crash_handler_dump_global_log_list() {
	int r = 0;

	size_t LenCurrentFileName = 0;
	char CurrentFileNameBuf[512];

	size_t LenLogFileName = 0;
	char LogFileNameBuf[512];

	int fdLogFile = -1;

	if (!!(r = gs_get_current_executable_filename(CurrentFileNameBuf, sizeof CurrentFileNameBuf, &LenCurrentFileName)))
		goto clean;

	/* FIXME: some of these defines */
	if (!!(r = gs_build_modified_filename(
		CurrentFileNameBuf, LenCurrentFileName,
		GS_STR_PARENT_EXPECTED_EXTENSION, strlen(GS_STR_PARENT_EXPECTED_EXTENSION),
		GS_STR_PARENT_EXPECTED_EXTENSION, strlen(GS_STR_PARENT_EXPECTED_EXTENSION),
		GS_LOG_STR_EXTRA_SUFFIX, strlen(GS_LOG_STR_EXTRA_SUFFIX),
		GS_LOG_STR_EXTRA_EXTENSION, strlen(GS_LOG_STR_EXTRA_EXTENSION),
		LogFileNameBuf, sizeof LogFileNameBuf, &LenLogFileName)))
	{
		goto clean;
	}

	{
		const char DumpingLogsMessage[] = "Dumping Logs\n";
		if (!!(r = gs_nix_write_stdout_wrapper(DumpingLogsMessage, (sizeof DumpingLogsMessage) - 1)))
			goto clean;
	}

	/* FIXME: some of these defines */
	if (!!(r = gs_log_nix_open_dump_file(
		LogFileNameBuf, LenLogFileName,
		GS_LOG_STR_EXPECTED_SUFFIX, strlen(GS_LOG_STR_EXPECTED_SUFFIX),
		&fdLogFile)))
	{
		goto clean;
	}

	{
		GsLogCrashHandlerDumpData Data = {};
		Data.Tripwire = GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_DATA;
		Data.fdLogFile = fdLogFile;
		Data.MaxWritePos = GS_ARBITRARY_LOG_DUMP_FILE_LIMIT_BYTES;
		Data.CurrentWritePos = 0;

		if (!!(r = gs_log_list_dump_all_lowlevel(GS_LOG_LIST_GLOBAL_NAME, &Data, gs_log_nix_crash_handler_dump_cb)))
			goto clean;
	}


clean:

	return r;
}

int gs_log_crash_handler_setup() {
	int r = 0;

	if (!!(r = gs_log_nix_crash_handler_hijack_signals()))
		goto clean;

clean:

	return r;
}
