#include <cstddef>
#include <cstring>

#include <string>

#include <gittest/misc.h>

void * gs_aux_argown(void **ptr)
{
	void *ret = *ptr;
	*ptr = NULL;
	return ret;
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
