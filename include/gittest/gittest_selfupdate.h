#ifndef _GITTEST_GITTEST_SELFUPDATE_H_
#define _GITTEST_GITTEST_SELFUPDATE_H_

#include <cstddef>

int gs_get_current_executable_filename(char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName);

int selfupdate_main(int argc, char **argv);

#endif /* _GITTEST_GITTEST_SELFUPDATE_H_ */
