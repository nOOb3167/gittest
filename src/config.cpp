#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */

#include <cstdlib>
#include <cstdio>

#include <string>
#include <map>

#include <gittest/misc.h>

#include <gittest/config.h>

size_t aux_config_decode_hex_char_(const char *pHexChar, size_t *oIsError) {

	if (oIsError)
		*oIsError = 0;

	/* '0' to '9' guaranteed contiguous */

	if (*pHexChar >= '0' && *pHexChar <= '9')
		return *pHexChar - '0';
	
	/* the letters are contiguous in ASCII but no standard */

	switch (*pHexChar) {
	case 'a':
	case 'A':
		return 10;
	case 'b':
	case 'B':
		return 11;
	case 'c':
	case 'C':
		return 12;
	case 'd':
	case 'D':
		return 13;
	case 'e':
	case 'E':
		return 14;
	case 'f':
	case 'F':
		return 15;
	}

	if (oIsError)
		*oIsError = 1;

	return 0;
}

int aux_config_decode_hex_pairwise_swapped(const std::string &BufferSwapped, std::string *oDecoded) {
	/* originally designed to decode string, as obtained by CMAKE's FILE(READ ... HEX) command.
	*  because CMAKE is designed by web developers (ex same as have brought us Base64 encoding),
	*  it will of course encode, say, 'G' (ASCII hex 0x47) as "47" instead of "74".
	*  such that : DECODEDBYTE = (BITPATTERN(HEX[0]) << 8) + (BITPATTERN(HEX[1]) << 0)
	*  instead of: DECODEDBYTE = (BITPATTERN(HEX[0]) << 0) + (BITPATTERN(HEX[1]) << 8)
	*  praise to the web industry for bringing us quality engineering once again. */

	int r = 0;

	std::string Decoded(BufferSwapped.size() / 2, '\0');

	std::string Buffer(BufferSwapped);

	size_t IsError = 0;

	/* one full byte is a hex pair of characters - better be divisible by two */

	if (Buffer.size() % 2 != 0)
		{ r = 1; goto clean; }

	/* swap characters in individual hex pairs */

	for (size_t i = 0; i < Buffer.size(); i += 2)
		std::swap(Buffer[i + 1], Buffer[i]);

	/* decode */

	for (size_t i = 0; i < Buffer.size(); i += 2)
		Decoded[i / 2] =
			(aux_config_decode_hex_char_(&Buffer[i],     &IsError) & 0xF) << 0 |
			(aux_config_decode_hex_char_(&Buffer[i + 1], &IsError) & 0xF) << 4;

	if (IsError)
		{ r = 1; goto clean; }

	if (oDecoded)
		oDecoded->swap(Decoded);

clean:

	return r;
}

int aux_config_parse_find_next_newline(const char *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew)
{
	/* effectively can not fail. end of the buffer is an implicit newline */
	const char newlineR = '\r';
	const char newlineN = '\n';
	const char *firstR = (const char *)memchr(DataStart + Offset, newlineR, DataLength - Offset);
	const char *firstN = (const char *)memchr(DataStart + Offset, newlineN, DataLength - Offset);
	const char *firstNewlineChar = (firstR && firstN) ? GS_MIN(firstR, firstN) : GS_MAX(firstR, firstN);
	if (! firstNewlineChar)
		*OffsetNew = DataLength;
	else
		*OffsetNew = (uint32_t)(firstNewlineChar - DataStart);
	return 0;
}

int aux_config_parse_skip_newline(const char *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew)
{
	/* do nothing if not at a newline char.
	*  end of buffer counts as being not at a newline char. */
	const char newlineR = '\r';
	const char newlineN = '\n';
	while (Offset < DataLength && (DataStart[Offset] == newlineR || DataStart[Offset] == newlineN))
		Offset += 1;
	*OffsetNew = Offset;
	return 0;
}

int aux_config_parse(
	const char *BufferBuf, size_t LenBuffer,
	std::map<std::string, std::string> *oKeyVal)
{
	int r = 0;

	std::map<std::string, std::string> KeyVal;

	uint32_t Offset = 0;
	uint32_t OldOffset = 0;
	const char *DataStart = BufferBuf;
	uint32_t DataLength = LenBuffer;

	const char equals = '=';
	const char hdr_nulterm_expected[] = "GITTEST_CONF";
	const size_t hdr_raw_size = sizeof(hdr_nulterm_expected) - 1;

	OldOffset = Offset;
	if (!!(r = aux_config_parse_find_next_newline(DataStart, DataLength, Offset, &Offset)))
		goto clean;
	/* hdr_raw_size of ASCII letters and 1 of NEWLINE */
	if (hdr_raw_size < Offset - OldOffset)
		{ r = 1; goto clean; }
	if (memcmp(hdr_nulterm_expected, DataStart + OldOffset, hdr_raw_size) != 0)
		{ r = 1; goto clean; }
	if (!!(r = aux_config_parse_skip_newline(DataStart, DataLength, Offset, &Offset)))
		goto clean;

	while (Offset < DataLength) {

		/* find where the current line ends */

		OldOffset = Offset;
		if (!!(r = aux_config_parse_find_next_newline(DataStart, DataLength, Offset, &Offset)))
			goto clean;

		/* extract current line - line should be of format 'KKK=VVV' */

		std::string line(DataStart + OldOffset, DataStart + Offset);

		/* split extracted line into KKK and VVV parts by equal sign */

		size_t equalspos = line.npos;
		if ((equalspos = line.find_first_of(equals, 0)) == line.npos)
			{ r = 1; goto clean; }
		std::string key(line.data() + 0, line.data() + equalspos);
		std::string val(line.data() + equalspos + 1, line.data() + line.size());

		/* record the gotten key value pair */

		KeyVal[key] = val;

		/* skip to the next line (or end of buffer) */

		if (!!(r = aux_config_parse_skip_newline(DataStart, DataLength, Offset, &Offset)))
			goto clean;
	}

	if (oKeyVal)
		oKeyVal->swap(KeyVal);

clean:

	return r;
}

int aux_config_read_fullpath(
	const char *PathFullBuf, size_t LenPathFull,
	std::map<std::string, std::string> *oKeyVal)
{
	int r = 0;

	std::map<std::string, std::string> KeyVal;

	const char newline = '\n';
	const char equals  = '=';
	const char hdr_nulterm_expected[] = "GITTEST_CONF";
	const size_t hdr_raw_size = sizeof(hdr_nulterm_expected) - 1;

	const size_t ArbitraryBufferSize = 4096;
	char buf[ArbitraryBufferSize];

	std::string retbuffer;
	
	FILE *f = NULL;
    
    size_t ret = 0;
    size_t idx = 0;

	if (!!(r = gs_buf_ensure_haszero(PathFullBuf, LenPathFull + 1)))
		{ r = 1; goto clean; }

	if (!(f = fopen(PathFullBuf, "rb")))
		{ r = 1; goto clean; }

	while ((ret = fread(buf, 1, ArbitraryBufferSize, f)) > 0)
		retbuffer.append(buf, ret);

	if (ferror(f) || !feof(f))
		{ r = 1; goto clean; }

	if (!!(r = aux_config_parse(retbuffer.data(), retbuffer.size(), &KeyVal)))
		goto clean;

	if (oKeyVal)
		oKeyVal->swap(KeyVal);

clean:
	if (f)
		fclose(f);

	return r;
}

/* returned value scoped not even to map lifetime - becomes invalid on map modification so do not do that */
const char * aux_config_key(const confmap_t &KeyVal, const char *Key) {
	const confmap_t::const_iterator &it = KeyVal.find(Key);
	if (it == KeyVal.end())
		return NULL;
	return it->second.c_str();
}

/* returned value copied */
int aux_config_key_ex(const confmap_t &KeyVal, const char *Key, std::string *oVal) {
	const confmap_t::const_iterator &it = KeyVal.find(Key);
	if (it == KeyVal.end())
		return 1;
	{
		std::string Val(it->second);
		if (oVal)
			oVal->swap(Val);
	}
	return 0;
}

int aux_config_key_uint32(const confmap_t &KeyVal, const char *Key, uint32_t *oVal) {
	GS_ASSERT(sizeof(uint32_t) <= sizeof(long long));
	const confmap_t::const_iterator &it = KeyVal.find(Key);
	if (it == KeyVal.end())
		return 1;
	{
		const char *startPtr = it->second.c_str();
		char *endPtr = 0;
		errno = 0;
		unsigned long long valLL = strtoull(startPtr, &endPtr, 10);
		if (errno == ERANGE && (valLL == ULLONG_MAX))
			return 2;
		if (errno == EINVAL)
			return 2;
		if (endPtr != startPtr + it->second.size())
			return 2;
		if (oVal)
			*oVal = (uint32_t)valLL;
	}
	return 0;
}

int aux_config_read_default_everything(std::map<std::string, std::string> *oKeyVal) {
	int r = 0;

	const char LocBuf[] = GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_PATHNAME;
	size_t LenLoc = (sizeof(GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_PATHNAME)) - 1;
	const char NameBuf[] = GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_FILENAME;
	size_t LenName = (sizeof(GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_FILENAME)) - 1;

	if (!!(r = aux_config_read_builtin_or_relative_current_executable(
		LocBuf, LenLoc,
		NameBuf, LenName,
		oKeyVal)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_config_read_builtin(std::map<std::string, std::string> *oKeyVal) {
	int r = 0;

	std::string BufferBuiltinConfig(GS_CONFIG_DEFS_GLOBAL_CONFIG_BUILTIN_HEXSTRING);
	std::string DecodedConfig;

	if (!!(r = aux_config_decode_hex_pairwise_swapped(BufferBuiltinConfig, &DecodedConfig)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_parse(
		DecodedConfig.data(), DecodedConfig.size(),
		oKeyVal)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_config_read_builtin_or_relative_current_executable(
	const char *ExpectedLocationBuf, size_t LenExpectedLocation,
	const char *ExpectedNameBuf, size_t LenExpectedName,
	std::map<std::string, std::string> *oKeyVal)
{
	int r = 0;

	size_t LenPath = 0;
	char PathBuf[512];

	size_t LenPathFull = 0;
	char PathFullBuf[512];

	size_t PathIsExist = 0;

	if (!!(r = gs_build_path_interpret_relative_current_executable(
		ExpectedLocationBuf, LenExpectedLocation,
		PathBuf, sizeof PathBuf, &LenPath)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_path_append_abs_rel(
		PathBuf, LenPath,
		ExpectedNameBuf, LenExpectedName,
		PathFullBuf, sizeof PathFullBuf, &LenPathFull)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_file_exist(PathFullBuf, LenPathFull, &PathIsExist)))
		GS_GOTO_CLEAN();

	if (PathIsExist) {
		/* read from the file system */

		if (!!(r = aux_config_read_fullpath(
			PathFullBuf, LenPathFull,
			oKeyVal)))
		{
			GS_GOTO_CLEAN();
		}
	}
	else {
		/* use the builtin config (preprocessor definition) */

		if (!!(r = aux_config_read_builtin(oKeyVal)))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int aux_config_key_ex_interpret_relative_current_executable(
	const confmap_t &KeyVal, const char *Key, std::string *oVal)
{

	const confmap_t::const_iterator &it = KeyVal.find(Key);

	size_t LenPath = 0;
	char PathBuf[512];

	if (it == KeyVal.end())
		return 1;

	{
		std::string RawVal = it->second;

		if (!!(gs_build_path_interpret_relative_current_executable(
			RawVal.c_str(), RawVal.size(), PathBuf, sizeof PathBuf, &LenPath)))
		{
			return 1;
		}
	}

	if (oVal)
		*oVal = std::string(PathBuf, LenPath);

	return 0;
}

int aux_config_get_common_vars(
	const confmap_t &KeyVal,
	GsAuxConfigCommonVars *oCommonVars)
{
	int r = 0;

	GsAuxConfigCommonVars CommonVars = {};

	GS_AUX_CONFIG_COMMON_VAR_UINT32_NONUCF(KeyVal, CommonVars, ServPort);
	GS_AUX_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, ServHostName);
	GS_AUX_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, RefNameMain);
	GS_AUX_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, RefNameSelfUpdate);
	GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoMainPath);
	GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoSelfUpdatePath);
	GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoMasterUpdatePath);
	GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoMasterUpdateCheckoutPath);
	GS_AUX_CONFIG_COMMON_VAR_UINT32_NONUCF(KeyVal, CommonVars, ServBlobSoftSizeLimit);

	if (oCommonVars)
		*oCommonVars = CommonVars;

clean:

	return r;
}
