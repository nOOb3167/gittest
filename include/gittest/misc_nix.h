#ifndef _GITTEST_MISC_NIX_H_
#define _GITTEST_MISC_NIX_H_

#include <stddef.h>

int gs_nix_path_ensure_absolute(const char *PathBuf, size_t LenPath);

int gs_nix_readlink_wrapper(
	const char *InputPathBuf, size_t LenInputPathBuf,
	char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName);
int gs_nix_open_wrapper(
	const char *LogFileNameBuf, size_t LenLogFileName,
	int *oFdLogFile);
int gs_nix_close_wrapper(int fd);
int gs_nix_write_wrapper(int fd, const char *Buf, size_t LenBuf);
int gs_nix_write_stdout_wrapper(const char *Buf, size_t LenBuf);

#endif /* _GITTEST_MISC_NIX_H_ */
