#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <climits>  // ULLONG_MAX

#include <sstream>

#include <windows.h>
#include <shlwapi.h> // PathAppend etc

#include <gittest/misc.h>
#include <gittest/filesys.h>
#include <gittest/gittest.h>

#include <gittest/gittest_selfupdate.h>

//= win32 parent process =
//http://stackoverflow.com/questions/185254/how-can-a-win32-process-get-the-pid-of-its-parent/558251#558251
//  by pid
//http://stackoverflow.com/questions/185254/how-can-a-win32-process-get-the-pid-of-its-parent/979116#979116
//  by handle duplication
//https://msdn.microsoft.com/en-us/library/windows/desktop/ms684868(v=vs.85).aspx
//  handle duplication mentioned
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms684863(v=vs.85).aspx
//  CREATE_NO_WINDOW - for no console handle

/* for use in GetTempFileName. GetTempFileName uses only 'up to the first three' chars */
#define GS_STR_TEMP_FILE_PREFIX_STRING "gst"

#define GS_STR_CHILD_CONSTANT "CHILD"
#define GS_CHILD_PARENT_TIMEOUT_MS 5000

int gs_win_build_parent_command_line_mode_main(
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oParentCommandLine, size_t ParentCommandLineSize, size_t *oLenParentCommandLine);

int gs_win_build_child_command_line(
	const char *ChildFileNameBuf, size_t LenChildFileName,
	const char *HandleCurrentProcessSerializedBuf, size_t LenHandleCurrentProcessSerialized,
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oChildCommandLine, size_t ChildCommandLineSize, size_t *oLenChildCommandLine);

int aux_win_selfupdate_overwrite_parent(
	const char *ArgvHandleSerialized, size_t LenArgvHandleSerialized,
	const char *ArgvParentFileName, size_t LenArgvParentFileName,
	const char *ArgvChildFileName, size_t LenArgvChildFileName);

int aux_win_selfupdate_main_mode_child(
	const char *ArgvHandleSerialized, size_t LenArgvHandleSerialized,
	const char *ArgvParentFileName, size_t LenArgvParentFileName,
	const char *ArgvChildFileName, size_t LenArgvChildFileName);


void gs_close_handle(HANDLE handle) {
	if (handle)
		if (!CloseHandle(handle))
			GS_ASSERT(0);
}

int gs_serialize_windows_process_handle(HANDLE handle, char *ioBuf, size_t BufSize) {
	/* serialize a HANDLE value into a hexadecimal number string */
	int r = 0;

	GS_ASSERT(sizeof(long long) >= sizeof(HANDLE));

	unsigned long long lluHandle = (unsigned long long) handle;

	// NOTE: _snprintf, microsoft's bastardized version
	int len = _snprintf(ioBuf, BufSize, "%llX", lluHandle);

	if (len <= 0 || len >= BufSize)
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int gs_deserialize_windows_process_handle(
	HANDLE *oHandle,
	const char *BufZeroTermBuf, size_t LenBufZeroTerm)
{
	/* deserialize a hexadecimal number string into a HANDLE value */
	int r = 0;

	HANDLE Handle = NULL;

	GS_ASSERT(sizeof(long long) >= sizeof(HANDLE));

	if (! gs_buf_ensure_haszero(BufZeroTermBuf, LenBufZeroTerm + 1))

	{
		const char *startPtr = BufZeroTermBuf;
		char *endPtr = 0;
		errno = 0;
		unsigned long long lluVal = strtoull(startPtr, &endPtr, 16);
		if (errno == ERANGE && (lluVal == ULLONG_MAX))
			GS_ERR_CLEAN(1);
		if (errno == EINVAL)
			GS_ERR_CLEAN(1);
		if (endPtr >= BufZeroTermBuf + LenBufZeroTerm + 1)
			GS_ERR_CLEAN(1);

		Handle = (HANDLE) lluVal;
	}

	if (oHandle)
		*oHandle = Handle;

clean:

	return r;
}

int gs_win_build_parent_command_line_mode_main(
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oParentCommandLine, size_t ParentCommandLineSize, size_t *oLenParentCommandLine)
{
	int r = 0;

	std::string ParentFileName(ParentFileNameBuf, LenParentFileName);

	std::string ArgUpdateMode = GS_SELFUPDATE_ARG_UPDATEMODE;
	std::string ArgMain = GS_SELFUPDATE_ARG_MAIN;

	std::string quote("\"", 1);
	std::string space(" ", 1);

	std::stringstream ss;
	std::string out;

	ss << quote << ParentFileName << quote << space << ArgUpdateMode << space << ArgMain;
	out = ss.str();

	if (!!(r = gs_buf_copy_zero_terminate_ex(
		out.c_str(), out.size(),
		oParentCommandLine, ParentCommandLineSize, oLenParentCommandLine)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_win_build_child_command_line(
	const char *ChildFileNameBuf, size_t LenChildFileName,
	const char *HandleCurrentProcessSerializedBuf, size_t LenHandleCurrentProcessSerialized,
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oChildCommandLine, size_t ChildCommandLineSize, size_t *oLenChildCommandLine)
{
	int r = 0;

	std::string ChildFileName(ChildFileNameBuf, LenChildFileName);
	std::string HandleCurrentProcessSerialized(HandleCurrentProcessSerializedBuf, LenHandleCurrentProcessSerialized);
	std::string ParentFileName(ParentFileNameBuf, LenParentFileName);

	std::string ArgUpdateMode = GS_SELFUPDATE_ARG_UPDATEMODE;
	std::string ArgChild = GS_SELFUPDATE_ARG_CHILD;

	std::string quote("\"", 1);
	std::string space(" ", 1);

	std::stringstream ss;
	std::string out;

	/* NOTE: ChildFileNameBuf used twice */

	ss << quote << ChildFileName << quote << space
	   << ArgUpdateMode << space
	   << ArgChild << space
	   << HandleCurrentProcessSerialized << space
	   << quote << ParentFileName << quote << space
	   << quote << ChildFileName << quote;
	out = ss.str();

	if (!!(r = gs_buf_copy_zero_terminate_ex(
		out.c_str(), out.size(),
		oChildCommandLine, ChildCommandLineSize, oLenChildCommandLine)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_win_selfupdate_overwrite_parent(
	const char *ArgvHandleSerialized, size_t LenArgvHandleSerialized,
	const char *ArgvParentFileName, size_t LenArgvParentFileName,
	const char *ArgvChildFileName, size_t LenArgvChildFileName)
{
	int r = 0;

	size_t LenChildFileName = 0;
	char ChildFileName[512] = {};

	HANDLE hProcessParent = NULL;
	DWORD Ret = 0;
	BOOL  Ok = 0;

	if (!!(r = gs_get_current_executable_filename(ChildFileName, sizeof ChildFileName, &LenChildFileName)))
		GS_GOTO_CLEAN();

	if (strcmp(ChildFileName, ArgvChildFileName) != 0)
		GS_ERR_CLEAN(1);

	if (!!(r = gs_deserialize_windows_process_handle(&hProcessParent, ArgvHandleSerialized, LenArgvHandleSerialized)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "waiting on deserialized process handle [h=[%llX]]", (long long)hProcessParent);

	// could also be WAIT_TIMEOUT. other values are failure modes.
	if (WAIT_OBJECT_0 != (Ret = WaitForSingleObject(hProcessParent, GS_CHILD_PARENT_TIMEOUT_MS)))
		GS_ERR_CLEAN(1);

	GS_LOG(I, PF, "moving [src=[%.*s], dst=[%.*s]]",
		LenArgvChildFileName, ArgvChildFileName,
		LenArgvParentFileName, ArgvParentFileName);

	if (!(Ok = MoveFileEx(ArgvChildFileName, ArgvParentFileName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)))
		GS_ERR_CLEAN(1);

clean:
	gs_close_handle(hProcessParent);

	return r;

}

int aux_win_selfupdate_main_mode_child(
	const char *ArgvHandleSerialized, size_t LenArgvHandleSerialized,
	const char *ArgvParentFileName, size_t LenArgvParentFileName,
	const char *ArgvChildFileName, size_t LenArgvChildFileName)
{
	int r = 0;

	// FIXME: are overwriting the parent and forking it free of race conditions?

	if (!!(r = aux_win_selfupdate_overwrite_parent(
		ArgvHandleSerialized, LenArgvHandleSerialized,
		ArgvParentFileName, LenArgvParentFileName,
		ArgvChildFileName, LenArgvChildFileName)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = aux_selfupdate_fork_parent_mode_main_and_quit(ArgvParentFileName, LenArgvParentFileName)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_write_temp_file(
	uint8_t *BufferUpdateData, uint32_t BufferUpdateSize,
	char *oTempFileNameBuf, size_t TempFileNameBufSize)
{
	int r = 0;

	char TempPathBuf[512] = {};

	HANDLE hTempFile = INVALID_HANDLE_VALUE;

	LARGE_INTEGER TempFileSizeInitial = {};
	DWORD NumberOfBytesWritten = 0;

	BOOL Ok = 0;
	DWORD Dw = 0;
	UINT Uw = 0;

	if (!(Dw = GetTempPath(sizeof TempPathBuf, TempPathBuf)))
		GS_ERR_CLEAN(1);

	if (Dw >= sizeof TempPathBuf)
		GS_ERR_CLEAN(1);

	/* NOTE: MAX_PATH specific to GetTempFile API */
	if (TempFileNameBufSize < MAX_PATH)
		GS_ERR_CLEAN(1);

	/* get a temporary file name AND create a temporary file */
	if (!(Uw = GetTempFileName(TempPathBuf, GS_STR_TEMP_FILE_PREFIX_STRING, 0, oTempFileNameBuf)))
		GS_ERR_CLEAN(1);

	if (Uw == ERROR_BUFFER_OVERFLOW)
		GS_ERR_CLEAN(1);

	if ((hTempFile = CreateFile(
		oTempFileNameBuf,
		GENERIC_WRITE,
		FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL)) == INVALID_HANDLE_VALUE)
	{
		GS_ERR_CLEAN(1);
	}

	/* check that the temporary file is actually empty */
	if (!(Ok = GetFileSizeEx(hTempFile, &TempFileSizeInitial)))
		GS_ERR_CLEAN(1);
	if (TempFileSizeInitial.LowPart != 0 || TempFileSizeInitial.HighPart != 0)
		GS_ERR_CLEAN(1);

	if (!(Ok = WriteFile(hTempFile, BufferUpdateData, BufferUpdateSize, &NumberOfBytesWritten, NULL)))
		GS_ERR_CLEAN(1);

	if (NumberOfBytesWritten != BufferUpdateSize)
		GS_ERR_CLEAN(1);

clean:
	if (hTempFile != INVALID_HANDLE_VALUE)
		CloseHandle(hTempFile);

	return r;
}

int aux_selfupdate_create_child(
	const char *FileNameChildBuf, size_t LenFileNameChild,
	uint8_t *BufferUpdateData, uint32_t BufferUpdateSize)
{
	int r = 0;

	char TempFileNameBuf[512] = {};

	BOOL Ok = 0;

	GS_ASSERT(sizeof TempFileNameBuf >= MAX_PATH);

	if (!!(r = gs_write_temp_file(BufferUpdateData, BufferUpdateSize, TempFileNameBuf, sizeof TempFileNameBuf)))
		GS_GOTO_CLEAN();

	if (!(Ok = MoveFileEx(TempFileNameBuf, FileNameChildBuf, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)))
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int gs_process_start(
	const char *FileNameParentBuf, size_t LenFileNameParent,
	const char *ParentCommandLineBuf, size_t LenParentCommandLine)
{
	/* create a process and discard all the handles (process and thread handles) */
	int r = 0;

	STARTUPINFO si = {};
	PROCESS_INFORMATION pi = {};
	HANDLE hChildProcess = NULL;
	HANDLE hChildThread = NULL;

	/* https://msdn.microsoft.com/en-us/library/windows/desktop/ms682425(v=vs.85).aspx
	*    32768 actually */
	const size_t MagicCommandLineLenghtLimit = 32767;
	const size_t ReasonableCommandLineLengthLimit = 1024;
	char CommandLineCopyBuf[ReasonableCommandLineLengthLimit];

	BOOL Ok = 0;

	if (LenParentCommandLine >= ReasonableCommandLineLengthLimit)
		GS_ERR_CLEAN(1);

	memcpy(CommandLineCopyBuf, ParentCommandLineBuf, LenParentCommandLine);
	memset(CommandLineCopyBuf + LenParentCommandLine, '\0', 1);

	if (!!(r = gs_file_exist_ensure(FileNameParentBuf, LenFileNameParent)))
		GS_GOTO_CLEAN();

	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	ZeroMemory(&pi, sizeof pi);

	if (!(Ok = CreateProcess(
		FileNameParentBuf,
		CommandLineCopyBuf,
		NULL,
		NULL,
		TRUE,
		0, /* CREATE_NEW_CONSOLE - meh it closes on quit */
		NULL,
		NULL,
		&si,
		&pi)))
	{
		GS_ERR_CLEAN(1);
	}
	hChildProcess = pi.hProcess;
	hChildThread = pi.hThread;

clean:
	gs_close_handle(hChildThread);

	gs_close_handle(hChildProcess);

	return r;
}

int aux_selfupdate_fork_parent_mode_main_and_quit(
	const char *FileNameParentBuf, size_t LenFileNameParent)
{
	int r = 0;

	size_t LenParentCommandLine = 0;
	char ParentCommandLineBuf[1024];

	GS_LOG(I, PF, "(re-)starting parent process [name=[%.*s]]", (int)LenFileNameParent, FileNameParentBuf);

	if (!!(r = gs_win_build_parent_command_line_mode_main(
		FileNameParentBuf, LenFileNameParent,
		ParentCommandLineBuf, sizeof ParentCommandLineBuf, &LenParentCommandLine)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_process_start(
		FileNameParentBuf, LenFileNameParent,
		ParentCommandLineBuf, LenParentCommandLine)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_selfupdate_fork_child_and_quit(
	const char *FileNameChildBuf, size_t LenFileNameChild)
{
	int r = 0;

	size_t LenParentFileName = 0;
	char ParentFileName[512] = {};

	HANDLE hCurrentProcessPseudo = NULL;
	HANDLE hCurrentProcess = NULL;

	size_t LenHandleCurrentProcessSerialized = 0;
	char HandleCurrentProcessSerialized[512] = {};

	size_t LenChildCommandLine = 0;
	char ChildCommandLine[1024];

	BOOL  Ok = 0;

	GS_LOG(I, PF, "starting child process [name=[%.*s]]", (int)LenFileNameChild, FileNameChildBuf);

	if (!!(r = gs_get_current_executable_filename(ParentFileName, sizeof ParentFileName, &LenParentFileName)))
		GS_GOTO_CLEAN();

	hCurrentProcessPseudo = GetCurrentProcess();

	if (!(Ok = DuplicateHandle(hCurrentProcessPseudo, hCurrentProcessPseudo, hCurrentProcessPseudo, &hCurrentProcess, 0, TRUE, DUPLICATE_SAME_ACCESS)))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_serialize_windows_process_handle(hCurrentProcess, HandleCurrentProcessSerialized, sizeof HandleCurrentProcessSerialized)))
		GS_GOTO_CLEAN();

	LenHandleCurrentProcessSerialized = strlen(HandleCurrentProcessSerialized);

	if (!!(r = gs_win_build_child_command_line(
		FileNameChildBuf, LenFileNameChild,
		HandleCurrentProcessSerialized, LenHandleCurrentProcessSerialized,
		ParentFileName, LenParentFileName,
		ChildCommandLine, sizeof ChildCommandLine, &LenChildCommandLine)))
	{
		GS_ERR_CLEAN(1);
	}

	if (!!(r = gs_process_start(
		FileNameChildBuf, LenFileNameChild,
		ChildCommandLine, LenChildCommandLine)))
	{
		GS_ERR_CLEAN(1);
	}

clean:
	gs_close_handle(hCurrentProcess);
	
	// NOTE: no closing the pseudo handle
	//gs_close_handle(hCurrentProcessPseudo);

	return r;
}

int aux_selfupdate_main_prepare_mode_child(int argc, char **argv) {
	int r = 0;

	if (argc != 6)
		GS_ERR_CLEAN_L(1, I, PF, "args ([argc=%d])", argc);

	{
		const size_t LenArgvHandleSerialized = strlen(argv[3]);
		const size_t LenArgvParentFileName = strlen(argv[4]);
		const size_t LenArgvChildFileName = strlen(argv[5]);

		if (!!(r = aux_win_selfupdate_main_mode_child(
			argv[3], LenArgvHandleSerialized,
			argv[4], LenArgvParentFileName,
			argv[5], LenArgvChildFileName)))
		{
			GS_GOTO_CLEAN();
		}
	}

clean:

	return r;
}
