#ifndef _GITTEST_MISC_H_
#define _GITTEST_MISC_H_

#include <cassert>

#include <memory>
#include <string>
#include <map>

#include <gittest/log_defs.h>

//#define GS_DBG_CLEAN {}
#define GS_DBG_CLEAN() { assert(0); }
//#define GS_DBG_CLEAN { DebugBreak(); }

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

typedef ::std::map<::std::string, ::std::string> confmap_t;

// FIXME: evil? two character identifier inside header..
template<typename T>
using sp = ::std::shared_ptr<T>;

int gs_build_modified_filename(
	char *BaseFileNameBuf, size_t LenBaseFileName,
	char *ExpectedSuffix, size_t LenExpectedSuffix,
	char *ExpectedExtension, size_t LenExpectedExtension,
	char *ExtraSuffix, size_t LenExtraSuffix,
	char *ExtraExtension, size_t LenExtraExtension,
	char *ioModifiedFileNameBuf, size_t ModifiedFileNameSize, size_t *oLenModifiedFileName);

#endif /* _GITTEST_MISC_H_ */
