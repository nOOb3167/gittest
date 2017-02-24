#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#include <cstddef>
#include <cstdint>

#include <windows.h>

#include <gittest/gittest_selfupdate.h> // gs_get_current_executable_filename
#include <gittest/misc.h> // gs_build_modified_filename

#include <gittest/log.h>

#define GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_DATA 0x429d83ff

#define GS_ARBITRARY_LOG_DUMP_FILE_LIMIT_BYTES 10 * 1024 * 1024 /* 10MB */

/* http://stackoverflow.com/questions/1394250/detect-program-termination-c-windows/1400395#1400395 */

/* NOTE: avoid using logging functions such as GS_GOTO_CLEAN() inside crash handler ! */

int gs_log_win_open_dump_file(
	const char *LogFileNameBuf, size_t LenLogFileName,
	const char *ExpectedSuffixBuf, size_t LenExpectedSuffix,
	HANDLE *ohLogFile);

struct GsLogCrashHandlerDumpData { uint32_t Tripwire; HANDLE hLogFile; size_t MaxWritePos; size_t CurrentWritePos; };
int gs_log_crash_handler_dump_cb(void *ctx, const char *d, int64_t l);
int gs_log_crash_handler_dump_global_log_list();

LONG WINAPI gs_log_crash_handler_unhandled_exception_filter_(struct _EXCEPTION_POINTERS *ExceptionInfo);

int gs_log_win_open_dump_file(
	const char *LogFileNameBuf, size_t LenLogFileName,
	const char *ExpectedSuffixBuf, size_t LenExpectedSuffix,
	HANDLE *ohLogFile)
{
	int r = 0;

	HANDLE hLogFile = INVALID_HANDLE_VALUE;

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

	if ((hLogFile = CreateFile(
		LogFileNameBuf,
		GENERIC_WRITE,
		FILE_SHARE_DELETE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL)) == INVALID_HANDLE_VALUE)
	{
		r = 1; goto clean;
	}

	if (ohLogFile)
		*ohLogFile = hLogFile;

clean:
	if (!!r) {
		if (hLogFile != INVALID_HANDLE_VALUE)
			CloseHandle(hLogFile);
	}

	return r;
}

int gs_log_crash_handler_dump_cb(void *ctx, const char *d, int64_t l) {
	GsLogCrashHandlerDumpData *Data = (GsLogCrashHandlerDumpData *)ctx;

	DWORD NumToWrite = (DWORD) l;
	DWORD NumberOfBytesWritten = 0;
	BOOL Ok = 0;

	if (Data->Tripwire != GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_DATA)
		return 1;

	Data->CurrentWritePos += NumToWrite;

	/* NOTE: return zero - if over limit, just avoid writing anything */
	if (Data->CurrentWritePos > Data->MaxWritePos)
		return 0;

	if (!(Ok = WriteFile(Data->hLogFile, d, NumToWrite, &NumberOfBytesWritten, NULL)))
		return 1;

	if (NumberOfBytesWritten != l)
		return 1;

	return 0;
}

int gs_log_crash_handler_dump_global_log_list() {
	int r = 0;

	size_t LenCurrentFileName = 0;
	char CurrentFileNameBuf[512];
	
	size_t LenLogFileName = 0;
	char LogFileNameBuf[512];

	HANDLE hLogFile = INVALID_HANDLE_VALUE;

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

	printf("Dumping Logs To: [%.*s]\n", (int)LenLogFileName, LogFileNameBuf);

	if (!!(r = gs_log_win_open_dump_file(
		LogFileNameBuf, LenLogFileName,
		GS_LOG_STR_EXPECTED_SUFFIX, strlen(GS_LOG_STR_EXPECTED_SUFFIX),
		&hLogFile)))
	{
		goto clean;
	}

	{
		GsLogCrashHandlerDumpData Data = {};
		Data.Tripwire = GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_DATA;
		Data.hLogFile = hLogFile;
		Data.MaxWritePos = GS_ARBITRARY_LOG_DUMP_FILE_LIMIT_BYTES;
		Data.CurrentWritePos = 0;

		if (!!(r = gs_log_list_dump_all_lowlevel(GS_LOG_LIST_GLOBAL_NAME, &Data, gs_log_crash_handler_dump_cb)))
			goto clean;
	}

clean:
	if (hLogFile != INVALID_HANDLE_VALUE)
		CloseHandle(hLogFile);

	return r;
}

LONG WINAPI gs_log_crash_handler_unhandled_exception_filter_(struct _EXCEPTION_POINTERS *ExceptionInfo) {
	//DebugBreak();
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
