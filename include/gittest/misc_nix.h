#ifndef _GITTEST_MISC_NIX_H_
#define _GITTEST_MISC_NIX_H_

#include <stddef.h>

int gs_nix_write_wrapper(int fd, const char *Buf, size_t LenBuf);
int gs_nix_write_stdout_wrapper(const char *Buf, size_t LenBuf);
int gs_nix_unlink_wrapper(const char *FileNameBuf, size_t LenFileName);
int gs_nix_rename_wrapper(
	const char *SrcFileNameBuf, size_t LenSrcFileName,
	const char *DstFileNameBuf, size_t LenDstFileName);

int gs_nix_open_tmp_mask_rwx(int *oFdTmpFile);
int gs_nix_open_mask_rw(
	const char *LogFileNameBuf, size_t LenLogFileName,
	int *oFdLogFile);
int gs_nix_open_mask_rwx(
	const char *LogFileNameBuf, size_t LenLogFileName,
	int *oFdLogFile);

int gs_nix_fork_exec(
	char *ParentArgvUnifiedBuf, size_t LenParentArgvUnified,
	char **ArgvPtrs, size_t *LenArgvPtrs);

#endif /* _GITTEST_MISC_NIX_H_ */
