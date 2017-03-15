#include <stddef.h>
#include <string.h>

#include <signal.h>

#include <gittest/gittest_selfupdate.h>
#include <gittest/misc.h>
#include <gittest/misc_nix.h>
#include <gittest/log.h>

#define GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_DATA 0x429d83ff

#define GS_ARBITRARY_LOG_DUMP_FILE_LIMIT_BYTES 10 * 1024 * 1024 /* 10MB */

/**
**
** FIXME: WARNING: global variable. really kind of not want this one.
**   https://www.gnu.org/software/libc/manual/html_node/Termination-in-Handler.html
**   if used, it should be used to implement the recursive delivery check as in above link.
**   also signals are global state to begin with so maybe a global variable is fine.
**   the current solution is to block delivery of all other signals (sigfillset sa_mask)
**   during our crash handling signal handlers - thus hoping to obsolete the variable.
**/
//volatile sig_atomic_t g_gs_log_nix_fatal_error_in_progress = 0;

struct GsLogCrashHandlerDumpData { uint32_t Tripwire; int fdLogFile; size_t MaxWritePos; size_t CurrentWritePos; };
int gs_log_nix_crash_handler_dump_cb(void *ctx, const char *d, int64_t l);

int gs_log_nix_open_dump_file(
	const char *LogFileNameBuf, size_t LenLogFileName,
	const char *ExpectedSuffixBuf, size_t LenExpectedSuffix,
	int *oFdLogFile);

void gs_log_nix_crash_handler_sa_sigaction_SIGNAL_HANDLER_(int signo, siginfo_t *info, void *context);
int gs_log_nix_crash_handler_unhijack_signal_revert_default(int signum);
int gs_log_nix_crash_handler_hijack_signal(int signum);
int gs_log_nix_crash_handler_unhijack_signals_revert_default();
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

	if (!!(r = gs_nix_open_mask_rw(LogFileNameBuf, LenLogFileName, oFdLogFile)))
		goto clean;

clean:

	return r;
}

void gs_log_nix_crash_handler_sa_sigaction_SIGNAL_HANDLER_(int signo, siginfo_t *info, void *context) {
	/* NOTE: this is a signal handler ie special restricted code path */

	/* not much to do about errors here presumably */

	/* https://www.gnu.org/software/libc/manual/html_node/Termination-in-Handler.html
	*  FIXME: above recommends a global variable protocol related to
	*    recursive signal delivery.
	*  the current design instead is to block all other signals (sa_mask sigfillset)
	*  during crash handling signal handler execution. */
	//if (g_gs_log_nix_fatal_error_in_progress)
	//	raise(signo);
	//g_gs_log_nix_fatal_error_in_progress = 1;

	/* importantly, revert all hijacked signals to SIG_DFL action */
	if (!!gs_log_nix_crash_handler_unhijack_signals_revert_default())
		{ /* dummy */ }

	if (!!gs_log_crash_handler_dump_global_log_list()) {
		const char err[] = "[ERROR] inside crash handler gs_log_nix_crash_handler_sa_sigaction_SIGNAL_HANDLER_\n";
		if (!!gs_nix_write_stdout_wrapper(err, (sizeof err) - 1))
			{ /* dummy */ }
	}

	/* signal action should already be SIG_DFL when calling this.
	*  but accidental recursion here would be pretty bad, so make doubly sure. */
	gs_log_nix_crash_handler_unhijack_signal_revert_default(signo);

	raise(signo);
}

int gs_log_nix_crash_handler_unhijack_signal_revert_default(int signum) {
	int r = 0;

	/* https://github.com/plausiblelabs/plcrashreporter/blob/master/Source/PLCrashReporter.m 
	*    search for the signal_handler_callback function */

	struct sigaction act = {};

	/* act.sa_mask initialized later */
	act.sa_handler = SIG_DFL;
	act.sa_flags = 0;

	/* sigemptyset aka we request to not block any signals during execution of signal signum.
	*  https://www.gnu.org/software/libc/manual/html_node/Signals-in-Handler.html#Signals-in-Handler
	*    """sigaction to explicitly specify which signals should be blocked"""
	*    """These signals are in addition to the signal for which the handler was invoked"""
	*  sigemptyset still prevents concurrent delivery of the same signal. */

	if (!!(r = sigemptyset(&act.sa_mask)))
		goto clean;

	if (!!(r = sigaction(signum, &act, NULL)))
		goto clean;

clean:

	return r;
}

int gs_log_nix_crash_handler_hijack_signal(int signum) {
	int r = 0;

	struct sigaction act = {};

	/* act.sa_mask initialized later */
	act.sa_sigaction = gs_log_nix_crash_handler_sa_sigaction_SIGNAL_HANDLER_;
	act.sa_flags = SA_SIGINFO;

	/* sigfillset aka we request to block all signals during execution signal signum */

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

int gs_log_nix_crash_handler_unhijack_signals_revert_default() {
	int r = 0;

	if (!!(r = gs_log_nix_crash_handler_unhijack_signal_revert_default(SIGFPE)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_unhijack_signal_revert_default(SIGILL)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_unhijack_signal_revert_default(SIGSEGV)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_unhijack_signal_revert_default(SIGBUS)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_unhijack_signal_revert_default(SIGABRT)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_unhijack_signal_revert_default(SIGTERM)))
		goto clean;

	if (!!(r = gs_log_nix_crash_handler_unhijack_signal_revert_default(SIGQUIT)))
		goto clean;

clean:

	return r;
}

int gs_log_nix_crash_handler_hijack_signals() {
	int r = 0;

	/* https://github.com/plausiblelabs/plcrashreporter/blob/master/Source/PLCrashReporter.m
	*    see the monitored_signals array for an example of which signals to catch */

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
	gs_nix_close_wrapper_noerr(fdLogFile);

	return r;
}

int gs_log_crash_handler_setup() {
	int r = 0;

	if (!!(r = gs_log_nix_crash_handler_hijack_signals()))
		goto clean;

clean:

	return r;
}
