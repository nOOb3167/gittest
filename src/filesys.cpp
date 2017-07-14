#include <cstddef>
#include <cstring>

#include <gittest/misc.h>

#include <gittest/filesys.h>

int gs_build_modified_filename(
	const char *BaseFileNameBuf, size_t LenBaseFileName,
	const char *ExpectedSuffix, size_t LenExpectedSuffix,
	const char *ExpectedExtension, size_t LenExpectedExtension,
	const char *ExtraSuffix, size_t LenExtraSuffix,
	const char *ExtraExtension, size_t LenExtraExtension,
	char *ioModifiedFileNameBuf, size_t ModifiedFileNameSize, size_t *oLenModifiedFileName)
{
	int r = 0;

	const size_t OffsetStartOfCheck = LenBaseFileName - LenExpectedSuffix;
	const size_t OffsetStartOfChange = LenBaseFileName - LenExpectedExtension;
	const size_t LenModifiedFileName = OffsetStartOfChange + LenExtraSuffix + LenExtraExtension;
    
	if (LenBaseFileName < LenExpectedSuffix)
		GS_ERR_CLEAN(1);
	if (LenExpectedSuffix < LenExpectedExtension)
		GS_ERR_CLEAN(1);

	if (strcmp(ExpectedSuffix, BaseFileNameBuf + OffsetStartOfCheck) != 0)
		GS_ERR_CLEAN(1);
	if (strcmp(ExpectedExtension, BaseFileNameBuf + OffsetStartOfChange) != 0)
		GS_ERR_CLEAN(1);

	if (ModifiedFileNameSize < OffsetStartOfChange + LenExtraSuffix + LenExtraExtension + 1 /*zero terminator*/)
		GS_ERR_CLEAN(1);

	memcpy(ioModifiedFileNameBuf, BaseFileNameBuf, OffsetStartOfChange);
	memcpy(ioModifiedFileNameBuf + OffsetStartOfChange, ExtraSuffix, LenExtraSuffix);
	memcpy(ioModifiedFileNameBuf + OffsetStartOfChange + LenExtraSuffix, ExtraExtension, LenExtraExtension);
	memset(ioModifiedFileNameBuf + OffsetStartOfChange + LenExtraSuffix + LenExtraExtension, '\0', 1);

	GS_ASSERT(ioModifiedFileNameBuf[LenModifiedFileName] == '\0');

	if (oLenModifiedFileName)
		*oLenModifiedFileName = LenModifiedFileName;

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
