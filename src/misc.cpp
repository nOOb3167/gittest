#include <cstddef>
#include <cstring>

#include <string>

#include <gittest/misc.h>

void gs_aux_delete_nulling(void **ptr)
{
	if (*ptr) {
		delete *ptr;
		*ptr = NULL;
	}
}

void * gs_aux_argown(void **ptr)
{
	void *ret = *ptr;
	*ptr = NULL;
	return ret;
}

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

int gs_buf_copy_zero_terminate(
	const char *SrcBuf, size_t LenSrc,
	char *ioDstBuf, size_t DstBufSize, size_t *oLenDst)
{
	int r = 0;

	if (!!(r = gs_buf_strnlen(SrcBuf, LenSrc + 1, NULL)))
		GS_GOTO_CLEAN();

	if (LenSrc >= DstBufSize)
		GS_ERR_CLEAN(1);

	memcpy(ioDstBuf, SrcBuf, LenSrc);
	memset(ioDstBuf + LenSrc, '\0', 1);

	if (oLenDst)
		*oLenDst = LenSrc;

clean:

	return r;
}

int gs_buf_strnlen(const char *Buf, size_t BufSize, size_t *oLenBufOpt) {
	size_t LenBuf = strnlen(Buf, BufSize);
	if (oLenBufOpt)
		*oLenBufOpt = LenBuf;
	return LenBuf == BufSize;
}

int gs_buf_ensure_haszero(const char *Buf, size_t BufSize) {
	return !memchr(Buf, '\0', BufSize);
}

int aux_char_from_string_alloc(const std::string &String, char **oStrBuf, size_t *oLenStr) {
	int r = 0;

	size_t LenStr = 0;
	char *StrBuf = NULL;
	size_t StrBufSize = 0;

	if (String.size() == 0)
		GS_ERR_CLEAN(1);

	/* chars plus null terminator */
	LenStr = String.size();
	StrBufSize = LenStr + 1;
	StrBuf = new char[StrBufSize];
	memcpy(StrBuf, String.c_str(), StrBufSize);

	if (oStrBuf)
		*oStrBuf = StrBuf;

	if (oLenStr)
		*oLenStr = LenStr;

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

void gs_current_thread_name_set_cstr(
	const char *NameCStr)
{
	size_t arbitrary_length_limit = 2048;

	if (gs_buf_ensure_haszero(NameCStr, arbitrary_length_limit))
		GS_ASSERT(0);

	gs_current_thread_name_set(NameCStr, strlen(NameCStr));
}

void gs_current_thread_name_set_cstr_2(
	const char *BaseNameCStr,
	const char *optExtraNameCStr)
{
	std::string ThreadName(BaseNameCStr);

	if (optExtraNameCStr)
		ThreadName.append(optExtraNameCStr);

	gs_current_thread_name_set_cstr(ThreadName.c_str());
}
