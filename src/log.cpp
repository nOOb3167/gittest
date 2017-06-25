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
#include <cstring>

#include <algorithm>
#include <utility>
#include <memory>
#include <string>
#include <deque>
#include <sstream>
#include <stdexcept> // std::runtime_error
#include <atomic>

#include <gittest/misc.h>
#include <gittest/cbuf.h>

#include <gittest/log.h>

/* NOTE: implementation of logging should not itself log (avoid recursion / deadlock)
*    therefore function calls and macros such as GS_GOTO_CLEAN which may result in logging
*    must be avoided inside logging implementation code. */

/* NOTE: GsLogList references GsLog instances.
*    both protect data with mutexes.
*    GsLogList by design calls GsLog operations.
*    if GsLog needs to then either call back into GsLogList,
*    or just calls into GsLogList eg GS_GOTO_CLEAN->GsLog->GsLogList,
*    a deadlock may occur.
*    Against the first problem, recursive mutex may help.
*    Against the second, probably lock mutexes in the same order. */

typedef ::std::map<::std::string, GsLogBase *> gs_log_map_t;

struct GsVersion {
	uint32_t mVersion;
	char dummy[64];
};

struct GsLogList {
	GsVersion mVersion;

	sp<std::mutex> mMutexData;
	sp<gs_log_map_t> mLogs;

	struct GsLogUnified *mLogUnifiedOpt;  /**< owned */
};

struct GsLogTls {
	GsLogBase *mpCurrentLog;
};

typedef int (*gs_log_base_func_message_limit_level_t)(GsLogBase *XKlass, uint32_t Level);
typedef void (*gs_log_base_func_message_log_t)(GsLogBase *XKlass, uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine);
typedef int  (*gs_log_base_func_dump_t)(GsLogBase *XKlass, GsLogDump *oLogDump);
/* lowlevel : intended for use within crash handler - dump without synchronization or memory allocation etc */
typedef int(*gs_log_base_func_dump_lowlevel_t)(GsLogBase *XKlass, void *ctx, gs_bypart_cb_t cb);

struct GsLogBase {
	GsVersion mVersion;
	gs_tripwire_t mTripwire;

	sp<std::mutex> mMutexData;
	std::string mPrefix;
	GsLogBase *mPreviousLog;
	struct GsLogUnified *mLogUnifiedOpt;  /**< notowned */

	gs_log_base_func_message_limit_level_t mFuncMessageLimitLevel;
	gs_log_base_func_message_log_t mFuncMessageLog;
	gs_log_base_func_dump_t mFuncDump;
	gs_log_base_func_dump_lowlevel_t mFuncDumpLowLevel;
};

struct GsLog {
	GsLogBase mBase;
	gs_tripwire_t mTripwire;

	sp<cbuf> mMsg;
	std::atomic<uint32_t> mLogLevelLimit;
};

static GS_THREAD_LOCAL_DESIGNATOR GsLogTls g_tls_log_global = {};

GsLogTls *gs_log_global_get() {
	return &g_tls_log_global;
}

GsLogBase *gs_log_base_cast_(void *Log) {
	GsLogBase *a = (GsLogBase *)Log;
	if (a->mTripwire != GS_TRIPWIRE_LOG_BASE)
		GS_ASSERT(0);
	return a;
}

int gs_log_base_init(GsLogBase *Klass, uint32_t LogLevelLimit, const char *Prefix) {
	int r = 0;

	gs_log_version_make_compiled(&Klass->mVersion);
	Klass->mTripwire = GS_TRIPWIRE_LOG_BASE;

	Klass->mMutexData = sp<std::mutex>(new std::mutex);
	Klass->mPrefix = std::string(Prefix);
	Klass->mPreviousLog = NULL;
	Klass->mLogUnifiedOpt = NULL;

	/* virtual functions */

	Klass->mFuncMessageLimitLevel = NULL;
	Klass->mFuncMessageLog = NULL;
	Klass->mFuncDump = NULL;
	Klass->mFuncDumpLowLevel = NULL;

	{ std::lock_guard<std::mutex> lock(*Klass->mMutexData); }

clean:

	return r;
}

void gs_log_base_enter(GsLogBase *Klass) {
	std::lock_guard<std::mutex> lock(*Klass->mMutexData);

	GsLogTls *lg = gs_log_global_get();
	/* no recursive entry */
	if (Klass->mPreviousLog)
		GS_ASSERT(0);
	Klass->mPreviousLog = Klass;
	std::swap(Klass->mPreviousLog, lg->mpCurrentLog);
}

void gs_log_base_exit(GsLogBase *Klass) {
	std::lock_guard<std::mutex> lock(*Klass->mMutexData);

	GsLogTls *lg = gs_log_global_get();

	std::swap(Klass->mPreviousLog, lg->mpCurrentLog);

	/* presumably previous exit not paired with an entry? */
	if (Klass->mPreviousLog != Klass)
		GS_ASSERT(0);

	Klass->mPreviousLog = NULL;
}

GsLog *gs_log_cast_(void *Log) {
	GsLog *a = (GsLog *)Log;

	if (a->mTripwire != GS_TRIPWIRE_LOG)
		GS_ASSERT(0);

	return a;
}

int gs_log_create(const char *Prefix, GsLog **oLog) {
	int r = 0;

	GsLog *Log = new GsLog;

	const uint32_t DefaultLevel = GS_LOG_LEVEL_INFO;

	if (!!(r = gs_log_base_init(&Log->mBase, DefaultLevel, Prefix)))
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

	sp<cbuf> c;

	if (!!(r = cbuf_setup_cpp(GS_LOG_DEFAULT_SIZE, &c)))
		goto clean;

	Klass->mTripwire = GS_TRIPWIRE_LOG;

	Klass->mMsg = c;
	Klass->mLogLevelLimit = LogLevelLimit;

	/* virtual functions */

	Klass->mBase.mFuncMessageLimitLevel = gs_log_message_limit_level;
	Klass->mBase.mFuncMessageLog = gs_log_message_log;
	Klass->mBase.mFuncDump = gs_log_dump_and_flush;
	Klass->mBase.mFuncDumpLowLevel = gs_log_dump_lowlevel;

	{ std::lock_guard<std::mutex> lock(*Klass->mBase.mMutexData); }

clean:

	return r;
}

int gs_log_message_limit_level(GsLogBase *XKlass, uint32_t Level)
{
	GsLog *Klass = GS_LOG_CAST(XKlass);

	if (Level > Klass->mLogLevelLimit)
		return 1;
	return 0;
}

void gs_log_message_log(GsLogBase *XKlass, uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine) {
	GsLog *Klass = GS_LOG_CAST(XKlass);

	std::lock_guard<std::mutex> lock(*Klass->mBase.mMutexData);

	std::stringstream ss;
	ss << "[" + Klass->mBase.mPrefix + "] [" << CppFile << ":" << CppLine << "]: [" << std::string(MsgBuf, MsgSize) << "]" << std::endl;

	const std::string &ssstr = ss.str();

	if (!!cbuf_push_back_discarding_trunc(Klass->mMsg.get(), ssstr.data(), ssstr.size()))
		GS_ASSERT(0);
}

int gs_log_dump_and_flush(GsLogBase *XKlass, GsLogDump *oLogDump) {
	int r = 0;

	GsLog *Klass = GS_LOG_CAST(XKlass);

	std::string Dump;

	std::lock_guard<std::mutex> lock(*Klass->mBase.mMutexData);

	if (!!(r = cbuf_read_full_bypart_cpp(Klass->mMsg.get(),
		[&Dump](const char *d, int64_t l) {
			Dump.append(d, l);
			Dump.append("\n");
			return 0;
		})))
	{
		goto clean;
	}

	cbuf_clear(Klass->mMsg.get());

	if (oLogDump) {
		const char *DumpCStr = Dump.c_str();
		const size_t LenDumpCStr = strlen(DumpCStr);
		const size_t DumpCStrSize = LenDumpCStr + 1;

		gs_log_dump_reset_to(oLogDump, DumpCStr, DumpCStrSize, LenDumpCStr);
	}

clean:

	return r;
}

/* lowlevel - see comment */
int gs_log_dump_lowlevel(GsLogBase *XKlass, void *ctx, gs_bypart_cb_t cb) {
	int r = 0;

	GsLog *Klass = GS_LOG_CAST(XKlass);

	if (!!(r = cbuf_read_full_bypart(Klass->mMsg.get(), ctx, cb)))
		goto clean;

clean:

	return r;
}

void gs_log_tls_SZ(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf, uint32_t MsgSize){
	GsLogTls *lg = gs_log_global_get();
	if (lg->mpCurrentLog && !!gs_log_message_limit_level(lg->mpCurrentLog, Level))
		return;
	if (lg->mpCurrentLog->mLogUnifiedOpt)
		gs_log_unified_message_log(
			lg->mpCurrentLog->mLogUnifiedOpt,
			lg->mpCurrentLog->mPrefix.c_str(),
			Level, MsgBuf, MsgSize, CppFile, CppLine);
	if (lg->mpCurrentLog)
		lg->mpCurrentLog->mFuncMessageLog(lg->mpCurrentLog, Level, MsgBuf, MsgSize, CppFile, CppLine);
}

void gs_log_tls_S(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf){
	const size_t sanity_arbitrary_max = 2048;
	size_t MsgSize = strnlen(MsgBuf, sanity_arbitrary_max);
	GS_ASSERT(MsgSize < sanity_arbitrary_max);

	GsLogTls *lg = gs_log_global_get();
	if (lg->mpCurrentLog && !!gs_log_message_limit_level(lg->mpCurrentLog, Level))
		return;
	if (lg->mpCurrentLog->mLogUnifiedOpt)
		gs_log_unified_message_log(
			lg->mpCurrentLog->mLogUnifiedOpt,
			lg->mpCurrentLog->mPrefix.c_str(),
			Level, MsgBuf, MsgSize, CppFile, CppLine);
	if (lg->mpCurrentLog)
		lg->mpCurrentLog->mFuncMessageLog(lg->mpCurrentLog, Level, MsgBuf, MsgSize, CppFile, CppLine);
}

void gs_log_tls_PF(const char *CppFile, int CppLine, uint32_t Level, const char *Format, ...) {
	GsLogTls *lg = gs_log_global_get();
	if (lg->mpCurrentLog && !!gs_log_message_limit_level(lg->mpCurrentLog, Level))
		return;

	const size_t sanity_arbitrary_max = 2048;
	char buf[sanity_arbitrary_max] = {};
	int numwrite = 0;

	va_list argp;
	va_start(argp, Format);

	if ((numwrite = vsnprintf(buf, sizeof buf, Format, argp)) == -1)
		GS_ASSERT(0);
	if (numwrite >= sizeof buf)
		GS_ASSERT(0);

	va_end(argp);

	size_t MsgSize = strnlen(buf, sanity_arbitrary_max);
	GS_ASSERT(MsgSize < sanity_arbitrary_max);

	if (lg->mpCurrentLog->mLogUnifiedOpt)
		gs_log_unified_message_log(
			lg->mpCurrentLog->mLogUnifiedOpt,
			lg->mpCurrentLog->mPrefix.c_str(),
			Level, buf, MsgSize, CppFile, CppLine);
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

void gs_log_dump_reset_to_noalloc(GsLogDump *ioDump, char *Buf, size_t BufSize, size_t LenBuf) {
	gs_log_dump_reset(ioDump);

	GS_ASSERT(Buf || (!Buf && !BufSize && !LenBuf));

	ioDump->mBuf = Buf;
	ioDump->mBufSize = BufSize;
	ioDump->mLenBuf = LenBuf;
}

void gs_log_dump_reset_to(GsLogDump *ioDump, const char *Buf, size_t BufSize, size_t LenBuf) {
	GS_ASSERT(Buf || (!Buf && !BufSize && !LenBuf));

	char *NewBuf = new char[BufSize];

	memcpy(NewBuf, Buf, BufSize);

	gs_log_dump_reset_to_noalloc(ioDump, NewBuf, BufSize, LenBuf);
}

int gs_log_list_create(GsLogList **oLogList) {
	int r = 0;

	GsLogList *LogList = new GsLogList();

	struct GsLogUnified *LogUnified = NULL;

	gs_log_version_make_compiled(&LogList->mVersion);

	LogList->mMutexData = sp<std::mutex>(new std::mutex);
	LogList->mLogs = sp<gs_log_map_t>(new gs_log_map_t);
	LogList->mLogUnifiedOpt = NULL;

	if (!!(r = gs_log_unified_create(&LogUnified)))
		goto clean;

	if (!!(r = gs_log_list_set_log_unified(LogList, LogUnified)))
		goto clean;

	{ std::lock_guard<std::mutex> lock(*LogList->mMutexData); }

	if (oLogList)
		*oLogList = LogList;

clean:

	return r;
}

GsLogList *gs_log_list_global_create_cpp() {
	int r = 0;

	GsLogList *LogList = NULL;

	if (!!(r = gs_log_list_create(&LogList))) {
		/* NOTE: c++ API (exception throwing) */
		GS_ASSERT(0);
		throw std::runtime_error("[ERROR] exception from gs_log_list_global_create_cpp");
	}

	return LogList;
}

int gs_log_list_free(GsLogList *LogList) {
	int r = 0;

	/* NOTE: special deletion protocol - managed by shared_ptr technically */

	{
		std::lock_guard<std::mutex> lock(*LogList->mMutexData);

		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			goto clean;

		GS_DELETE_F(&LogList->mLogUnifiedOpt, gs_log_unified_destroy);
	}

	GS_DELETE(&LogList, GsLogList);

clean:

	return r;
}

int gs_log_list_set_log_unified(GsLogList *LogList, struct GsLogUnified *LogUnified)
{
	int r = 0;

	{
		std::lock_guard<std::mutex> lock(*LogList->mMutexData);

		if (LogList->mLogUnifiedOpt)
			{ r = 1; goto clean; }

		LogList->mLogUnifiedOpt = LogUnified;
	}

clean:

	return r;
}

/** add GsLog to GsLogList and bind GsLogUnified */
int gs_log_list_add_log(GsLogList *LogList, GsLogBase *Log) {
	int r = 0;

	{
		std::lock_guard<std::mutex> lock1(*LogList->mMutexData);
		std::lock_guard<std::mutex> lock2(*Log->mMutexData);

		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			goto clean;

		if (!!(r = gs_log_version_check_compiled(&Log->mVersion)))
			goto clean;

		if (LogList->mLogs->find(Log->mPrefix) != LogList->mLogs->end())
			{ r = 1; goto clean; }

		Log->mLogUnifiedOpt = LogList->mLogUnifiedOpt;

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

GsLogBase * gs_log_list_get_log_ret_2(GsLogList *LogList, const char *Prefix1, const char *optPrefix2)
{
	GsLogBase *Log = NULL;

	std::string Name(Prefix1);

	if (optPrefix2)
		Name.append(optPrefix2);

	if (!!gs_log_list_get_log(LogList, Name.c_str(), &Log))
		return NULL;

	return Log;
}

/* lowlevel - see comment */
int gs_log_list_dump_all_lowlevel(GsLogList *LogList, void *ctx, gs_bypart_cb_t cb) {
	int r = 0;

	if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
		goto clean;

	// FIXME: eventually have the loggers also on a c style linked list to avoid c++ container access
	for (gs_log_map_t::iterator it = LogList->mLogs->begin(); it != LogList->mLogs->end(); it++) {
		GsLogBase *LogBase = it->second;
		
		size_t LenHeader = 0;
		char Header[64] = {};

		if (!!(r = gs_log_dump_construct_header_(
			LogBase->mPrefix.data(), LogBase->mPrefix.size(),
			Header, sizeof Header, &LenHeader)))
		{
			goto clean;
		}

		if (!!(r = cb(ctx, Header, LenHeader)))
			goto clean;

		if (!!(r = LogBase->mFuncDumpLowLevel(LogBase, ctx, cb)))
			goto clean;
	}

clean:

	return r;
}

int gs_log_list_dump_all(GsLogList *LogList, GsLogDump *oRetDump) {
	int r = 0;

	std::string RetString;

	std::stringstream ss;

	std::lock_guard<std::mutex> lock(*LogList->mMutexData);
	{
		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			goto clean;

		for (gs_log_map_t::iterator it = LogList->mLogs->begin(); it != LogList->mLogs->end(); it++) {
			GsLogDump LogDump = {};
			GsLogBase *LogBase = it->second;

			if (!!(r = LogBase->mFuncDump(LogBase, &LogDump)))
				goto cleansub;

			ss << "=[= " << LogBase->mPrefix << " =]=" << std::endl;

			ss.write(LogDump.mBuf, LogDump.mLenBuf);

			ss << std::endl;

		cleansub:
			gs_log_dump_reset(&LogDump);

			if (!!r)
				goto clean;
		}
	}

	RetString = ss.str();

	if (oRetDump) {
		const char *RetCStr = RetString.c_str();
		const size_t LenRetCStr = strlen(RetCStr);
		const size_t RetCStrSize = LenRetCStr + 1;

		gs_log_dump_reset_to(oRetDump, RetCStr, RetCStrSize, LenRetCStr);
	}

clean:

	return r;
}

int gs_log_create_common_logs() {
	int r = 0;

	GS_LOG_ADD(gs_log_create_ret("selfup"));

	GS_LOG_ADD(gs_log_create_ret("serv"));
	GS_LOG_ADD(gs_log_create_ret("clnt_worker"));
	GS_LOG_ADD(gs_log_create_ret("clnt_aux"));
	GS_LOG_ADD(gs_log_create_ret("clnt_serv"));
	GS_LOG_ADD(gs_log_create_ret("serv_worker"));
	GS_LOG_ADD(gs_log_create_ret("serv_aux"));
	GS_LOG_ADD(gs_log_create_ret("serv_serv"));

	// net2
	GS_LOG_ADD(gs_log_create_ret("ntwk_clnt"));
	GS_LOG_ADD(gs_log_create_ret("work_clnt"));
	GS_LOG_ADD(gs_log_create_ret("ntwk_serv"));
	GS_LOG_ADD(gs_log_create_ret("work_serv"));
	GS_LOG_ADD(gs_log_create_ret("ntwk_selfup"));
	GS_LOG_ADD(gs_log_create_ret("work_selfup"));

	GS_LOG_ADD(gs_log_create_ret("ntwk_test02"));
	GS_LOG_ADD(gs_log_create_ret("work_test02"));

clean:

	return r;
}

int gs_log_dump_construct_header_(
	const char *PrefixBuf, size_t PrefixSize,
	char *ioHeaderBuf, size_t HeaderSize, size_t *oLenHeader)
{
	int r = 0;

	const char LumpA[] = "\n=[= ";
	const char LumpB[] = " =]=\n";
	const uint32_t LenLump = 5;
	const uint32_t PrefixTrunc = GS_MIN(PrefixSize, HeaderSize - 2 * LenLump);
	const uint32_t NumToWrite = PrefixTrunc + 2 * LenLump;

	if (NumToWrite > HeaderSize)
		{ r = 1; goto clean; }

	memcpy(ioHeaderBuf + 0, LumpA, LenLump);
	memcpy(ioHeaderBuf + LenLump, PrefixBuf, PrefixTrunc);
	memcpy(ioHeaderBuf + LenLump + PrefixTrunc, LumpB, LenLump);

	if (oLenHeader)
		*oLenHeader = NumToWrite;

clean:

	return r;
}

int gs_log_crash_handler_dump_global_log_list_suffix_2(
	const char *SuffixBuf1, const char *SuffixBuf2)
{
	std::string ThreadName(SuffixBuf1);

	if (SuffixBuf2)
		ThreadName.append(SuffixBuf2);

	return gs_log_crash_handler_dump_global_log_list_suffix(ThreadName.c_str(), ThreadName.size());
}
