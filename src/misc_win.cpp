#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */

#include <cstddef>

#include <windows.h>
#include <shlwapi.h> // PathAppend etc

#include <gittest/misc.h>

/* https://gcc.gnu.org/onlinedocs/gcc-4.4.4/gcc/Structure_002dPacking-Pragmas.html
*    pragma pack gcc support */

#pragma pack(push, 8)

typedef struct {
	DWORD dwType;
	LPCSTR szName;
	DWORD dwThreadID;
	DWORD dwFlags;
} THREADNAME_INFO;

#pragma pack(pop)

static EXCEPTION_DISPOSITION NTAPI gs_win_ignore_handler(
	EXCEPTION_RECORD *rec,
	void *frame,
	CONTEXT *ctx,
	void *disp);

int gs_win_path_directory(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath);
int gs_win_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute);
int gs_win_path_canonicalize(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath);

EXCEPTION_DISPOSITION NTAPI gs_win_ignore_handler(
	EXCEPTION_RECORD *rec,
	void *frame,
	CONTEXT *ctx,
	void *disp)
{
	return ExceptionContinueExecution;
}

int gs_win_path_directory(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath)
{
	int r = 0;

	char Drive[_MAX_DRIVE] = {};
	char Dir[_MAX_DIR] = {};
	char FName[_MAX_FNAME] = {};
	char Ext[_MAX_EXT] = {};

	/* http://www.flounder.com/msdn_documentation_errors_and_omissions.htm
	*    see for _splitpath: """no more than this many characters will be written to each buffer""" */
	_splitpath(InputPathBuf, Drive, Dir, FName, Ext);

	if (!!(r = _makepath_s(ioOutputPathBuf, OutputPathBufSize, Drive, Dir, NULL, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_buf_strnlen(ioOutputPathBuf, OutputPathBufSize, oLenOutputPath)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_win_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute)
{
	int r = 0;

	size_t IsAbsolute = false;

	if (!!(r = gs_buf_strnlen(PathBuf, LenPath + 1, NULL)))
		GS_GOTO_CLEAN();

	/* maximum length for PathIsRelative */
	if (LenPath > MAX_PATH)
		GS_ERR_CLEAN(1);

	IsAbsolute = ! PathIsRelative(PathBuf);

	if (oIsAbsolute)
		*oIsAbsolute = IsAbsolute;

clean:

	return r;
}

int gs_win_path_canonicalize(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath)
{
	int r = 0;

	/** required length for PathCanonicalize **/
	if (OutputPathBufSize < MAX_PATH || LenInputPath > MAX_PATH)
		GS_ERR_CLEAN(1);

	/** this does fucking nothing (ex retains mixed slash backslash) **/
	if (! PathCanonicalize(ioOutputPathBuf, InputPathBuf))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_buf_strnlen(ioOutputPathBuf, OutputPathBufSize, oLenOutputPath)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

void gs_current_thread_name_set(
	const char *NameBuf,
	size_t LenName)
{
	/* https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx */

	const DWORD MS_VC_EXCEPTION = 0x406D1388;

	/* can this be omitted? seeing a handler is setup below */
	if (!IsDebuggerPresent())
		return;

	/* dwType is magic. dwThreadID of -1 means name is set for current thread. */

	THREADNAME_INFO ti = {};
	ti.dwType = 0x1000;
	ti.szName = NameBuf;
	ti.dwThreadID = -1;
	ti.dwFlags = 0;

	/* will be throwing a special exception.
	*  if a debugger hypothetically were to not catch it,
	*  setup a handler, catching and ignoring the exception. */

	NT_TIB *tib = ((NT_TIB*)NtCurrentTeb());

	EXCEPTION_REGISTRATION_RECORD rec = {};
	rec.Next = tib->ExceptionList;
	rec.Handler = gs_win_ignore_handler;

	tib->ExceptionList = &rec;

	/* a debugger followin the special exception protocol will
	*  use the exception information to obtain the wanted thread name */

	RaiseException(
		MS_VC_EXCEPTION,
		0,
		sizeof(ti) / sizeof(ULONG_PTR),
		(ULONG_PTR*)&ti);

	/* teardown the exception ignoring handler */

	tib->ExceptionList = tib->ExceptionList->Next;
}

void gs_debug_break() {
	DebugBreak();
}

int gs_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute) {
	return gs_win_path_is_absolute(PathBuf, LenPath, oIsAbsolute);
}

int gs_path_append_abs_rel(
	const char *AbsoluteBuf, size_t LenAbsolute,
	const char *RelativeBuf, size_t LenRelative,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath)
{
	int r = 0;

	size_t LenOutputPathTmp = 0;

	/** maximum length for PathIsRelative and PathAppend **/
	if (LenAbsolute > MAX_PATH || LenRelative > MAX_PATH)
		GS_ERR_CLEAN(1);

	if (PathIsRelative(AbsoluteBuf))
		GS_GOTO_CLEAN();

	if (! PathIsRelative(RelativeBuf))
		GS_GOTO_CLEAN();

	/* prep output buffer with absolute path */

	if (!!(r = gs_buf_copy_zero_terminate(
		AbsoluteBuf, LenAbsolute,
		ioOutputPathBuf, OutputPathBufSize, &LenOutputPathTmp)))
	{
		GS_GOTO_CLEAN();
	}

	/* append */

	if (! PathAppend(ioOutputPathBuf, RelativeBuf))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_buf_strnlen(ioOutputPathBuf, OutputPathBufSize, oLenOutputPath)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_file_exist(
	const char *FileNameBuf, size_t LenFileName,
	size_t *oIsExist)
{
	int r = 0;

	int IsExist = 0;

	if (!!(r = gs_buf_ensure_haszero(FileNameBuf, LenFileName + 1)))
		GS_GOTO_CLEAN();

	/* https://blogs.msdn.microsoft.com/oldnewthing/20071023-00/?p=24713/ */
	/* INVALID_FILE_ATTRIBUTES if file does not exist, apparently */
	IsExist = !(INVALID_FILE_ATTRIBUTES == GetFileAttributes(FileNameBuf));

	if (oIsExist)
		*oIsExist = IsExist;

clean:

	return r;
}

int gs_file_exist_ensure(const char *FileNameBuf, size_t LenFileName) {
	int r = 0;

	if (!!(r = gs_buf_ensure_haszero(FileNameBuf, LenFileName + 1)))
		GS_GOTO_CLEAN();

	/* https://blogs.msdn.microsoft.com/oldnewthing/20071023-00/?p=24713/ */
	/* INVALID_FILE_ATTRIBUTES if file does not exist, apparently */
	if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(FileNameBuf))
		GS_ERR_CLEAN(1);

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

int gs_get_current_executable_directory(
	char *ioCurrentExecutableDirBuf, size_t CurrentExecutableDirSize, size_t *oLenCurrentExecutableDir)
{
	int r = 0;

	size_t LenCurrentExecutable = 0;
	char CurrentExecutableBuf[512] = {};

	if (!!(r = gs_get_current_executable_filename(
		CurrentExecutableBuf, sizeof CurrentExecutableBuf, &LenCurrentExecutable)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_win_path_directory(
		CurrentExecutableBuf, LenCurrentExecutable,
		ioCurrentExecutableDirBuf, CurrentExecutableDirSize, oLenCurrentExecutableDir)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_build_current_executable_relative_filename(
	const char *RelativeBuf, size_t LenRelative,
	char *ioCombinedBuf, size_t CombinedBufSize, size_t *oLenCombined)
{
	int r = 0;

	size_t LenPathCurrentExecutableDir = 0;
	char PathCurrentExecutableDirBuf[512] = {};
	size_t LenPathModification = 0;
	char PathModificationBuf[512] = {};

	/* get directory */
	if (!!(r = gs_get_current_executable_directory(
		PathCurrentExecutableDirBuf, sizeof PathCurrentExecutableDirBuf, &LenPathCurrentExecutableDir)))
	{
		GS_ERR_CLEAN(1);
	}

	/* ensure relative and append */

	if (!!(r = gs_path_append_abs_rel(
		PathCurrentExecutableDirBuf, LenPathCurrentExecutableDir,
		RelativeBuf, LenRelative,
		PathModificationBuf, sizeof PathModificationBuf, &LenPathModification)))
	{
		GS_GOTO_CLEAN();
	}

	/* canonicalize into output */

	if (!!(r = gs_win_path_canonicalize(
		PathModificationBuf, LenPathModification,
		ioCombinedBuf, CombinedBufSize, oLenCombined)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}
