#include <cassert>
#include <cstdint>

#include <memory>
#include <string>
#include <deque>
#include <sstream>

#include <gittest/misc.h>

#include <gittest/log.h>

// FIXME: port to non-msvc (use thread_local keyword most likely)
__declspec( thread ) GsLogGlobal g_tls_log_global = {};

GsLogGlobal *gs_log_global_get() {
	return &g_tls_log_global;
}

GsLogGlobal *gs_log_global_ensure() {
	GsLogGlobal *lg = gs_log_global_get();
	if (lg->mpCurrentLog == NULL) {
		/* FIXME: is there any sane way to eventually delete[] this? */
		sp<GsLogBase> *p = new sp<GsLogBase>();
		lg->mpCurrentLog = p;
	}
	return lg;
}

GsLogBase::GsLogBase()
	: mPreviousLog()
{}

GsLogBase::~GsLogBase() {
	/* have previous entry not paired with an exit? */
	if (mPreviousLog)
		assert(0);
}

void GsLogBase::Enter() {
	GsLogGlobal *lg = gs_log_global_ensure();
	/* no recursive entry */
	if (mPreviousLog)
		assert(0);
	mPreviousLog = shared_from_this();
	mPreviousLog.swap(*lg->mpCurrentLog);
}

void GsLogBase::Exit() {
	GsLogGlobal *lg = gs_log_global_ensure();
	/* have previous exit not paired with an entry? */
	// FIXME: toplevel mpCurrentLog is NULL
	//   enable this check if toplevel dummy log design is used
	//if (!mPreviousLog)
	//	assert(0);
	mPreviousLog.swap(*lg->mpCurrentLog);
	if (mPreviousLog != shared_from_this())
		assert(0);
	mPreviousLog = sp<GsLogBase>();
}

GsLog::GsLog()
	: GsLogBase(),
	mMsg(new std::deque<sp<std::string> >),
	mLogLevelLimit(0)
{}

sp<GsLog> GsLog::Create() {
	sp<GsLog> Ret(new GsLog);
	return Ret;
}

void GsLog::MessageLog(uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine) {
	if (Level > mLogLevelLimit)
		return;
	std::stringstream ss;
	ss << "[" << CppFile << ":" << CppLine << "]: [" << std::string(MsgBuf, MsgSize) << "]";
	sp<std::string> Msg(new std::string(ss.str()));
	mMsg->push_back(Msg);
}

GsLogGuard::GsLogGuard(const sp<GsLog> &Log)
	: mLog(Log)
{
	mLog->Enter();
}

GsLogGuard::~GsLogGuard() {
	mLog->Exit();
}

void gs_log_info(uint32_t Level, const char *MsgBuf, uint32_t MsgSize) {
	GsLogGlobal *lg = gs_log_global_ensure();
	(*lg->mpCurrentLog)->MessageLog(Level, MsgBuf, MsgSize, "[dummy]", 0);
}

void gs_log_info_(uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine)
{
	GsLogGlobal *lg = gs_log_global_ensure();
	(*lg->mpCurrentLog)->MessageLog(Level, MsgBuf, MsgSize, CppFile, CppLine);
}