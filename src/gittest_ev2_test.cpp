#include <cassert>
#include <cstdlib>

#define EVENT2_VISIBILITY_STATIC_MSVC
#include <event2/event.h>

int main(int argc, char **argv)
{
	struct event_base *Base = NULL;

	if (!(Base = event_base_new()))
		assert(0);

	return EXIT_SUCCESS;
}
