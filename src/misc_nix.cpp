#include <signal.h>
#include <sys/prctl.h>

#include <gittest/misc.h>

#include <gittest/misc_nix.h>

void gs_current_thread_name_set(
	const char *NameBuf,
	size_t LenName)
{
	/* http://stackoverflow.com/questions/778085/how-to-name-a-thread-in-linux/778124#778124 */
	int r = 0;

	/* the limit is magic, see prctl(2) for PR_SET_NAME */
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
