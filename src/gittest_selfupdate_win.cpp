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

//= win32 parent process =
//http://stackoverflow.com/questions/185254/how-can-a-win32-process-get-the-pid-of-its-parent/558251#558251
//  by pid
//http://stackoverflow.com/questions/185254/how-can-a-win32-process-get-the-pid-of-its-parent/979116#979116
//  by handle duplication
//https://msdn.microsoft.com/en-us/library/windows/desktop/ms684868(v=vs.85).aspx
//  handle duplication mentioned
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms684863(v=vs.85).aspx
//  CREATE_NO_WINDOW - for no console handle

#define GS_DBG_CLEAN { assert(0); }

#define GS_ERR_CLEAN(THE_R) { r = (THE_R); GS_DBG_CLEAN; goto clean; }
#define GS_GOTO_CLEAN() { GS_DBG_CLEAN; goto clean; }
#define GS_ERR_CLEANSUB(THE_R) { r = (THE_R); GS_DBG_CLEAN; goto cleansub; }
#define GS_GOTO_CLEANSUB() { GS_DBG_CLEAN; goto cleansub; }

#define GS_STR_CHILD_CONSTANT "CHILD"
#define GS_STR_PARENT_FILENAME_EXPECTED "gittest_selfupdate.exe"
#define GS_STR_CHILD_FILENAME_WANTED "gittest_selfupdate_helper.exe"
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

int gs_deserialize_windows_process_handle(HANDLE *oHandle, char *BufZeroTerm, size_t BufSize) {
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

int gs_build_child_command_line(
	char *FileNameBuf, size_t LenFileNameBuf,
	char *HandleCurrentProcessSerialized, size_t LenHandleCurrentProcessSerialized,
	char *ParentFileNameBuf, size_t LenParentFileName,
	char *oChildCommandLine, size_t LenChildCommandLine)
{
	int r = 0;

	char ChildConstant[] = GS_STR_CHILD_CONSTANT;
	size_t LenChildConstant = strlen(ChildConstant);

	// quote + pathstr + quote + space + CHILD + space + handlestr + zero
	if (1 /*quote*/ + LenFileNameBuf /*pathstr*/ + 1 /*quote*/ + 1 /*space*/ +
		LenChildConstant /*CHILD*/ + 1 /*space*/ +
		LenHandleCurrentProcessSerialized /*handlestr*/ + 1 /*space*/ +
		1 /*quote*/ + LenParentFileName /*pathstrparent*/ + 1 /*quote*/ + 1 /*zero*/
		>= LenChildCommandLine)
	{
		GS_ERR_CLEAN(1);
	}

	char * const PtrArg0 = oChildCommandLine;
	char * const PtrArg1 = PtrArg0 + 1 + LenFileNameBuf + 1 + 1;
	char * const PtrArg2 = PtrArg1 + LenChildConstant + 1;
	char * const PtrArg3 = PtrArg2 + LenHandleCurrentProcessSerialized + 1;
	memset(PtrArg0, '"', 1); /*quote*/
	memcpy(PtrArg0 + 1, FileNameBuf, LenFileNameBuf); /*pathstr*/
	memset(PtrArg0 + 1 + LenFileNameBuf, '"', 1); /*quote*/
	memset(PtrArg0 + 1 + LenFileNameBuf + 1, ' ', 1); /*space*/

	memcpy(PtrArg1, ChildConstant, LenChildConstant); /*CHILD*/
	memset(PtrArg1 + LenChildConstant, ' ', 1); /*space*/

	memcpy(PtrArg2, HandleCurrentProcessSerialized, LenHandleCurrentProcessSerialized); /*handlestr*/
	memset(PtrArg2 + LenHandleCurrentProcessSerialized, ' ', 1); /*space*/

	memset(PtrArg3, '"', 1); /*quote*/
	memcpy(PtrArg3 + 1, ParentFileNameBuf, LenParentFileName); /*pathstrparent*/
	memset(PtrArg3 + 1 + LenParentFileName, '"', 1); /*quote*/
	memset(PtrArg3 + 1 + LenParentFileName + 1, '\0', 1); /*zero*/

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

int gs_build_child_filename(
	char *ParentFileNameBuf, size_t LenParentFileName,
	char *ioChildFileNameBuf, size_t ChildFileNameSize, size_t *oLenChildFileName)
{
	// returned from GetModuleFileName: ${ParentFileNameBuf}${GS_STR_PARENT_FILENAME_EXPECTED}
	//   commonpath: ${ParentFileNameBuf}
	//   childfilename: ${GS_STR_CHILD_FILENAME_WANTED}
	// building: ${commonpath}${childfilename}'\0'
	int r = 0;

	char ExpectedFileName[] = GS_STR_PARENT_FILENAME_EXPECTED;
	size_t LenExpectedFileName = strlen(ExpectedFileName);

	char WantedFileName[] = GS_STR_CHILD_FILENAME_WANTED;
	size_t LenWantedFileName = strlen(WantedFileName);

	if (LenParentFileName < LenExpectedFileName)
		GS_ERR_CLEAN(1);

	const size_t OffsetStartOfChange = LenParentFileName - LenExpectedFileName;

	if (strcmp(ExpectedFileName, ParentFileNameBuf + OffsetStartOfChange) != 0)
		GS_ERR_CLEAN(1);

	if (ChildFileNameSize < OffsetStartOfChange /*commonpath*/ + LenWantedFileName /*childfilename*/ + 1 /*zero*/)
		GS_ERR_CLEAN(1);

	memcpy(ioChildFileNameBuf, ParentFileNameBuf, OffsetStartOfChange);
	memcpy(ioChildFileNameBuf + OffsetStartOfChange, WantedFileName, LenWantedFileName);
	memset(ioChildFileNameBuf + OffsetStartOfChange + LenWantedFileName, '\0', 1);

	const size_t LenChildFileName = OffsetStartOfChange + LenWantedFileName;

	if (oLenChildFileName)
		*oLenChildFileName = LenChildFileName;

clean:

	return r;
}

int selfup() {
	int r = 0;

	size_t LenParentFileName = 0;
	char ParentFileName[512] = {};
	size_t LenChildFileName = 0;
	char ChildFileName[512] = {};

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

	if (!!(r = gs_build_child_filename(ParentFileName, LenParentFileName, ChildFileName, sizeof ChildFileName, &LenChildFileName)))
		GS_GOTO_CLEAN();

	/* https://blogs.msdn.microsoft.com/oldnewthing/20071023-00/?p=24713/ */
	/* INVALID_FILE_ATTRIBUTES if file does not exist, apparently */
	if (INVALID_FILE_ATTRIBUTES == (Dw = GetFileAttributes(ChildFileName)))
		GS_ERR_CLEAN(1);

	printf("Current Process [%s]\n", ChildFileName);

	hCurrentProcessPseudo = GetCurrentProcess();

	if (!(Ok = DuplicateHandle(hCurrentProcessPseudo, hCurrentProcessPseudo, hCurrentProcessPseudo, &hCurrentProcess, 0, TRUE, DUPLICATE_SAME_ACCESS)))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_serialize_windows_process_handle(hCurrentProcess, HandleCurrentProcessSerialized, sizeof HandleCurrentProcessSerialized)))
		GS_GOTO_CLEAN();

	const size_t LenHandleCurrentProcessSerialized = strlen(HandleCurrentProcessSerialized);

	if (!!(r = gs_build_child_command_line(
		ChildFileName, LenChildFileName,
		HandleCurrentProcessSerialized, LenHandleCurrentProcessSerialized,
		ParentFileName, LenParentFileName,
		ChildCommandLine, sizeof ChildCommandLine)))
	{
		GS_ERR_CLEAN(1);
	}

	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	ZeroMemory(&pi, sizeof pi);

	if (!(Ok = CreateProcess(ChildFileName, ChildCommandLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)))
		GS_ERR_CLEAN(1);
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

int childup(
	char *ArgvHandleSerialized, size_t ArgvHandleSerializedSize,
	char *ArgvParentFileName, size_t ArgvParentFileNameSize)
{
	int r = 0;

	size_t LenChildFileName = 0;
	char ChildFileName[512] = {};

	HANDLE hProcessParent = NULL;
	DWORD Ret = 0;
	BOOL  Ok  = 0;

	if (!!(r = gs_get_current_executable_filename(ChildFileName, sizeof ChildFileName, &LenChildFileName)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_deserialize_windows_process_handle(&hProcessParent, ArgvHandleSerialized, ArgvHandleSerializedSize)))
		GS_GOTO_CLEAN();

	// could also be WAIT_TIMEOUT. other values are failure modes.
	if (WAIT_OBJECT_0 != (Ret = WaitForSingleObject(hProcessParent, GS_CHILD_PARENT_TIMEOUT_MS)))
		GS_ERR_CLEAN(1);

	if (!(Ok = MoveFileEx(ChildFileName, ArgvParentFileName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)))
		GS_ERR_CLEAN(1);

clean:
	gs_close_handle(hProcessParent);

	return r;
}

int selfupdate_main(int argc, char **argv) {
	if (argc < 2)
		assert(0);
	
	if (strcmp(argv[1], "DUMMY") == 0) {
		if (!!selfup())
			assert(0);
	} else if (strcmp(argv[1], "CHILD") == 0) {
		if (argc < 4)
			assert(0);
		const size_t ArgvHandleSerializedSize = strlen(argv[2]) + 1;
		const size_t ArgvParentFileNameSize = strlen(argv[3]) + 1;
		if (!!childup(
			argv[2], ArgvHandleSerializedSize,
			argv[3], ArgvParentFileNameSize))
		{
			assert(0);
		}
	} else {
		assert(0);
	}

	return EXIT_SUCCESS;
}
