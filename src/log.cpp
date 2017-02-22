#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

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
#include <stdexcept> // std::runtime_error

#include <gittest/misc.h>

#include <gittest/log.h>

typedef ::std::map<::std::string, sp<GsLogBase> > gs_log_map_t;

struct GsLogList {
	GsVersion mVersion;

	sp<std::mutex> mMutexData;
	sp<GsLogList> mSelf;
	sp<gs_log_map_t> mLogs;
};

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

void GsLogBase::GsLogBaseCompleteInit(const sp<GsLogBase> &Self) {
	mSelf = Self;
}

GsLogBase::GsLogBase(const std::string &Prefix)
	: mPreviousLog(),
	mPrefix(Prefix)
{
	mVersion = gs_log_version_make_compiled();
}

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
	mPreviousLog = mSelf;
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
	if (mPreviousLog != mSelf)
		assert(0);
	mPreviousLog = sp<GsLogBase>();
}

GsLog::GsLog(uint32_t LogLevelLimit, const std::string &Prefix)
	: GsLogBase(Prefix),
	mMsg(new std::deque<sp<std::string> >),
	mLogLevelLimit(LogLevelLimit)
{}

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

int gs_log_create(const char *Prefix, GsLog **oLog) {
	int r = 0;

	sp<GsLog> Log(new GsLog(GS_LOG_LEVEL_INFO, Prefix));
	Log->GsLogBaseCompleteInit(Log);

	if (oLog)
		*oLog = Log.get();

clean:

	return r;
}

GsLog * gs_log_create_ret(const char *Prefix) {
	GsLog *Log = NULL;

	if (!!gs_log_create(Prefix, &Log))
		return NULL;

	return Log;
}

void gs_log_tls(uint32_t Level, const char *MsgBuf, uint32_t MsgSize) {
	gs_log_tls_SZ("[dummy]", 0, Level, MsgBuf, MsgSize);
}

GsVersion gs_log_version_make_compiled() {
	GsVersion Ret;
	Ret.mVersion = GS_LOG_VERSION_COMPILED;
	return Ret;
}

int gs_log_version_check_compiled(GsVersion *other) {
	if (other->mVersion != GS_LOG_VERSION_COMPILED)
		return 1;
	return 0;
}

int gs_log_version_check(GsVersion *other, GsVersion compare) {
	if (other->mVersion != compare.mVersion)
		return 1;
	return 0;
}

int gs_log_list_create(GsLogList **oLogList) {
	int r = 0;

	sp<GsLogList> LogList(new GsLogList());

	LogList->mMutexData = sp<std::mutex>(new std::mutex);

	std::lock_guard<std::mutex> lock(*LogList->mMutexData);
	{
		LogList->mVersion = gs_log_version_make_compiled();
		LogList->mSelf = LogList;
		LogList->mLogs = sp<gs_log_map_t>(new gs_log_map_t);
	}

	if (oLogList)
		*oLogList = LogList.get();

clean:

	return r;
}

GsLogList *gs_log_list_global_create_cpp() {
	int r = 0;

	GsLogList *LogList = NULL;

	if (!!(r = gs_log_list_create(&LogList))) {
		/* NOTE: c++ API (exception throwing) */
		assert(0);
		throw std::runtime_error("[ERROR] exception from gs_log_list_global_create_cpp");
	}

	return LogList;
}

int gs_log_list_free(GsLogList *LogList) {
	int r = 0;

	/* NOTE: special deletion protocol - managed by shared_ptr technically */
	std::lock_guard<std::mutex> lock(*LogList->mMutexData);
	{
		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			GS_GOTO_CLEAN();

		LogList->mSelf = sp<GsLogList>();
	}

clean:

	return r;
}

int gs_log_list_add_log(GsLogList *LogList, GsLogBase *Log) {
	int r = 0;

	std::lock_guard<std::mutex> lock(*LogList->mMutexData);
	{
		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			GS_GOTO_CLEAN();

		if (!!(r = gs_log_version_check_compiled(&Log->mVersion)))
			GS_GOTO_CLEAN();

		if (LogList->mLogs->find(Log->mPrefix) != LogList->mLogs->end())
			GS_ERR_CLEAN(1);

		(*LogList->mLogs)[Log->mPrefix] = Log->mSelf;
	}

clean:

	return r;
}

int gs_log_list_get_log(GsLogList *LogList, const char *Prefix, GsLogBase **oLog) {
	int r = 0;

	sp<GsLogBase> Log;

	std::string sPrefix(Prefix);

	std::lock_guard<std::mutex> lock(*LogList->mMutexData);
	{
		gs_log_map_t::iterator it;

		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			GS_GOTO_CLEAN();

		it = LogList->mLogs->find(sPrefix);
		
		if (it == LogList->mLogs->end())
			GS_ERR_CLEAN(1);

		if (!!(r = gs_log_version_check_compiled(&it->second->mVersion)))
			GS_GOTO_CLEAN();

		Log = it->second;
	}

	if (oLog)
		*oLog = Log.get();

clean:

	return r;
}

GsLogBase * gs_log_list_get_log_ret(GsLogList *LogList, const char *Prefix) {
	GsLogBase *Log = NULL;

	if (!!gs_log_list_get_log(LogList, Prefix, &Log))
		return NULL;

	return Log;
}
