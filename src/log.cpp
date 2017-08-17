#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include <atomic>
#include <mutex>
#include <sstream>

#include <gittest/misc.h>
#include <gittest/cbuf.h>
#include <gittest/filesys.h>

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

#define GS_LOG_VERSION_COMPILED 0x00010000

#define GS_TRIPWIRE_LOG_BASE 0xA37F4680
#define GS_TRIPWIRE_LOG      0xA37F4681

#define GS_LOG_DEFAULT_SIZE 64 * 1024 /* 64KB */

#define GS_LOG_PREFIX_MAX 256

struct GsLogListNode
{
	struct GsLogBase     *mLog;
	struct GsLogListNode *mNext;
};

struct GsVersion {
	uint32_t mVersion;
	char dummy[64];
};

struct GsLogList {
	struct GsVersion mVersion;

	std::mutex mMutexData;
	struct GsLogListNode *mLogs;

	struct GsLogUnified *mLogUnifiedOpt;  /**< owned */
};

struct GsLogTls {
	struct GsLogBase *mpCurrentLog;
};

typedef int(*gs_log_base_func_message_limit_level_t)(GsLogBase *XKlass, uint32_t Level);
typedef void(*gs_log_base_func_message_log_t)(GsLogBase *XKlass, uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine);
/* lowlevel : intended for use within crash handler - dump without synchronization or memory allocation etc */
typedef int(*gs_log_base_func_dump_lowlevel_t)(GsLogBase *XKlass, void *ctx, gs_bypart_cb_t cb);

struct GsLogBase {
	struct GsVersion mVersion;
	uint32_t mMagic;

	std::mutex mMutexData;
	char mPrefixBuf[GS_LOG_PREFIX_MAX]; size_t mLenPrefix;
	struct GsLogBase *mPreviousLog;
	struct GsLogUnified *mLogUnifiedOpt;  /**< notowned */
	std::atomic<uint32_t> mLogLevelLimit;
	cbuf mMsg;

	gs_log_base_func_message_limit_level_t mFuncMessageLimitLevel;
	gs_log_base_func_message_log_t mFuncMessageLog;
	gs_log_base_func_dump_lowlevel_t mFuncDumpLowLevel;
};

static GS_THREAD_LOCAL_DESIGNATOR GsLogTls g_tls_log_global = {};

static int gs_log_message_limit_level(struct GsLogBase *Base, uint32_t Level);
static void gs_log_message_log(struct GsLogBase *Base, uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine);
static int gs_log_dump_lowlevel(struct GsLogBase *Base, void *ctx, gs_bypart_cb_t cb);

int gs_log_dump_construct_header_(
	const char *PrefixBuf, size_t PrefixSize,
	char *ioHeaderBuf, size_t HeaderSize, size_t *oLenHeader);

int gs_log_message_limit_level(struct GsLogBase *Base, uint32_t Level)
{
	// NOTE: no locking because mLogLevelLimit is std::atomic.. too clever for own good?
	if (Level > Base->mLogLevelLimit)
		return 1;
	return 0;
}

void gs_log_message_log(struct GsLogBase *Base, uint32_t Level, const char *MsgBuf, uint32_t MsgSize, const char *CppFile, int CppLine)
{
	std::lock_guard<std::mutex> lock(Base->mMutexData);

	std::stringstream ss;
	ss << "[" + std::string(Base->mPrefixBuf, Base->mLenPrefix) + "] [" << CppFile << ":" << CppLine << "]: [" << std::string(MsgBuf, MsgSize) << "]" << std::endl;

	const std::string &out = ss.str();

	if (!!cbuf_push_back_discarding_trunc(&Base->mMsg, out.data(), out.size()))
		GS_ASSERT(0);
}

int gs_log_dump_lowlevel(struct GsLogBase *Base, void *ctx, gs_bypart_cb_t cb)
{
	int r = 0;

	if (!!(r = cbuf_read_full_bypart(&Base->mMsg, ctx, cb)))
		goto clean;

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

int gs_log_crash_handler_dump_buf_cb(void *ctx, const char *d, int64_t l)
{
	GsLogCrashHandlerDumpBufData *Data = (GsLogCrashHandlerDumpBufData *)ctx;

	size_t WritePos = Data->CurrentWritePos;

	Data->CurrentWritePos += l;

	if (Data->Tripwire != GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_BUF_DATA)
		return 1;

	/* NOTE: return zero - if over limit, just avoid writing anything */
	if (Data->CurrentWritePos > Data->MaxWritePos)
		return 0;

	memmove(&Data->Buf[WritePos], d, l);

	return 0;
}

void gs_log_version_make_compiled(struct GsVersion *oVersion)
{
	struct GsVersion Ret;
	Ret.mVersion = GS_LOG_VERSION_COMPILED;
	*oVersion = Ret;
}

int gs_log_version_check_compiled(struct GsVersion *other)
{
	if (other->mVersion != GS_LOG_VERSION_COMPILED)
		return 1;
	return 0;
}

// FIXME: version with PrefixBuf, LenPrefix
struct GsLogBase * gs_log_base_create_ret(const char *Prefix)
{
	struct GsLogBase *Base = NULL;

	const uint32_t DefaultLevel = GS_LOG_LEVEL_INFO;

	if (!!gs_log_base_create(DefaultLevel, Prefix, strlen(Prefix), &Base))
		GS_ASSERT(0);

	return Base;
}

int gs_log_base_create(
	uint32_t LogLevelLimit,
	const char *PrefixBuf, size_t LenPrefix,
	struct GsLogBase **oBase)
{
	int r = 0;

	struct GsLogBase *Base = new GsLogBase();

	gs_log_version_make_compiled(&Base->mVersion);
	Base->mMagic = GS_TRIPWIRE_LOG_BASE;

	Base->mMutexData; /* dummy */
	Base->mPreviousLog = NULL;
	Base->mLogUnifiedOpt = NULL;
	Base->mLogLevelLimit = LogLevelLimit;
	Base->mMsg = {};

	if (!!(r = gs_buf_copy_zero_terminate(
		PrefixBuf, LenPrefix,
		Base->mPrefixBuf, GS_LOG_PREFIX_MAX, &Base->mLenPrefix)))
	{
		goto clean;
	}

	if (!!(r = cbuf_setup(GS_LOG_DEFAULT_SIZE, &Base->mMsg)))
		goto clean;

	/* virtual functions */

	Base->mFuncMessageLimitLevel = gs_log_message_limit_level;
	Base->mFuncMessageLog = gs_log_message_log;
	Base->mFuncDumpLowLevel = gs_log_dump_lowlevel;

	if (oBase)
		*oBase = Base;

clean:
	if (!!r) {
		GS_DELETE(&Base, GsLogBase);
	}

	return r;
}

int gs_log_base_destroy(struct GsLogBase *Base)
{
	GS_DELETE(&Base, GsLogBase);
	return 0;
}

void gs_log_base_enter(struct GsLogBase *Base)
{
	std::lock_guard<std::mutex> lock(Base->mMutexData);

	/* no recursive entry */
	if (Base->mPreviousLog)
		GS_ASSERT(0);
	Base->mPreviousLog = Base;
	std::swap(Base->mPreviousLog, g_tls_log_global.mpCurrentLog);
}

void gs_log_base_exit(struct GsLogBase *Base)
{
	std::lock_guard<std::mutex> lock(Base->mMutexData);

	std::swap(Base->mPreviousLog, g_tls_log_global.mpCurrentLog);

	/* presumably previous exit not paired with an entry? */
	if (Base->mPreviousLog != Base)
		GS_ASSERT(0);

	Base->mPreviousLog = NULL;
}

struct GsLogList * gs_log_list_global_create()
{
	int r = 0;

	struct GsLogList *LogList = NULL;

	if (!!(r = gs_log_list_create(&LogList)))
		GS_ASSERT(0);

	return LogList;
}

int gs_log_list_create(struct GsLogList **oLogList)
{
	int r = 0;

	struct GsLogList *LogList = new GsLogList();

	struct GsLogUnified *LogUnified = NULL;

	gs_log_version_make_compiled(&LogList->mVersion);

	LogList->mMutexData; /* dummy */
	LogList->mLogs = NULL; /* empty list node */
	LogList->mLogUnifiedOpt = NULL;

	if (!!(r = gs_log_unified_create(&LogUnified)))
		goto clean;

	if (!!(r = gs_log_list_set_log_unified(LogList, LogUnified)))
		goto clean;

	if (oLogList)
		*oLogList = LogList;

clean:

	return r;
}

int gs_log_list_free(struct GsLogList *LogList)
{
	int r = 0;

	{
		std::lock_guard<std::mutex> lock(LogList->mMutexData);

		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			goto clean;

		GS_DELETE_F(&LogList->mLogUnifiedOpt, gs_log_unified_destroy);
	}

	GS_DELETE(&LogList, GsLogList);

clean:

	return r;
}

int gs_log_list_set_log_unified(struct GsLogList *LogList, struct GsLogUnified *LogUnified)
{
	int r = 0;

	{
		std::lock_guard<std::mutex> lock(LogList->mMutexData);

		if (LogList->mLogUnifiedOpt)
			{ r = 1; goto clean; }

		LogList->mLogUnifiedOpt = LogUnified;
	}

clean:

	return r;
}

/** add GsLog to GsLogList and bind GsLogUnified */
int gs_log_list_add_log(struct GsLogList *LogList, struct GsLogBase *Log)
{
	int r = 0;

	{
		std::lock_guard<std::mutex> lock1(LogList->mMutexData);
		std::lock_guard<std::mutex> lock2(Log->mMutexData);

		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			goto clean;

		if (!!(r = gs_log_version_check_compiled(&Log->mVersion)))
			goto clean;

		/* list contains log already */
		for (struct GsLogListNode *Node = LogList->mLogs; Node != NULL; Node = Node->mNext)
			if (strcmp(Log->mPrefixBuf, Node->mLog->mPrefixBuf) == 0)
				{ r = 1; goto clean; }

		Log->mLogUnifiedOpt = LogList->mLogUnifiedOpt;

		/* list prepended with log */
		{
			struct GsLogListNode *Node = new GsLogListNode();
			Node->mLog = Log;
			Node->mNext = LogList->mLogs;

			LogList->mLogs = Node;
		}
	}

clean:

	return r;
}

int gs_log_list_get_log(struct GsLogList *LogList, const char *Prefix, struct GsLogBase **oLog)
{
	int r = 0;

	struct GsLogBase *Log = NULL;

	{
		std::lock_guard<std::mutex> lock(LogList->mMutexData);

		struct GsLogListNode *Node = NULL;

		if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
			goto clean;

		/* does list contain log */
		for (Node = LogList->mLogs; Node != NULL; Node = Node->mNext)
			if (strncmp(Prefix, Node->mLog->mPrefixBuf, Node->mLog->mLenPrefix) == 0)
				break;

		if (Node == NULL)
			{ r = 1; goto clean; }

		if (!!(r = gs_log_version_check_compiled(&Node->mLog->mVersion)))
			goto clean;

		Log = Node->mLog;
	}

	if (oLog)
		*oLog = Log;

clean:

	return r;
}

int gs_log_list_dump_all_lowlevel(GsLogList *LogList, void *ctx, gs_bypart_cb_t cb)
{
	int r = 0;

	if (!!(r = gs_log_version_check_compiled(&LogList->mVersion)))
		goto clean;

	for (struct GsLogListNode *Node = LogList->mLogs; Node != NULL; Node = Node->mNext) {
		size_t LenHeader = 0;
		char Header[256] = {};

		if (!!(r = gs_log_dump_construct_header_(
			Node->mLog->mPrefixBuf, Node->mLog->mLenPrefix,
			Header, sizeof Header, &LenHeader)))
		{
			goto clean;
		}

		if (!!(r = cb(ctx, Header, LenHeader)))
			goto clean;

		if (!!(r = Node->mLog->mFuncDumpLowLevel(Node->mLog, ctx, cb)))
			goto clean;
	}

clean:

	return r;
}

struct GsLogBase * gs_log_list_get_log_ret(struct GsLogList *LogList, const char *Prefix)
{
	struct GsLogBase *Log = NULL;

	if (!!gs_log_list_get_log(LogList, Prefix, &Log))
		return NULL;

	return Log;
}

struct GsLogBase * gs_log_list_get_log_ret_2(struct GsLogList *LogList, const char *Prefix1, const char *optPrefix2)
{
	struct GsLogBase *Log = NULL;

	std::string Name(Prefix1);

	if (optPrefix2)
		Name.append(optPrefix2);

	if (!!gs_log_list_get_log(LogList, Name.c_str(), &Log))
		return NULL;

	return Log;
}

int gs_log_crash_handler_dump_global_log_list_suffix(
	const char *SuffixBuf, size_t LenSuffix)
{
	int r = 0;

	size_t LenCombinedExtraSuffix = 0;
	char CombinedExtraSuffix[512];

	size_t LenCurrentFileName = 0;
	char CurrentFileNameBuf[512];
	
	size_t LenLogFileName = 0;
	char LogFileNameBuf[512];

	char *DumpBuf = NULL;
	size_t LenDump = 0;

	if ((LenCombinedExtraSuffix = strlen(GS_LOG_STR_EXTRA_SUFFIX) + LenSuffix)
		>= sizeof CombinedExtraSuffix)
		{ r = 1; goto clean; }

	memcpy(CombinedExtraSuffix, GS_LOG_STR_EXTRA_SUFFIX, strlen(GS_LOG_STR_EXTRA_SUFFIX));
	memcpy(CombinedExtraSuffix + strlen(GS_LOG_STR_EXTRA_SUFFIX), SuffixBuf, LenSuffix);
	memset(CombinedExtraSuffix + LenCombinedExtraSuffix, '\0', 1);

	if (!!(r = gs_get_current_executable_filename(CurrentFileNameBuf, sizeof CurrentFileNameBuf, &LenCurrentFileName)))
		goto clean;

	if (!!(r = gs_build_modified_filename(
		CurrentFileNameBuf, LenCurrentFileName,
		"", 0,
		GS_STR_EXECUTABLE_EXPECTED_EXTENSION, strlen(GS_STR_EXECUTABLE_EXPECTED_EXTENSION),
		CombinedExtraSuffix, LenCombinedExtraSuffix,
		GS_LOG_STR_EXTRA_EXTENSION, strlen(GS_LOG_STR_EXTRA_EXTENSION),
		LogFileNameBuf, sizeof LogFileNameBuf, &LenLogFileName)))
	{
		goto clean;
	}

	printf("Dumping Logs To: [%.*s]\n", (int)LenLogFileName, LogFileNameBuf);

	if (!(DumpBuf = (char *) malloc(GS_ARBITRARY_LOG_DUMP_FILE_LIMIT_BYTES)))
		goto clean;
	LenDump = GS_ARBITRARY_LOG_DUMP_FILE_LIMIT_BYTES;
	gs_debug_break();
	{
		GsLogCrashHandlerDumpBufData Data = {};
		Data.Tripwire = GS_TRIPWIRE_LOG_CRASH_HANDLER_DUMP_BUF_DATA;
		Data.Buf = DumpBuf;
		Data.MaxWritePos = LenDump;
		Data.CurrentWritePos = 0;

		if (!!(r = gs_log_list_dump_all_lowlevel(GS_LOG_LIST_GLOBAL_NAME, &Data, gs_log_crash_handler_dump_buf_cb)))
			goto clean;

		if (!!(r = gs_file_write_frombuffer(
			LogFileNameBuf, LenLogFileName,
			(uint8_t *) Data.Buf, GS_MIN(Data.CurrentWritePos, LenDump))))
		{
			goto clean;
		}
	}

clean:
	if (DumpBuf)
		free(DumpBuf);

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

void gs_log_tls_SZ(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf, uint32_t MsgSize)
{
	if (g_tls_log_global.mpCurrentLog && !!gs_log_message_limit_level(g_tls_log_global.mpCurrentLog, Level))
		return;
	if (g_tls_log_global.mpCurrentLog && g_tls_log_global.mpCurrentLog->mLogUnifiedOpt)
		gs_log_unified_message_log(
			g_tls_log_global.mpCurrentLog->mLogUnifiedOpt,
			g_tls_log_global.mpCurrentLog->mPrefixBuf,
			Level, MsgBuf, MsgSize, CppFile, CppLine);
	if (g_tls_log_global.mpCurrentLog)
		g_tls_log_global.mpCurrentLog->mFuncMessageLog(g_tls_log_global.mpCurrentLog, Level, MsgBuf, MsgSize, CppFile, CppLine);
}

void gs_log_tls_S(const char *CppFile, int CppLine, uint32_t Level, const char *MsgBuf)
{
	if (g_tls_log_global.mpCurrentLog && !!gs_log_message_limit_level(g_tls_log_global.mpCurrentLog, Level))
		return;

	const size_t sanity_arbitrary_max = 2048;
	size_t MsgSize = strnlen(MsgBuf, sanity_arbitrary_max);
	GS_ASSERT(MsgSize < sanity_arbitrary_max);

	gs_log_tls_SZ(CppFile, CppLine, Level, MsgBuf, MsgSize);
}

void gs_log_tls_PF(const char *CppFile, int CppLine, uint32_t Level, const char *Format, ...)
{
	if (g_tls_log_global.mpCurrentLog && !!gs_log_message_limit_level(g_tls_log_global.mpCurrentLog, Level))
		return;

	const size_t sanity_arbitrary_max = 2048;
	size_t MsgSize = 0;
	char buf[sanity_arbitrary_max] = {};
	int numwrite = 0;

	va_list argp;
	va_start(argp, Format);

	if ((numwrite = vsnprintf(buf, sizeof buf, Format, argp)) == -1)
		GS_ASSERT(0);
	if (numwrite >= sizeof buf)
		GS_ASSERT(0);

	va_end(argp);

	MsgSize = strnlen(buf, sanity_arbitrary_max);
	GS_ASSERT(MsgSize < sanity_arbitrary_max);

	gs_log_tls_SZ(CppFile, CppLine, Level, buf, MsgSize);
}
