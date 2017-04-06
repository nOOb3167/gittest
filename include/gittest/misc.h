#ifndef _GITTEST_MISC_H_
#define _GITTEST_MISC_H_

#include <cassert>

#include <memory>
#include <string>
#include <map>

#include <gittest/config_defs.h>
#include <gittest/log_defs.h>

/*
* = Visual Studio debugger function call expression evaluation (variable watch) =
*   for whatever reason, it seems function calls (ex call function with side effects from a variable watch expression)
*   fail / stall out if the currently selected thread (ex shown current in threads window) is inside certain callstacks.
*   in particular std::this_thread::sleep_for.
*   the workaround enabling function evaluation is to first step out of such calls (back into app source code),
*   and only then attempt to trigger reevaluation.
*/

#if defined (_MSC_VER)
#define GS_THREAD_LOCAL_DESIGNATOR __declspec( thread )
#else
#define GS_THREAD_LOCAL_DESIGNATOR __thread
#endif

#define GS_DEBUG_BREAK() gs_debug_break()

#define GS_ASSERT(x) \
	do { bool the_x = (x); if (! the_x) { GS_DEBUG_BREAK(); assert(0); } } while (0)

#define GS_DBG_CLEAN() GS_CONFIG_DEFS_MISC_GS_GOTO_CLEAN_HANDLING

#define GS_DBG_LOG() GS_LOG(CLEAN, S, "CLEAN");

#define GS_ERR_NO_CLEAN(THE_R) do { r = (THE_R); GS_DBG_LOG(); goto noclean; } while(0)
#define GS_ERR_CLEAN(THE_R) do { r = (THE_R); GS_DBG_LOG(); GS_DBG_CLEAN(); goto clean; } while(0)
#define GS_GOTO_CLEAN() do { GS_DBG_LOG(); GS_DBG_CLEAN(); goto clean; } while(0)
#define GS_ERR_CLEANSUB(THE_R) do { r = (THE_R); GS_DBG_LOG(); GS_DBG_CLEAN(); goto cleansub; } while(0)
#define GS_GOTO_CLEANSUB() do { GS_DBG_LOG(); GS_DBG_CLEAN(); goto cleansub; } while(0)

#define GS_ERR_NO_CLEAN_L(THE_R, LEVEL, TT, ...) do { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_ERR_NO_CLEAN(THE_R); } while(0)
#define GS_ERR_CLEAN_L(THE_R, LEVEL, TT, ...) do { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_ERR_CLEAN(THE_R); } while(0)
#define GS_GOTO_CLEAN_L(LEVEL, TT, ...) do { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_GOTO_CLEAN(); } while(0)

/* should not clash with other error codes etc - just used random.org */
#define GS_ERRCODE_RECONNECT 0x7BDD6EAF
#define GS_ERRCODE_EXIT      0x7BDD6EB0

#define GS_AUX_MARKER_STRUCT_IS_COPYABLE /* dummy (marker / documentation purpose) */

#define GS_DUMMY_BLOCK() ((void) 0)

/* WARNING: evaluates arguments multiple times. rework using block with decltype assignment. */
#define GS_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define GS_MIN(x, y) (((x) < (y)) ? (x) : (y))

#define GS_SP_SET_RAW_NULLING(VARNAME_SP, VARNAME_PRAW, TYPENAME) \
	do { VARNAME_SP = std::shared_ptr<TYPENAME>(VARNAME_PRAW); VARNAME_PRAW = NULL; } while(0)

typedef ::std::map<::std::string, ::std::string> confmap_t;

// FIXME: evil? two character identifier inside header..
template<typename T>
using sp = ::std::shared_ptr<T>;

int gs_build_modified_filename(
	const char *BaseFileNameBuf, size_t LenBaseFileName,
	const char *ExpectedSuffix, size_t LenExpectedSuffix,
	const char *ExpectedExtension, size_t LenExpectedExtension,
	const char *ExtraSuffix, size_t LenExtraSuffix,
	const char *ExtraExtension, size_t LenExtraExtension,
	char *ioModifiedFileNameBuf, size_t ModifiedFileNameSize, size_t *oLenModifiedFileName);

int gs_buf_copy_zero_terminate(
	const char *SrcBuf, size_t LenSrc,
	char *ioDstBuf, size_t DstBufSize, size_t *oLenDst);

int gs_buf_strnlen(const char *Buf, size_t BufSize, size_t *oLenBuf);

int gs_buf_ensure_haszero(const char *Buf, size_t BufSize);

int aux_char_from_string_alloc(const std::string &String, char **oStrBuf, size_t *oLenStr);

void gs_current_thread_name_set_cstr(
	const char *NameCStr);

void gs_current_thread_name_set(
	const char *NameBuf,
	size_t LenName);

void gs_debug_break();

int gs_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute);

int gs_path_append_abs_rel(
	const char *AbsoluteBuf, size_t LenAbsolute,
	const char *RelativeBuf, size_t LenRelative,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath);

#endif /* _GITTEST_MISC_H_ */
