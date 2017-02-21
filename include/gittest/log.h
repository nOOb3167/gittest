#ifndef _GITTEST_LOG_H_
#define _GITTEST_LOG_H_

#include <cstdint>

#include <memory>
#include <string>
#include <deque>

#include <gittest/misc.h>

#define GS_LOG_INFO(LEVEL, MSGBUF, MSGSIZE) { gs_log_info_((LEVEL), (MSGBUF), (MSGSIZE), __FILE__, __LINE__); }

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
	GsLog();
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

class GsLogGuard {
public:
	GsLogGuard::GsLogGuard(const sp<GsLog> &Log);
	~GsLogGuard();
private:
	sp<GsLogBase> mLog;
};

typedef GsLogGuard log_guard;

void gs_log_info_(uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine);

void gs_log_info(uint32_t Level, const char *MsgBuf, uint32_t MsgSize);

#endif /* _GITTEST_LOG_H_ */
