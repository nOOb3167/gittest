#include <cstdlib>
#include <cassert>

#include <enet/enet.h>

int stuff() {
	int r = 0;

clean:

	return r;
}

int main(int argc, char **argv) {

	enet_initialize();

	int r = stuff();
	assert(!r);

	return EXIT_SUCCESS;
}
