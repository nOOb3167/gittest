#ifndef _GITTEST_LOG_H_
#define _GITTEST_LOG_H_

#include <cstdint>

#include <memory>
#include <string>
#include <deque>

#include <gittest/misc.h>

#define GS_LOG_LEVEL_INFO 1000
#define GS_LOG_LEVEL_I GS_LOG_LEVEL_INFO

#define GS_LOG(LEVEL, TT, ...) { GS_LOG_TT_ ## TT (__FILE__, __LINE__, GS_LOG_LEVEL_ ## LEVEL, __VA_ARGS__); }

#define GS_LOG_TT_SZ gs_log_tls_SZ
#define GS_LOG_TT_S  gs_log_tls_S
#define GS_LOG_TT_PF  gs_log_tls_PF

class GsLogBase : std::enable_shared_from_this<GsLogBase> {
protected:
	GsLogBase();
public:
	virtual ~GsLogBase();
	void Enter();
	void Exit();
	virtual void MessageLog(uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine) = 0;
private:
	sp<GsLogBase> mPreviousLog;
};

class GsLog : public GsLogBase {
protected:
	GsLog(uint32_t LogLevelLimit);
public:
	static sp<GsLog> Create();
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
	GsLogGuard::GsLogGuard(const sp<GsLog> &Log)
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

void gs_log_tls_SZ(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf, uint32_t MsgSize);
void gs_log_tls_S(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf);
void gs_log_tls_PF(const char *CppFile, int CppLine, uint32_t Level, const char *Format, ...);

void gs_log_tls(uint32_t Level, const char *MsgBuf, uint32_t MsgSize);

#endif /* _GITTEST_LOG_H_ */
