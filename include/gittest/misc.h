#ifndef _GITTEST_MISC_H_
#define _GITTEST_MISC_H_

#include <cassert>

#include <memory>
#include <string>
#include <map>

#include <gittest/config_defs.h>
#include <gittest/log_defs.h>

#if defined (_MSC_VER)
#define GS_THREAD_LOCAL_DESIGNATOR __declspec( thread )
#else
#define GS_THREAD_LOCAL_DESIGNATOR __thread
#endif

#define GS_DEBUG_BREAK gs_debug_break()

#define GS_DBG_CLEAN() GS_CONFIG_DEFS_MISC_GS_GOTO_CLEAN_HANDLING

#define GS_DBG_LOG() GS_LOG(CLEAN, S, "CLEAN");

#define GS_ERR_NO_CLEAN(THE_R) { r = (THE_R); GS_DBG_LOG(); goto noclean; }
#define GS_ERR_CLEAN(THE_R) { r = (THE_R); GS_DBG_LOG(); GS_DBG_CLEAN(); goto clean; }
#define GS_GOTO_CLEAN() { GS_DBG_LOG(); GS_DBG_CLEAN(); goto clean; }
#define GS_ERR_CLEANSUB(THE_R) { r = (THE_R); GS_DBG_LOG(); GS_DBG_CLEAN(); goto cleansub; }
#define GS_GOTO_CLEANSUB() { GS_DBG_LOG(); GS_DBG_CLEAN(); goto cleansub; }

#define GS_ERR_NO_CLEAN_L(THE_R, LEVEL, TT, ...) { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_ERR_NO_CLEAN(THE_R); }
#define GS_ERR_CLEAN_L(THE_R, LEVEL, TT, ...) { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_ERR_CLEAN(THE_R); }
#define GS_GOTO_CLEAN_L(LEVEL, TT, ...) { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_GOTO_CLEAN(); }

#define GS_AUX_MARKER_STRUCT_IS_COPYABLE /* dummy (marker / documentation purpose) */

#define GS_DUMMY_BLOCK() ((void) 0)

/* WARNING: evaluates arguments multiple times. rework using block with decltype assignment. */
#define GS_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define GS_MIN(x, y) (((x) < (y)) ? (x) : (y))

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

int gs_buf_ensure_haszero(const char *Buf, size_t BufSize);

int aux_char_from_string_alloc(const std::string &String, char **oStrBuf, size_t *oLenStr);

#endif /* _GITTEST_MISC_H_ */
