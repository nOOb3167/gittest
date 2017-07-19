#include <cstddef>
#include <cstring>

#include <sstream>

#include <gittest/misc.h>

#include <gittest/filesys.h>

int gs_build_modified_filename(
	const char *BaseFileNameBuf, size_t LenBaseFileName,
	const char *ExpectedSuffixBuf, size_t LenExpectedSuffix,
	const char *ExpectedExtensionBuf, size_t LenExpectedExtension,
	const char *ExtraSuffixBuf, size_t LenExtraSuffix,
	const char *ExtraExtensionBuf, size_t LenExtraExtension,
	char *ioModifiedFileNameBuf, size_t ModifiedFileNameSize, size_t *oLenModifiedFileName)
{
	int r = 0;

	std::string BaseFileName(BaseFileNameBuf, LenBaseFileName);
	std::string ExpectedSuffix(ExpectedSuffixBuf, LenExpectedSuffix);
	std::string ExpectedExtension(ExpectedExtensionBuf, LenExpectedExtension);
	std::string ExtraSuffix(ExtraSuffixBuf, LenExtraSuffix);
	std::string ExtraExtension(ExtraExtensionBuf, LenExtraExtension);

	std::stringstream ss;
	std::string out;

	// NOTE: GS_MAX guarding against underflow
	if (BaseFileName.substr(GS_MIN(LenBaseFileName - LenExpectedSuffix   , LenBaseFileName)) != ExpectedSuffix ||
		BaseFileName.substr(GS_MIN(LenBaseFileName - LenExpectedExtension, LenBaseFileName)) != ExpectedExtension)
	{
		GS_ERR_CLEAN(1);
	}

	ss << BaseFileNameBuf << ExtraSuffix << ExtraExtension;
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
