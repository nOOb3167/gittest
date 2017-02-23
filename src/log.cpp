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

#include <algorithm>
#include <utility>
#include <memory>
#include <string>
#include <deque>
#include <sstream>
#include <stdexcept> // std::runtime_error

#include <gittest/misc.h>

#include <gittest/log.h>

/* NOTE: implementation of logging should not itself log (avoid recursion / deadlock)
*    therefore function calls and macros such as GS_GOTO_CLEAN which may result in logging
*    must be avoided inside logging implementation code. */

typedef ::std::map<::std::string, GsLogBase *> gs_log_map_t;

struct GsVersion {
	uint32_t mVersion;
	char dummy[64];
};

struct GsLogList {
	GsVersion mVersion;

	sp<std::mutex> mMutexData;
	sp<GsLogList> mSelf;
	sp<gs_log_map_t> mLogs;
};

struct GsLogGlobal {
	GsLogBase *mpCurrentLog;
};

typedef void (*gs_log_base_func_message_log_t)(GsLogBase *XKlass, uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine);
typedef int  (*gs_log_base_func_dump_t)(GsLogBase *XKlass, GsLogDump *oLogDump);

struct GsLogBase {
	GsVersion mVersion;
	gs_tripwire_t mTripwire;

	std::string mPrefix;
	GsLogBase *mPreviousLog;

	gs_log_base_func_message_log_t mFuncMessageLog;
	gs_log_base_func_dump_t mFuncDump;
};

struct GsLog {
	GsLogBase mBase;
	gs_tripwire_t mTripwire;

	sp<std::deque<sp<std::string> > > mMsg;
	uint32_t mLogLevelLimit;
};

// FIXME: port to non-msvc (use thread_local keyword most likely)
__declspec( thread ) GsLogGlobal g_tls_log_global = {};

GsLogGlobal *gs_log_global_get() {
	return &g_tls_log_global;
}

GsLogBase *gs_log_base_cast_(void *Log) {
	GsLogBase *a = (GsLogBase *)Log;
	if (a->mTripwire != GS_TRIPWIRE_LOG_BASE)
		assert(0);
	return a;
}

int gs_log_base_init(GsLogBase *Klass, uint32_t LogLevelLimit, const char *Prefix) {
	int r = 0;

	gs_log_version_make_compiled(&Klass->mVersion);
	Klass->mTripwire = GS_TRIPWIRE_LOG_BASE;
	Klass->mPrefix = std::string(Prefix);
	Klass->mPreviousLog = NULL;

	Klass->mFuncMessageLog = NULL;
	Klass->mFuncDump = NULL;

clean:

	return r;
}

void gs_log_base_enter(GsLogBase *Klass) {
	GsLogGlobal *lg = gs_log_global_get();
	/* no recursive entry */
	if (Klass->mPreviousLog)
		assert(0);
	Klass->mPreviousLog = Klass;
	std::swap(Klass->mPreviousLog, lg->mpCurrentLog);
}

void gs_log_base_exit(GsLogBase *Klass) {
	GsLogGlobal *lg = gs_log_global_get();
	/* have previous exit not paired with an entry? */
	// FIXME: toplevel mpCurrentLog is NULL
	//   enable this check if toplevel dummy log design is used
	//if (!mPreviousLog)
	//	assert(0);
	std::swap(Klass->mPreviousLog, lg->mpCurrentLog);
	if (Klass->mPreviousLog != Klass)
		assert(0);
	Klass->mPreviousLog = NULL;
}

GsLog *gs_log_cast_(void *Log) {
	GsLog *a = (GsLog *)Log;
	if (a->mTripwire != GS_TRIPWIRE_LOG)
		assert(0);
	return a;
}

int gs_log_create(const char *Prefix, GsLog **oLog) {
	int r = 0;

	GsLog *Log = new GsLog;
	GsLogBase *LogBase = &Log->mBase;

	const uint32_t DefaultLevel = GS_LOG_LEVEL_INFO;

	if (!!(r = gs_log_base_init(LogBase, DefaultLevel, Prefix)))
		goto clean;

	if (!!(r = gs_log_init(Log, DefaultLevel)))
		goto clean;

	if (oLog)
		*oLog = Log;

clean:

	return r;
}

GsLog * gs_log_create_ret(const char *Prefix) {
	GsLog *Log = NULL;

	if (!!gs_log_create(Prefix, &Log))
		return NULL;

	return Log;
}

int gs_log_init(GsLog *Klass, uint32_t LogLevelLimit) {
	int r = 0;

	Klass->mTripwire = GS_TRIPWIRE_LOG;
	Klass->mMsg = sp<std::deque<sp<std::string> > >(new std::deque<sp<std::string> >);
	Klass->mLogLevelLimit = LogLevelLimit;

	Klass->mBase.mFuncMessageLog = gs_log_message_log;
	Klass->mBase.mFuncDump = gs_log_dump_and_flush;

clean:

	return r;
}

void gs_log_message_log(GsLogBase *XKlass, uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine) {
	GsLog *Klass = GS_LOG_CAST(XKlass);

	if (Level > Klass->mLogLevelLimit)
		return;

	std::stringstream ss;
	ss << "[" + Klass->mBase.mPrefix + "] [" << CppFile << ":" << CppLine << "]: [" << std::string(MsgBuf, MsgSize) << "]";

	Klass->mMsg->push_back(sp<std::string>(new std::string(ss.str())));
}

int gs_log_dump_and_flush(GsLogBase *XKlass, GsLogDump *oLogDump) {
	int r = 0;

	GsLog *Klass = GS_LOG_CAST(XKlass);

	std::string Dump;

	for (std::deque<sp<std::string> >::iterator it = Klass->mMsg->begin(); it != Klass->mMsg->end(); it++) {
		const sp<std::string> &Line = *it;

		Dump.append(*Line);
		Dump.append("\n");
	}

	Klass->mMsg->clear();

	if (oLogDump) {
		const char *DumpCStr = Dump.c_str();
		const size_t LenDumpCStr = strlen(DumpCStr);
		const size_t DumpCStrSize = LenDumpCStr + 1;
		char *DumpC = new char[DumpCStrSize];
		memcpy(DumpC, DumpCStr, DumpCStrSize);
		oLogDump->mBuf = DumpC;
		oLogDump->mBufSize = DumpCStrSize;
		oLogDump->mLenBuf = LenDumpCStr;
	}

clean:

	return r;
}

void gs_log_tls_SZ(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf, uint32_t MsgSize){
	GsLogGlobal *lg = gs_log_global_get();
	if (lg->mpCurrentLog)
		lg->mpCurrentLog->mFuncMessageLog(lg->mpCurrentLog, Level, MsgBuf, MsgSize, CppFile, CppLine);
}

void gs_log_tls_S(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf){
	const size_t sanity_arbitrary_max = 2048;
	size_t MsgSize = strnlen(MsgBuf, sanity_arbitrary_max);
	assert(MsgSize < sanity_arbitrary_max);

	GsLogGlobal *lg = gs_log_global_get();
	if (lg->mpCurrentLog)
		lg->mpCurrentLog->mFuncMessageLog(lg->mpCurrentLog, Level, MsgBuf, MsgSize, CppFile, CppLine);
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

	GsLogGlobal *lg = gs_log_global_get();
	if (lg->mpCurrentLog)
		lg->mpCurrentLog->mFuncMessageLog(lg->mpCurrentLog, Level, buf, MsgSize, CppFile, CppLine);
}

void gs_log_tls(uint32_t Level, const char *MsgBuf, uint32_t MsgSize) {
	gs_log_tls_SZ("[dummy]", 0, Level, MsgBuf, MsgSize);
}

void gs_log_version_make_compiled(GsVersion *oVersion) {
	GsVersion Ret;
	Ret.mVersion = GS_LOG_VERSION_COMPILED;
	*oVersion = Ret;
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

void gs_log_dump_reset(GsLogDump *ioDump) {
	if (ioDump->mBuf) {
		delete ioDump->mBuf;
		ioDump->mBuf = NULL;
		
		ioDump->mBufSize = 0;
		ioDump->mLenBuf = 0;
	}
}

int gs_log_list_create(GsLogList **oLogList) {
	int r = 0;

	sp<GsLogList> LogList(new GsLogList());

	LogList->mMutexData = sp<std::mutex>(new std::mutex);

	std::lock_guard<std::mutex> lock(*LogList->mMutexData);
	{
		gs_log_version_make_compiled(&LogList->mVersion);
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
			goto clean;

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
			goto clean;

		if (!!(r = gs_log_version_check_compiled(&Log->mVersion)))
			goto clean;

		if (LogList->mLogs->find(Log->mPrefix) != LogList->mLogs->end())
			{ r = 1; goto clean; }

		(*LogList->mLogs)[Log->mPrefix] = Log;
	}

clean:

	return r;
}

int gs_log_list_get_log(GsLogList *LogList, const char *Prefix, GsLogBase **oLog) {
	int r = 0;

	GsLogBase *Log = NULL;

	std::string sPrefix(Prefix);

	std::lock_guard<std::mutex> lock(*LogList->mMutexData);
	{
		gs_log_map_t::iterator it;

		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			goto clean;

		it = LogList->mLogs->find(sPrefix);
		
		if (it == LogList->mLogs->end())
			{ r = 1; goto clean; };

		if (!!(r = gs_log_version_check_compiled(&it->second->mVersion)))
			goto clean;

		Log = it->second;
	}

	if (oLog)
		*oLog = Log;

clean:

	return r;
}

GsLogBase * gs_log_list_get_log_ret(GsLogList *LogList, const char *Prefix) {
	GsLogBase *Log = NULL;

	if (!!gs_log_list_get_log(LogList, Prefix, &Log))
		return NULL;

	return Log;
}

int gs_log_list_dump_all(GsLogList *LogList) {
	int r = 0;

	std::lock_guard<std::mutex> lock(*LogList->mMutexData);
	{
		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			goto clean;

		for (gs_log_map_t::iterator it = LogList->mLogs->begin(); it != LogList->mLogs->end(); it++) {
			GsLogDump LogDump = {};
			GsLogBase *LogBase = it->second;

			if (!!(r = LogBase->mFuncDump(LogBase, &LogDump)))
				goto cleansub;

		cleansub:
			gs_log_dump_reset(&LogDump);

			if (!!r)
				goto clean;
		}
	}

clean:

	return r;
}