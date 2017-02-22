#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstdarg>

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

GsLog::GsLog(uint32_t LogLevelLimit, const std::string &Prefix)
	: GsLogBase(),
	mMsg(new std::deque<sp<std::string> >),
	mLogLevelLimit(LogLevelLimit),
	mPrefix(Prefix)
{}

sp<GsLog> GsLog::Create() {
	sp<GsLog> Ret(new GsLog(GS_LOG_LEVEL_INFO, ""));
	return Ret;
}

sp<GsLog> GsLog::Create(const std::string &Prefix) {
	sp<GsLog> Ret(new GsLog(GS_LOG_LEVEL_INFO, Prefix));
	return Ret;
}

void GsLog::MessageLog(uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine) {
	if (Level > mLogLevelLimit)
		return;
	std::stringstream ss;
	ss << "[" + mPrefix + "] [" << CppFile << ":" << CppLine << "]: [" << std::string(MsgBuf, MsgSize) << "]";
	sp<std::string> Msg(new std::string(ss.str()));
	mMsg->push_back(Msg);
}

void gs_log_tls_SZ(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf, uint32_t MsgSize){
	GsLogGlobal *lg = gs_log_global_ensure();
	if (*lg->mpCurrentLog)
		(*lg->mpCurrentLog)->MessageLog(Level, MsgBuf, MsgSize, CppFile, CppLine);
}

void gs_log_tls_S(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf){
	const size_t sanity_arbitrary_max = 2048;
	size_t MsgSize = strnlen(MsgBuf, sanity_arbitrary_max);
	assert(MsgSize < sanity_arbitrary_max);

	GsLogGlobal *lg = gs_log_global_ensure();
	if (*lg->mpCurrentLog)
		(*lg->mpCurrentLog)->MessageLog(Level, MsgBuf, MsgSize, CppFile, CppLine);
}

void gs_log_tls_PF(const char *CppFile, int CppLine, uint32_t Level, const char *Format, ...) {
	const size_t sanity_arbitrary_max = 2048;

	char buf[sanity_arbitrary_max] = {};
	int numwrite = 0;

	va_list argp;
	va_start(argp, Format);

	if ((numwrite = vsnprintf(buf, sizeof buf, Format, argp)) == -1)
		assert(0);
	if (numwrite >= sizeof buf)
		assert(0);

	va_end(argp);

	size_t MsgSize = strnlen(buf, sanity_arbitrary_max);
	assert(MsgSize < sanity_arbitrary_max);

	GsLogGlobal *lg = gs_log_global_ensure();
	if (*lg->mpCurrentLog)
		(*lg->mpCurrentLog)->MessageLog(Level, buf, MsgSize, CppFile, CppLine);
}

void gs_log_tls(uint32_t Level, const char *MsgBuf, uint32_t MsgSize) {
	gs_log_tls_SZ("[dummy]", 0, Level, MsgBuf, MsgSize);
}
