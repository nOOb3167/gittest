#include <cstdlib>

#include <sqlite3.h>

#include <gittest/misc.h>
#include <gittest/log.h>

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

int callback_printout_sqlite(
	void *Ctx,
	int numcols,
	char **cols,
	char **colnames)
{
	GS_ASSERT(numcols == 1);

	printf("%s\n", cols[0]);

	return 0;
}

int logconvert() {
	int r = 0;

	const char DbFileNameBuf[] = "gittest_unified_log.sqlite";
	size_t LenDbPath = 0;
	char DbPathBuf[512] = {0};

	sqlite3 *Sqlite = NULL;

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
		SQLITE_OPEN_READONLY,
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

	if (SQLITE_OK != (r = sqlite3_exec(
		Sqlite,
		"SELECT msg FROM LogTable as L ORDER BY L.id;",
		callback_printout_sqlite,
		NULL,
		NULL)))
	{
		GS_GOTO_CLEAN();
	}

clean:
	if (!!r) {
		if (SQLITE_OK != sqlite3_close(Sqlite))
			GS_ASSERT(0);
	}

	return r;
}

int main(int argc, char **argv)
{
	int r = 0;

	if (!!(r = logconvert()))
		GS_GOTO_CLEAN();

clean:
	if (!!r)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
