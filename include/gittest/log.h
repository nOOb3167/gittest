#ifndef _GITTEST_LOG_H_
#define _GITTEST_LOG_H_

#include <stdint.h>

#include <gittest/bypart.h>

#define GS_LOG_STR_EXTRA_SUFFIX    "_log"
#define GS_LOG_STR_EXTRA_EXTENSION ".txt"

#define GS_LOG_ADD(PLOG) { if (!!gs_log_list_add_log(GS_LOG_LIST_GLOBAL_NAME, (PLOG))) { GS_ERR_CLEAN(1); } }
#define GS_LOG_GET(PREFIX) gs_log_list_get_log_ret(GS_LOG_LIST_GLOBAL_NAME, (PREFIX))
#define GS_LOG_GET_2(PREFIX1, OPT_PREFIX2) gs_log_list_get_log_ret_2(GS_LOG_LIST_GLOBAL_NAME, (PREFIX1), (OPT_PREFIX2))

/* global log list: user should define, signature 'GsLogList *', initialized eg by 'gs_log_list_global_create' */
#define GS_LOG_LIST_GLOBAL_NAME g_gs_log_list_global


struct GsVersion;
struct GsLogBase;
struct GsLogList;
struct GsLogTls;

struct GsLogUnified;

/* global log list: declaration only */
extern GsLogList *GS_LOG_LIST_GLOBAL_NAME;

void gs_log_version_make_compiled(struct GsVersion *oVersion);
int gs_log_version_check_compiled(struct GsVersion *other);

struct GsLogBase * gs_log_base_create_ret(const char *Prefix);
int gs_log_base_create(
	uint32_t LogLevelLimit,
	const char *PrefixBuf, size_t LenPrefix,
	struct GsLogBase **oBase);
int gs_log_base_destroy(struct GsLogBase *Base);
void gs_log_base_enter(struct GsLogBase *Base);
void gs_log_base_exit(struct GsLogBase *Base);

struct GsLogList * gs_log_list_global_create();
int gs_log_list_create(struct GsLogList **oLogList);
int gs_log_list_free(struct GsLogList *LogList);
int gs_log_list_set_log_unified(struct GsLogList *LogList, struct GsLogUnified *LogUnified);
int gs_log_list_add_log(struct GsLogList *LogList, struct GsLogBase *Log);
int gs_log_list_get_log(struct GsLogList *LogList, const char *Prefix, struct GsLogBase **oLog);
int gs_log_list_dump_all_lowlevel(GsLogList *LogList, void *ctx, gs_bypart_cb_t cb);
struct GsLogBase * gs_log_list_get_log_ret(struct GsLogList *LogList, const char *Prefix);
struct GsLogBase * gs_log_list_get_log_ret_2(struct GsLogList *LogList, const char *Prefix1, const char *optPrefix2);

int gs_log_crash_handler_dump_global_log_list_suffix_2(
	const char *SuffixBuf1, const char *SuffixBuf2);

/* defined per-platform */
int gs_log_crash_handler_setup();
int gs_log_crash_handler_dump_global_log_list_suffix(
	const char *SuffixBuf, size_t LenSuffix);

/* defined in log_unified.cpp */
int gs_log_unified_create(struct GsLogUnified **oLogUnified);
int gs_log_unified_destroy(struct GsLogUnified *LogUnified);
int gs_log_unified_message_log(
	GsLogUnified *LogUnified,
	const char *Prefix,
	uint32_t Level,
	const char *MsgBuf,
	uint32_t MsgSize,
	const char *CppFile,
	int CppLine);


void gs_log_crash_handler_printall_cpp();

/* global log list: can initialize the g_gs_log_list_global */
GsLogList *gs_log_list_global_create();

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

#endif /* _GITTEST_LOG_H_ */
