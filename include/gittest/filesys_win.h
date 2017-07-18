#ifndef _GITTEST_FILESYS_WIN_H_
#define _GITTEST_FILESYS_WIN_H_

#include <stddef.h>

int gs_build_path_expand_separated(
	const char *PathBuf, size_t LenPath,
	const char *ExtBuf, size_t LenExt,
	const char *SeparatorBuf, size_t LenSeparator,
	char *ExpandedBuf, size_t ExpandedSize, size_t *oLenExpanded);
int gs_build_current_executable_relative_filename(
	const char *RelativeBuf, size_t LenRelative,
	char *ioCombinedBuf, size_t CombinedBufSize, size_t *oLenCombined);

int gs_get_current_executable_filename(char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName);
int gs_get_current_executable_directory(
	char *ioCurrentExecutableDirBuf, size_t CurrentExecutableDirSize, size_t *oLenCurrentExecutableDir);

int gs_file_exist(
	const char *FileNameBuf, size_t LenFileName,
	size_t *oIsExist);
int gs_file_exist_ensure(const char *FileNameBuf, size_t LenFileName);

int gs_file_is_directory(const char *FileNameBuf, size_t LenFileName,
	size_t *oIsDirectory);

int gs_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute);

int gs_path_append_abs_rel(
	const char *AbsoluteBuf, size_t LenAbsolute,
	const char *RelativeBuf, size_t LenRelative,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath);

#endif /* _GITTEST_FILESYS_WIN_H_ */
