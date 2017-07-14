#ifndef _GITTEST_FILESYS_H_
#define _GITTEST_FILESYS_H_

#include <stddef.h>

int gs_build_modified_filename(
	const char *BaseFileNameBuf, size_t LenBaseFileName,
	const char *ExpectedSuffixBuf, size_t LenExpectedSuffix,
	const char *ExpectedExtensionBuf, size_t LenExpectedExtension,
	const char *ExtraSuffixBuf, size_t LenExtraSuffix,
	const char *ExtraExtensionBuf, size_t LenExtraExtension,
	char *ioModifiedFileNameBuf, size_t ModifiedFileNameSize, size_t *oLenModifiedFileName);
int gs_build_path_interpret_relative_current_executable(
	const char *PossiblyRelativePathBuf, size_t LenPossiblyRelativePath,
	char *ioPathBuf, size_t PathBufSize, size_t *oLenPathBuf);

int gs_path_kludge_filenameize(char *ioPathBuf, size_t *oLenPath);

#ifdef _MSC_VER
#  include <gittest/filesys_win.h>
#else
#  include <gittest/filesys_nix.h>
#endif /* _MSC_VER */

#endif /* _GITTEST_FILESYS_H_ */
