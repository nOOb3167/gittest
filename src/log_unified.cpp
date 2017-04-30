#include <stddef.h>

#include <mutex>
#include <sstream>

#include <sqlite3.h>

#include <gittest/misc.h>
#include <gittest/gittest_selfupdate.h>

#include <gittest/log.h>

/** @sa
       ::gs_log_unified_create
	   ::gs_log_unified_destroy
	   ::gs_log_unified_message_log
*/
struct GsLogUnified {
	std::mutex mMutexData;

	sqlite3 *mSqlite;
	sqlite3_stmt *mSqliteStmtTableCreate;
	sqlite3_stmt *mSqliteStmtLogInsert;

	char mCurExeNameBuf[512]; size_t mLenCurExeName;
};

int gs_log_unified_create(struct GsLogUnified **oLogUnified)
{
	int r = 0;

	struct GsLogUnified *LogUnified = new GsLogUnified();

	const char DbFileNameBuf[] = "gittest_unified_log.sqlite";
	size_t LenDbPath = 0;
	char DbPathBuf[512] = {0};

	sqlite3 *Sqlite = NULL;
	sqlite3_stmt *SqliteStmtTableCreate = NULL;
	sqlite3_stmt *SqliteStmtLogInsert = NULL;

	if (!!(r = gs_build_path_interpret_relative_current_executable(
		DbFileNameBuf, sizeof DbFileNameBuf,
		DbPathBuf, sizeof DbPathBuf, &LenDbPath)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_buf_ensure_haszero(DbPathBuf, LenDbPath + 1)))
		GS_GOTO_CLEAN();

	if (SQLITE_OK != (r = sqlite3_open_v2(
		DbPathBuf,
		&Sqlite,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

	/* NOTE: hardcoded 4 minute timeout */
	if (SQLITE_OK != (r = sqlite3_busy_timeout(Sqlite, 240000)))
		GS_GOTO_CLEAN();

	/* NOTE: sqlite disable sync because why not */
	if (SQLITE_OK != (r = sqlite3_exec(Sqlite, "PRAGMA synchronous=OFF;", NULL, NULL, NULL)))
		GS_GOTO_CLEAN();

	// https://sqlite.org/autoinc.html
	//   sqlite autoincrement can be achieved by inserting NULL into INTEGER PRIMARY KEY
	if (SQLITE_OK != (r = sqlite3_prepare_v2(
		Sqlite,
		"CREATE TABLE IF NOT EXISTS LogTable (id INTEGER PRIMARY KEY, msg TEXT);",
		-1,
		&SqliteStmtTableCreate,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

	/* create table (esp before preparing statements using it) */

	if (SQLITE_DONE != (r = sqlite3_step(SqliteStmtTableCreate)))
		GS_GOTO_CLEAN();

	if (SQLITE_OK != (r = sqlite3_clear_bindings(SqliteStmtTableCreate)))
		GS_GOTO_CLEAN();
	if (SQLITE_OK != (r = sqlite3_reset(SqliteStmtTableCreate)))
		GS_GOTO_CLEAN();

	if (SQLITE_OK != (r = sqlite3_prepare_v2(
		Sqlite,
		"INSERT INTO LogTable (id, msg) VALUES (NULL, ?)",
		-1,
		&SqliteStmtLogInsert,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}


	LogUnified->mSqlite = Sqlite;
	LogUnified->mSqliteStmtTableCreate = SqliteStmtTableCreate;
	LogUnified->mSqliteStmtLogInsert = SqliteStmtLogInsert;
	if (!!(r = gs_get_current_executable_filename(LogUnified->mCurExeNameBuf, 512, &LogUnified->mLenCurExeName)))
		GS_GOTO_CLEAN();
	if (!!(r = gs_path_kludge_filenameize(LogUnified->mCurExeNameBuf, &LogUnified->mLenCurExeName)))
		GS_GOTO_CLEAN();


	{ std::lock_guard<std::mutex> lock(LogUnified->mMutexData); }

	if (oLogUnified)
		*oLogUnified = LogUnified;

clean:
	if (!!r) {
		if (SQLITE_OK != sqlite3_finalize(SqliteStmtLogInsert))
			GS_ASSERT(0);

		if (SQLITE_OK != sqlite3_finalize(SqliteStmtTableCreate))
			GS_ASSERT(0);

		if (SQLITE_OK != sqlite3_close(Sqlite))
			GS_ASSERT(0);
	}

	return r;
}

int gs_log_unified_destroy(struct GsLogUnified *LogUnified)
{
	if (! LogUnified)
		return 0;

	{
		std::lock_guard<std::mutex> lock(LogUnified->mMutexData);

		if (SQLITE_OK != sqlite3_finalize(LogUnified->mSqliteStmtTableCreate))
			GS_ASSERT(0);

		if (SQLITE_OK != sqlite3_finalize(LogUnified->mSqliteStmtLogInsert))
			GS_ASSERT(0);

		if (SQLITE_OK != sqlite3_close(LogUnified->mSqlite))
			GS_ASSERT(0);
	}

	GS_DELETE(&LogUnified);

	return 0;
}

int gs_log_unified_message_log(
	GsLogUnified *LogUnified,
	const char *Prefix,
	uint32_t Level,
	const char *MsgBuf,
	uint32_t MsgSize,
	const char *CppFile,
	int CppLine)
{
	int r = 0;

	std::stringstream ss;
	ss  << "[" + std::string(LogUnified->mCurExeNameBuf, LogUnified->mLenCurExeName) + "] "
		<< "[" + std::string(Prefix) + "] "
		<< "[" << CppFile << ":" << CppLine << "]: "
		<< "[" << std::string(MsgBuf, MsgSize) << "]"
		<< std::endl;

	const std::string &ssstr = ss.str();

	{
		std::lock_guard<std::mutex> lock(LogUnified->mMutexData);

		if (SQLITE_OK != (r = sqlite3_bind_text(LogUnified->mSqliteStmtLogInsert, 1, ssstr.c_str(), -1, SQLITE_TRANSIENT)))
			GS_GOTO_CLEAN();
		if (SQLITE_DONE != (r = sqlite3_step(LogUnified->mSqliteStmtLogInsert)))
			GS_GOTO_CLEAN();
		if (SQLITE_OK != (r = sqlite3_clear_bindings(LogUnified->mSqliteStmtLogInsert)))
			GS_GOTO_CLEAN();
		if (SQLITE_OK != (r = sqlite3_reset(LogUnified->mSqliteStmtLogInsert)))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}
