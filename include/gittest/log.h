#ifndef _GITTEST_LOG_H_
#define _GITTEST_LOG_H_

#include <cstdint>

#include <memory>
#include <string>
#include <deque>

#include <gittest/misc.h>
#include <gittest/log_defs.h>

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
	GsLog(uint32_t LogLevelLimit, const std::string &Prefix);
public:
	static sp<GsLog> Create();
	static sp<GsLog> Create(const std::string &Prefix);
	void MessageLog(uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine);
private:
	sp<std::deque<sp<std::string> > > mMsg;
	uint32_t mLogLevelLimit;
	std::string mPrefix;
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

void gs_log_tls(uint32_t Level, const char *MsgBuf, uint32_t MsgSize);

#endif /* _GITTEST_LOG_H_ */
