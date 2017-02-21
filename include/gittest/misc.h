#ifndef _GITTEST_MISC_H_
#define _GITTEST_MISC_H_

#include <memory>
#include <string>
#include <map>

//#define GS_DBG_CLEAN {}
#define GS_DBG_CLEAN { assert(0); }
//#define GS_DBG_CLEAN { DebugBreak(); }

#define GS_ERR_NO_CLEAN(THE_R) { r = (THE_R); goto noclean; }
#define GS_ERR_CLEAN(THE_R) { r = (THE_R); GS_DBG_CLEAN; goto clean; }
#define GS_GOTO_CLEAN() { GS_DBG_CLEAN; goto clean; }
#define GS_ERR_CLEANSUB(THE_R) { r = (THE_R); GS_DBG_CLEAN; goto cleansub; }
#define GS_GOTO_CLEANSUB() { GS_DBG_CLEAN; goto cleansub; }

#define GS_AUX_MARKER_STRUCT_IS_COPYABLE /* dummy (marker / documentation purpose) */

typedef ::std::map<::std::string, ::std::string> confmap_t;

// FIXME: evil? two character identifier inside header..
template<typename T>
using sp = ::std::shared_ptr<T>;

#endif /* _GITTEST_MISC_H_ */
