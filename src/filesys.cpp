#include <cstddef>
#include <cstring>

#include <sstream>

#include <gittest/misc.h>

#include <gittest/filesys.h>

int gs_build_modified_filename(
	const char *BaseFileNameBuf, size_t LenBaseFileName,
	const char *ExpectedSuffixBuf, size_t LenExpectedSuffix,
	const char *ExpectedExtensionBuf, size_t LenExpectedExtension,
	const char *AddedSuffixBuf, size_t LenAddedSuffix,
	const char *ReplacedExtensionBuf, size_t LenReplacedExtension,
	char *ioModifiedFileNameBuf, size_t ModifiedFileNameSize, size_t *oLenModifiedFileName)
{
	int r = 0;

	std::string BaseFileName(BaseFileNameBuf, LenBaseFileName);
	std::string ExpectedSuffix(ExpectedSuffixBuf, LenExpectedSuffix);
	std::string ExpectedExtension(ExpectedExtensionBuf, LenExpectedExtension);
	std::string AddedSuffix(AddedSuffixBuf, LenAddedSuffix);
	std::string ReplacedExtension(ReplacedExtensionBuf, LenReplacedExtension);

	std::stringstream ss;
	std::string out;

	size_t ExtensionCutoffOffset = GS_MIN(LenBaseFileName - LenExpectedExtension, LenBaseFileName);
	size_t SuffixCheckOffset = GS_MIN(LenBaseFileName - LenExpectedExtension - LenExpectedSuffix, LenBaseFileName);

	// NOTE: GS_MIN effectively guarding against underflow
	if (BaseFileName.substr(ExtensionCutoffOffset) != ExpectedExtension ||
		BaseFileName.substr(SuffixCheckOffset, ExtensionCutoffOffset - SuffixCheckOffset) != ExpectedSuffix)
	{
		GS_ERR_CLEAN(1);
	}

	ss << BaseFileName.substr(0, ExtensionCutoffOffset) << AddedSuffix << ReplacedExtension;
	out = ss.str();

	if (!!(r = gs_buf_copy_zero_terminate(
		out.c_str(), out.size(),
		ioModifiedFileNameBuf, ModifiedFileNameSize, oLenModifiedFileName)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_build_path_interpret_relative_current_executable(
	const char *PossiblyRelativePathBuf, size_t LenPossiblyRelativePath,
	char *ioPathBuf, size_t PathBufSize, size_t *oLenPathBuf)
{
	int r = 0;

	size_t PossiblyRelativePathIsAbsolute = 0;

	if (!!(r = gs_path_is_absolute(
		PossiblyRelativePathBuf, LenPossiblyRelativePath,
		&PossiblyRelativePathIsAbsolute)))
	{
		GS_GOTO_CLEAN();
	}

	if (PossiblyRelativePathIsAbsolute) {

		if (!!(r = gs_buf_copy_zero_terminate(
			PossiblyRelativePathBuf, LenPossiblyRelativePath,
			ioPathBuf, PathBufSize, oLenPathBuf)))
		{
			GS_GOTO_CLEAN();
		}

	} else {

		if (!!(r = gs_build_current_executable_relative_filename(
			PossiblyRelativePathBuf, LenPossiblyRelativePath,
			ioPathBuf, PathBufSize, oLenPathBuf)))
		{
			GS_GOTO_CLEAN();
		}

	}

clean:

	return r;
}

int gs_path_kludge_filenameize(char *ioPathBuf, size_t *oLenPath)
{
	char *sep = strrchr(ioPathBuf, '/');
	sep = sep ? sep : strrchr(ioPathBuf, '\\');
	if (sep) {
		sep++; /* skip separator */
		size_t len = ioPathBuf + strlen(ioPathBuf) - sep;
		memmove(ioPathBuf, sep, len);
		memset(ioPathBuf + len, '\0', 1);
	}
	*oLenPath = strlen(ioPathBuf);
	return 0;
}
