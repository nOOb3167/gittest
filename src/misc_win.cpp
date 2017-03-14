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

static EXCEPTION_DISPOSITION NTAPI ignore_handler(
	EXCEPTION_RECORD *rec,
	void *frame,
	CONTEXT *ctx,
	void *disp);


int gs_win_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute) {
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

EXCEPTION_DISPOSITION NTAPI ignore_handler(
	EXCEPTION_RECORD *rec,
	void *frame,
	CONTEXT *ctx,
	void *disp)
{
	return ExceptionContinueExecution;
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
	rec.Handler = ignore_handler;

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
