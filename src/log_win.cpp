#include <cstddef>

#include <windows.h>

#include <gittest/gittest_selfupdate.h> // gs_get_current_executable_filename
#include <gittest/misc.h> // gs_build_modified_filename

#include <gittest/log.h>

/* http://stackoverflow.com/questions/1394250/detect-program-termination-c-windows/1400395#1400395 */

/* NOTE: avoid using logging functions such as GS_GOTO_CLEAN() inside crash handler ! */

int gs_log_crash_handler_dump_global_log_list();

LONG WINAPI gs_log_crash_handler_unhandled_exception_filter_(struct _EXCEPTION_POINTERS *ExceptionInfo);

int gs_log_crash_handler_dump_global_log_list() {
	int r = 0;

	size_t LenCurrentFileName = 0;
	char CurrentFileNameBuf[512];
	
	size_t LenLogFileName = 0;
	char LogFileNameBuf[512];

	if (!!(r = gs_get_current_executable_filename(CurrentFileNameBuf, sizeof CurrentFileNameBuf, &LenCurrentFileName)))
		goto clean;

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

clean:

	return r;
}

LONG WINAPI gs_log_crash_handler_unhandled_exception_filter_(struct _EXCEPTION_POINTERS *ExceptionInfo) {
	DebugBreak();
	/* not much to do about errors here presumably */
	if (!!gs_log_crash_handler_dump_global_log_list())
		printf("[ERROR] inside crash handler gs_log_crash_handler_unhandled_exception_filter_ \n");

	return EXCEPTION_CONTINUE_SEARCH;
}

int gs_log_crash_handler_setup() {
	int r = 0;

	SetUnhandledExceptionFilter(gs_log_crash_handler_unhandled_exception_filter_);

clean:

	return r;
}
