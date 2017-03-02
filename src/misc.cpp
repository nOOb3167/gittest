#include <cstddef>
#include <cstring>

#include <gittest/misc.h>

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

	assert(ioModifiedFileNameBuf[LenModifiedFileName] == '\0');

	if (oLenModifiedFileName)
		*oLenModifiedFileName = LenModifiedFileName;

clean:

	return r;
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
