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

#define GS_LOG_ADD(PLOG) { if (!!gs_log_list_add_log(GS_LOG_LIST_GLOBAL_NAME, (PLOG))) { GS_ERR_CLEAN(1); } }
#define GS_LOG_GET(PREFIX) gs_log_list_get_log_ret(GS_LOG_LIST_GLOBAL_NAME, (PREFIX))

struct GsLogList;

/* global log list: user should define, signature 'GsLogList *', initialized eg by 'gs_log_list_global_create' */
#define GS_LOG_LIST_GLOBAL_NAME g_gs_log_list_global
/* global log list: declaration only */
extern GsLogList *g_gs_log_list_global;
/* global log list: can initialize the g_gs_log_list_global */
GsLogList *gs_log_list_global_create_cpp();

struct GsVersion {
	uint32_t mVersion;
};

class GsLogBase : public std::enable_shared_from_this<GsLogBase> {
public:
	GsVersion mVersion;
protected:
	GsLogBase(const std::string &Prefix);
public:
	virtual ~GsLogBase();
	void Enter();
	void Exit();
	virtual void MessageLog(uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine) = 0;
public:
	std::string mPrefix;
private:
	sp<GsLogBase> mPreviousLog;
};

class GsLog : public GsLogBase {
protected:
	GsLog(uint32_t LogLevelLimit, const std::string &Prefix);
public:
	static sp<GsLog> Create();
	static sp<GsLog> Create(const std::string &Prefix);
	void MessageLog(uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine);
private:
	sp<std::deque<sp<std::string> > > mMsg;
	uint32_t mLogLevelLimit;
};

struct GsLogGlobal {
	sp<GsLogBase> *mpCurrentLog;
};

template<typename T>
class GsLogGuard {
public:
	GsLogGuard::GsLogGuard(const sp<T> &Log)
		: mLog(Log)
	{
		mLog->Enter();
	}

	~GsLogGuard() {
		mLog->Exit();
	}

	sp<T> GetLog() {
		return mLog;
	}

private:
	sp<T> mLog;
};

template<typename T>
using log_guard = GsLogGuard<T>;

void gs_log_tls(uint32_t Level, const char *MsgBuf, uint32_t MsgSize);

GsVersion gs_log_version_make_compiled();
int gs_log_version_check_compiled(GsVersion *other);
int gs_log_version_check(GsVersion *other, GsVersion compare);

int gs_log_list_create(GsLogList **oLogList);
int gs_log_list_free(GsLogList *LogList);
int gs_log_list_add_log(GsLogList *LogList, GsLogBase *Log);
int gs_log_list_get_log(GsLogList *LogList, const char *Prefix, GsLogBase **oLog);
GsLogBase * gs_log_list_get_log_ret(GsLogList *LogList, const char *Prefix);

#endif /* _GITTEST_LOG_H_ */
