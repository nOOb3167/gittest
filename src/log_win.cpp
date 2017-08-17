#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <windows.h>

#include <gittest/log.h>

#define GS_TRIPWIRE_LOG_CRASH_HANDLER_PRINTF_DATA 0x429d8400

/* http://stackoverflow.com/questions/1394250/detect-program-termination-c-windows/1400395#1400395 */

/* NOTE: avoid using logging functions such as GS_GOTO_CLEAN() inside crash handler ! */

struct GsLogCrashHandlerPrintfData { uint32_t Tripwire; };
int gs_log_crash_handler_printf_cb(void *ctx, const char *d, int64_t l);

LONG WINAPI gs_log_crash_handler_unhandled_exception_filter_(struct _EXCEPTION_POINTERS *ExceptionInfo);

int gs_log_crash_handler_printf_cb(void *ctx, const char *d, int64_t l)
{
	GsLogCrashHandlerPrintfData *Data = (GsLogCrashHandlerPrintfData *) ctx;

	if (Data->Tripwire != GS_TRIPWIRE_LOG_CRASH_HANDLER_PRINTF_DATA)
		return 1;

	printf("%.*s", (int)l, d);

	return 0;
}

LONG WINAPI gs_log_crash_handler_unhandled_exception_filter_(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	//DebugBreak();
	/* not much to do about errors here presumably */
	if (!!gs_log_crash_handler_dump_global_log_list_suffix("", strlen("")))
		printf("[ERROR] inside crash handler gs_log_crash_handler_unhandled_exception_filter_\n");

	return EXCEPTION_CONTINUE_SEARCH;
}

int gs_log_crash_handler_setup()
{
	int r = 0;

	SetUnhandledExceptionFilter(gs_log_crash_handler_unhandled_exception_filter_);

clean:

	return r;
}

void gs_log_crash_handler_printall()
{
	GsLogCrashHandlerPrintfData Data = {};
	Data.Tripwire = GS_TRIPWIRE_LOG_CRASH_HANDLER_PRINTF_DATA;

	if (gs_log_list_dump_all_lowlevel(GS_LOG_LIST_GLOBAL_NAME, &Data, gs_log_crash_handler_printf_cb))
	{ /* dummy */ }
}
