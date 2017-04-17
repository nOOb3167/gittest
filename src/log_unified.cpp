#include <stddef.h>

#include <mutex>

#include <sqlite3.h>

#include <gittest/misc.h>
#include <gittest/gittest_selfupdate.h>

#include <gittest/log.h>

/** @sa
       ::gs_log_unified_create
	   ::gs_log_unified_destroy
*/
struct GsLogUnified {
	std::mutex mMutexData;

	sqlite3 *mSqlite;
	sqlite3_stmt *mSqliteStmtTableCreate;
	sqlite3_stmt *mSqliteStmtLogInsert;
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

	// FIXME: hardcoded 4 minute timeout
	if (SQLITE_OK != (r = sqlite3_busy_timeout(Sqlite, 240000)))
		GS_GOTO_CLEAN();

	// FIXME: sqlite disable sync because why not
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

	/* create table */

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
		const char *qqmore = sqlite3_errmsg(Sqlite);
		GS_GOTO_CLEAN();
	}

	if (SQLITE_OK != (r = sqlite3_bind_text(SqliteStmtLogInsert, 1, "hello", -1, SQLITE_TRANSIENT)))
		GS_GOTO_CLEAN();
	if (SQLITE_DONE != (r = sqlite3_step(SqliteStmtLogInsert)))
		GS_GOTO_CLEAN();
	if (SQLITE_OK != (r = sqlite3_clear_bindings(SqliteStmtLogInsert)))
		GS_GOTO_CLEAN();
	if (SQLITE_OK != (r = sqlite3_reset(SqliteStmtLogInsert)))
		GS_GOTO_CLEAN();

	LogUnified->mSqlite = Sqlite;
	LogUnified->mSqliteStmtTableCreate = SqliteStmtTableCreate;
	LogUnified->mSqliteStmtLogInsert = SqliteStmtLogInsert;

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
