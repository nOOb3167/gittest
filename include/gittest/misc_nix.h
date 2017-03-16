#ifndef _GITTEST_MISC_NIX_H_
#define _GITTEST_MISC_NIX_H_

#include <stddef.h>

int gs_nix_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute);
int gs_nix_path_ensure_absolute(const char *PathBuf, size_t LenPath);
int gs_nix_path_eat_trailing_slashes(
	const char *InputPathBuf, size_t LenInputPath,
	size_t *oNewLen);
int gs_nix_path_eat_trailing_nonslashes(
	const char *InputPathBuf, size_t LenInputPath,
	size_t *oNewLen);
int gs_nix_path_ensure_starts_with_lump(
	const char *InputPathBuf, size_t LenInputPath);
int gs_nix_path_add_trailing_slash_cond_inplace(
	char *DataStart, size_t DataLength, size_t OffsetOnePastEnd, size_t *OffsetOnePastEndNew);
int gs_nix_path_append_midslashing_inplace(
	const char *ToAddBuf, size_t LenToAdd,
	char *DataStart, size_t DataLength, size_t OffsetOnePastEnd, size_t *OffsetOnePastEndNew);

int gs_nix_absolute_path_directory(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath);

int gs_nix_access_wrapper(
	const char *InputPathBuf, size_t LenInpuPath,
	int mode);
int gs_nix_readlink_wrapper(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName);

int gs_nix_close_wrapper(int fd);
int gs_nix_close_wrapper_noerr(int fd);
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
