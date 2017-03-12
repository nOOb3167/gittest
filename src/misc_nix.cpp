#include <gittest/misc.h>

#include <signal.h>
#include <sys/prctl.h>

void gs_current_thread_name_set(
	const char *NameBuf,
	size_t LenName)
{
	/* http://stackoverflow.com/questions/778085/how-to-name-a-thread-in-linux/778124#778124 */
	int r = 0;

	if (LenName >= 16)
		GS_ERR_CLEAN(1);

	if (!!(r = prctl(PR_SET_NAME, NameBuf, 0, 0, 0)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

void gs_debug_break() {
	/* NOTE: theoretically can fail with nonzero status */
	raise(SIGTRAP);
}
