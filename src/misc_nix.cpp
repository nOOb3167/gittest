#include <gittest/misc.h>

#include <sys/prctl.h>

void gs_current_thread_name_set(
	const char *NameBuf,
	size_t LenName)
{
	int r = 0;

	if (!!(r = prctl(PR_SET_NAME, NameBuf, 0, 0, 0)))
		GS_GOTO_CLEAN();

clean:

	return r;
}
