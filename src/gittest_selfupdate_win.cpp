#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */


#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cerrno>
#include <cstring>

#include <windows.h>

#include <gittest/misc.h>

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

#define GS_STR_CHILD_CONSTANT "CHILD"
#define GS_CHILD_PARENT_TIMEOUT_MS 5000

void gs_close_handle(HANDLE handle) {
	if (handle)
		if (!CloseHandle(handle))
			assert(0);
}

int gs_serialize_windows_process_handle(HANDLE handle, char *ioBuf, size_t BufSize) {
	/* serialize a HANDLE value into a hexadecimal number string */
	int r = 0;

	assert(sizeof(long long) >= sizeof(HANDLE));

	unsigned long long lluHandle = (unsigned long long) handle;

	// NOTE: _snprintf, microsoft's bastardized version
	int len = _snprintf(ioBuf, BufSize, "%llX", lluHandle);

	if (len <= 0 || len >= BufSize)
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int gs_deserialize_windows_process_handle(HANDLE *oHandle, const char *BufZeroTerm, size_t BufSize) {
	/* deserialize a hexadecimal number string into a HANDLE value */
	int r = 0;

	HANDLE Handle = NULL;

	assert(sizeof(long long) >= sizeof(HANDLE));

	if (! memchr(BufZeroTerm, '\0', BufSize))
		GS_ERR_CLEAN(1);

	const char *startPtr = BufZeroTerm;
	char *endPtr = 0;
	errno = 0;
	long long lluVal = strtoull(startPtr, &endPtr, 16);
	if (errno = ERANGE && (lluVal == LONG_MIN || lluVal == LONG_MAX))
		GS_ERR_CLEAN(1);
	if (endPtr >= BufZeroTerm + BufSize)
		GS_ERR_CLEAN(1);

	Handle = (HANDLE) lluVal;

	if (oHandle)
		*oHandle = Handle;

clean:

	return r;
}

int gs_get_current_executable_filename(char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName) {
	int r = 0;

	DWORD LenFileName = 0;

	LenFileName = GetModuleFileName(NULL, ioFileNameBuf, FileNameSize);
	if (!(LenFileName != 0 && LenFileName < FileNameSize))
		GS_ERR_CLEAN(1);

	if (oLenFileName)
		*oLenFileName = LenFileName;

clean:

	return r;
}

int gs_build_child_command_line(
	const char *ChildFileNameBuf, size_t LenChildFileName,
	const char *HandleCurrentProcessSerialized, size_t LenHandleCurrentProcessSerialized,
	const char *ParentFileNameBuf, size_t LenParentFileName,
	char *oChildCommandLine, size_t LenChildCommandLine)
{
	int r = 0;

	/* NOTE: ChildFileNameBuf is both pathstr and pathstrchild */

	if (1 /*quote*/ + LenChildFileName /*pathstr*/ + 1 /*quote*/          + 1 /*space*/ +
		strlen(GS_SELFUPDATE_ARG_UPDATEMODE)                            + 1 /*space*/ +
		strlen(GS_SELFUPDATE_ARG_CHILD)                                 + 1 /*space*/ +
		LenHandleCurrentProcessSerialized /*handlestr*/                 + 1 /*space*/ +
		1 /*quote*/ + LenParentFileName /*pathstrparent*/ + 1 /*quote*/ + 1 /*space*/ +
		1 /*quote*/ + LenChildFileName /*pathstrchild*/     + 1 /*quote*/ + 1 /*zero*/
		>= LenChildCommandLine)
	{
		GS_ERR_CLEAN(1);
	}

	char * const PtrArg0 = oChildCommandLine;
	char * const PtrArg1 = PtrArg0 + 1 + LenChildFileName + 1 + 1;
	char * const PtrArg2 = PtrArg1 + strlen(GS_SELFUPDATE_ARG_UPDATEMODE) + 1;
	char * const PtrArg3 = PtrArg2 + strlen(GS_SELFUPDATE_ARG_CHILD) + 1;
	char * const PtrArg4 = PtrArg3 + LenHandleCurrentProcessSerialized + 1;
	char * const PtrArg5 = PtrArg4 + 1 + LenParentFileName + 1 + 1;
	char * const PtrArg6 = PtrArg5 + 1 + LenChildFileName + 1 + 1;
	memset(PtrArg0, '"', 1);
	memcpy(PtrArg0 + 1, ChildFileNameBuf, LenChildFileName);
	memset(PtrArg0 + 1 + LenChildFileName, '"', 1);
	memset(PtrArg0 + 1 + LenChildFileName + 1, ' ', 1);

	memcpy(PtrArg1, GS_SELFUPDATE_ARG_UPDATEMODE, strlen(GS_SELFUPDATE_ARG_UPDATEMODE));
	memset(PtrArg1 + strlen(GS_SELFUPDATE_ARG_UPDATEMODE), ' ', 1);

	memcpy(PtrArg2, GS_SELFUPDATE_ARG_CHILD, strlen(GS_SELFUPDATE_ARG_CHILD));
	memset(PtrArg2 + strlen(GS_SELFUPDATE_ARG_CHILD), ' ', 1);

	memcpy(PtrArg3, HandleCurrentProcessSerialized, LenHandleCurrentProcessSerialized);
	memset(PtrArg3 + LenHandleCurrentProcessSerialized, ' ', 1);

	memset(PtrArg4, '"', 1);
	memcpy(PtrArg4 + 1, ParentFileNameBuf, LenParentFileName);
	memset(PtrArg4 + 1 + LenParentFileName, '"', 1);
	memset(PtrArg4 + 1 + LenParentFileName + 1, ' ', 1);

	memset(PtrArg5, '"', 1);
	memcpy(PtrArg5 + 1, ChildFileNameBuf, LenChildFileName);
	memset(PtrArg5 + 1 + LenChildFileName, '"', 1);
	memset(PtrArg5 + 1 + LenChildFileName + 1, '\0', 1);

clean:

	return r;
}

int gs_build_child_filename(
	char *ParentFileNameBuf, size_t LenParentFileName,
	char *ExpectedSuffix, size_t LenExpectedSuffix,
	char *ExpectedExtension, size_t LenExpectedExtension,
	char *ExtraSuffix, size_t LenExtraSuffix,
	char *ioChildFileNameBuf, size_t ChildFileNameSize, size_t *oLenChildFileName)
{
	// modify example ${path}${expectedsuffix}${expectedextension}
	// into           ${path}${expectedsuffix}${extrasuffix}${expectedextension}'\0'
	// aka c:/blah/gittest.exe -> c:/blah/gittest_helper.exe

	int r = 0;

	if (LenParentFileName < LenExpectedSuffix)
		GS_ERR_CLEAN(1);
	if (LenExpectedSuffix < LenExpectedExtension)
		GS_ERR_CLEAN(1);

	const size_t OffsetStartOfCheck = LenParentFileName - LenExpectedSuffix;
	const size_t OffsetStartOfChange = LenParentFileName - LenExpectedExtension;

	if (strcmp(ExpectedSuffix, ParentFileNameBuf + OffsetStartOfCheck) != 0)
		GS_ERR_CLEAN(1);
	if (strcmp(ExpectedExtension, ParentFileNameBuf + OffsetStartOfChange) != 0)
		GS_ERR_CLEAN(1);

	if (ChildFileNameSize < OffsetStartOfChange + LenExtraSuffix + LenExpectedExtension + 1 /*zero terminator*/)
		GS_ERR_CLEAN(1);

	memcpy(ioChildFileNameBuf, ParentFileNameBuf, OffsetStartOfChange);
	memcpy(ioChildFileNameBuf + OffsetStartOfChange, ExtraSuffix, LenExtraSuffix);
	memcpy(ioChildFileNameBuf + OffsetStartOfChange + LenExtraSuffix, ExpectedExtension, LenExpectedExtension);
	memset(ioChildFileNameBuf + OffsetStartOfChange + LenExtraSuffix + LenExpectedExtension, '\0', 1);

	const size_t LenChildFileName = OffsetStartOfChange + LenExtraSuffix + LenExpectedExtension;

	if (oLenChildFileName)
		*oLenChildFileName = LenChildFileName;

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

	/* gets a temporary file name AND create a temporary file */
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

	assert(sizeof TempFileNameBuf >= MAX_PATH);

	if (!!(r = gs_write_temp_file(BufferUpdateData, BufferUpdateSize, TempFileNameBuf, sizeof TempFileNameBuf)))
		GS_GOTO_CLEAN();

	if (!(Ok = MoveFileEx(TempFileNameBuf, FileNameChildBuf, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)))
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int aux_selfupdate_fork_and_quit(const char *FileNameChildBuf, size_t LenFileNameChild) {
	int r = 0;

	size_t LenParentFileName = 0;
	char ParentFileName[512] = {};

	HANDLE hCurrentProcessPseudo = NULL;
	HANDLE hCurrentProcess = NULL;

	char HandleCurrentProcessSerialized[512] = {};
	char ChildCommandLine[1024];

	STARTUPINFO si = {};
	PROCESS_INFORMATION pi = {};
	HANDLE hChildProcess = NULL;
	HANDLE hChildThread = NULL;

	DWORD Dw = 0;
	BOOL  Ok = 0;

	if (!!(r = gs_get_current_executable_filename(ParentFileName, sizeof ParentFileName, &LenParentFileName)))
		GS_GOTO_CLEAN();

	/* https://blogs.msdn.microsoft.com/oldnewthing/20071023-00/?p=24713/ */
	/* INVALID_FILE_ATTRIBUTES if file does not exist, apparently */
	if (INVALID_FILE_ATTRIBUTES == (Dw = GetFileAttributes(FileNameChildBuf)))
		GS_ERR_CLEAN(1);

	printf("Child Process [%s]\n", FileNameChildBuf);

	hCurrentProcessPseudo = GetCurrentProcess();

	if (!(Ok = DuplicateHandle(hCurrentProcessPseudo, hCurrentProcessPseudo, hCurrentProcessPseudo, &hCurrentProcess, 0, TRUE, DUPLICATE_SAME_ACCESS)))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_serialize_windows_process_handle(hCurrentProcess, HandleCurrentProcessSerialized, sizeof HandleCurrentProcessSerialized)))
		GS_GOTO_CLEAN();

	const size_t LenHandleCurrentProcessSerialized = strlen(HandleCurrentProcessSerialized);

	if (!!(r = gs_build_child_command_line(
		FileNameChildBuf, LenFileNameChild,
		HandleCurrentProcessSerialized, LenHandleCurrentProcessSerialized,
		ParentFileName, LenParentFileName,
		ChildCommandLine, sizeof ChildCommandLine)))
	{
		GS_ERR_CLEAN(1);
	}

	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	ZeroMemory(&pi, sizeof pi);

	if (!(Ok = CreateProcess(
		FileNameChildBuf,
		ChildCommandLine,
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

	gs_close_handle(hCurrentProcess);
	
	// NOTE: no closing the pseudo handle
	//gs_close_handle(hCurrentProcessPseudo);

	return r;
}

int aux_selfupdate_overwrite_parent(
	const char *ArgvHandleSerialized, size_t ArgvHandleSerializedSize,
	const char *ArgvParentFileName, size_t ArgvParentFileNameSize,
	const char *ArgvChildFileName, size_t ArgvChildFileNameSize)
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

	if (!!(r = gs_deserialize_windows_process_handle(&hProcessParent, ArgvHandleSerialized, ArgvHandleSerializedSize)))
		GS_GOTO_CLEAN();

	// could also be WAIT_TIMEOUT. other values are failure modes.
	if (WAIT_OBJECT_0 != (Ret = WaitForSingleObject(hProcessParent, GS_CHILD_PARENT_TIMEOUT_MS)))
		GS_ERR_CLEAN(1);

	if (!(Ok = MoveFileEx(ArgvChildFileName, ArgvParentFileName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)))
		GS_ERR_CLEAN(1);

clean:
	gs_close_handle(hProcessParent);

	return r;

}