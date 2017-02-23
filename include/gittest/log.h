#ifndef _GITTEST_LOG_H_
#define _GITTEST_LOG_H_

#include <cstdint>

#include <memory>
#include <string>
#include <deque>
#include <list>
#include <mutex>
#include <atomic>

#include <gittest/misc.h>
#include <gittest/log_defs.h>

#define GS_LOG_VERSION_COMPILED 0x00010000

#define GS_TRIPWIRE_LOG_BASE 0xA37F4680
#define GS_TRIPWIRE_LOG      0xA37F4681

#define GS_LOG_STR_EXTRA_SUFFIX "_log"
#define GS_LOG_STR_EXTRA_EXTENSION ".txt"

#define GS_LOG_ADD(PLOG) { if (!!gs_log_list_add_log(GS_LOG_LIST_GLOBAL_NAME, GS_LOG_BASE_CAST((PLOG)))) { GS_ERR_CLEAN(1); } }
#define GS_LOG_GET(PREFIX) gs_log_list_get_log_ret(GS_LOG_LIST_GLOBAL_NAME, (PREFIX))

/* global log list: user should define, signature 'GsLogList *', initialized eg by 'gs_log_list_global_create' */
#define GS_LOG_LIST_GLOBAL_NAME g_gs_log_list_global

#define GS_LOG_BASE_CAST(PTR) gs_log_base_cast_((GsLogBase *)(PTR))
#define GS_LOG_CAST(PTR) gs_log_cast_((GsLog *)(PTR))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef uint32_t gs_tripwire_t;

struct GsVersion;
struct GsLogList;
struct GsLogTls;
struct GsLogBase;
struct GsLog;

/* public structure - free with gs_log_dump_free */
struct GsLogDump {
	char *mBuf;
	size_t mBufSize;
	size_t mLenBuf;
};

/* global log list: declaration only */
extern GsLogList *GS_LOG_LIST_GLOBAL_NAME;


GsLogBase *gs_log_base_cast_(void *Log);
int gs_log_base_init(GsLogBase *Klass, uint32_t LogLevelLimit, const char *Prefix);
void gs_log_base_enter(GsLogBase *Klass);
void gs_log_base_exit(GsLogBase *Klass);

GsLog *gs_log_cast_(void *Log);
int gs_log_create(const char *Prefix, GsLog **oLog);
GsLog * gs_log_create_ret(const char *Prefix);
int gs_log_init(GsLog *Klass, uint32_t LogLevelLimit);
void gs_log_message_log(GsLogBase *XKlass, uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine);
int gs_log_dump_and_flush(GsLogBase *XKlass, GsLogDump *oLogDump);

void gs_log_tls(uint32_t Level, const char *MsgBuf, uint32_t MsgSize);

void gs_log_version_make_compiled(GsVersion *oVersion);
int gs_log_version_check_compiled(GsVersion *other);
int gs_log_version_check(GsVersion *other, GsVersion compare);

void gs_log_dump_reset(GsLogDump *ioDump);
void gs_log_dump_reset_to_noalloc(GsLogDump *ioDump, char *Buf, size_t BufSize, size_t LenBuf);
void gs_log_dump_reset_to(GsLogDump *ioDump, const char *Buf, size_t BufSize, size_t LenBuf);

int gs_log_list_create(GsLogList **oLogList);
int gs_log_list_free(GsLogList *LogList);
int gs_log_list_add_log(GsLogList *LogList, GsLogBase *Log);
int gs_log_list_get_log(GsLogList *LogList, const char *Prefix, GsLogBase **oLog);
GsLogBase * gs_log_list_get_log_ret(GsLogList *LogList, const char *Prefix);
int gs_log_list_dump_all(GsLogList *LogList, GsLogDump *oRetDump);

int gs_log_crash_handler_setup();


#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

/* global log list: can initialize the g_gs_log_list_global */
GsLogList *gs_log_list_global_create_cpp();

class GsLogGuard {
public:
	GsLogGuard(GsLogBase *Log)
		: mLog(Log)
	{
		gs_log_base_enter(mLog);
	}

	~GsLogGuard() {
		gs_log_base_exit(mLog);
	}

	GsLogBase *GetLog() {
		return mLog;
	}

private:
	GsLogBase *mLog;
};

typedef GsLogGuard log_guard_t;

#endif /* __cplusplus */

#endif /* _GITTEST_LOG_H_ */
