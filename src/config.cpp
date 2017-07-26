#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include <string>
#include <map>

#include <gittest/misc.h>
#include <gittest/config.h>
#include <gittest/filesys.h>

#include <GsConfigHeader.h>  /* including the generated config header */

#define GS_CONFIG_HEADER_STRING "GITTEST_CONF"

#define GS_CONFIG_SUBST_PATTERN_ALPHA  "@"
#define GS_CONFIG_SUBST_PATTERN_SEP    "@SEP@"
#define GS_CONFIG_SUBST_PATTERN_EXEDIR "@EXEDIR@"

#define GS_CONFIG_COMMON_VAR_UINT32_NONUCF(KEYVAL, COMVARS, NAME)                  \
	{                                                                              \
		uint32_t Conf ## NAME = 0;                                                 \
		if (!!(r = gs_config_key_uint32((KEYVAL), "Conf" # NAME, & Conf ## NAME))) \
			goto clean;                                                            \
		(COMVARS).NAME = Conf ## NAME;                                             \
	}

#define GS_CONFIG_COMMON_VAR_STRING_NONUCF(KEYVAL, COMVARS, NAME)                                             \
	{                                                                                                         \
		std::string Conf ## NAME;                                                                             \
		if (!!(r = gs_config_key_ex((KEYVAL), "Conf" # NAME, & Conf ## NAME)))                                \
			goto clean;                                                                                       \
		if (!!(r = aux_char_from_string_alloc(Conf ## NAME, &(COMVARS).NAME ## Buf, &(COMVARS).Len ## NAME))) \
			goto clean;                                                                                       \
	}

#define GS_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KEYVAL, COMVARS, NAME)              \
	{                                                                                                                \
		std::string Conf ## NAME;                                                                                    \
		if (!!(r = gs_config_key_ex_interpret_relative_current_executable((KEYVAL), "Conf" # NAME, & Conf ## NAME))) \
			goto clean;                                                                                              \
		if (!!(r = aux_char_from_string_alloc(Conf ## NAME, &(COMVARS).NAME ## Buf, &(COMVARS).Len ## NAME)))        \
			goto clean;                                                                                              \
	}

#define GS_CONFIG_COMMON_VAR_STRING_INTERPRET_JAVA_CLASS_PATH_SPECIAL_NONUCF(KEYVAL, COMVARS, COMVARS_SEP_NAME, NAME) \
	{                                                                                                                 \
		std::string Conf ## NAME;                                                                                     \
		if (!!(r = gs_config_key_ex_interpret_java_class_path_special((KEYVAL),                                       \
			(COMVARS).COMVARS_SEP_NAME ## Buf, (COMVARS).Len ## COMVARS_SEP_NAME,                                     \
			"Conf" # NAME, & Conf ## NAME)))                                                                          \
		{                                                                                                             \
			goto clean;                                                                                               \
		}                                                                                                             \
		if (!!(r = aux_char_from_string_alloc(Conf ## NAME, &(COMVARS).NAME ## Buf, &(COMVARS).Len ## NAME)))         \
			goto clean;                                                                                               \
	}

#define GS_CONFIG_COMMON_VAR_STRING_INTERPRET_SUBST_NONUCF(KEYVAL, COMVARS, NAME)                             \
	{                                                                                                         \
		std::string Conf ## NAME;                                                                             \
		if (!!(r = gs_config_key_ex_interpret_subst((KEYVAL), "Conf" # NAME, & Conf ## NAME)))                \
			goto clean;                                                                                       \
		if (!!(r = aux_char_from_string_alloc(Conf ## NAME, &(COMVARS).NAME ## Buf, &(COMVARS).Len ## NAME))) \
			goto clean;                                                                                       \
	}

typedef ::std::map<::std::string, ::std::string> confmap_t;

/** @sa
       ::gs_conf_map_create
       ::gs_conf_map_destroy
*/
struct GsConfMap
{
	confmap_t mMap;
};

int gs_config_pattern_subst(const std::string &RawVal, std::string *oResultVal);

size_t gs_config_decode_hex_char_(const char *pHexChar, size_t *oIsError);
int gs_config_decode_hex(const std::string &BufferSwapped, std::string *oDecoded);
int gs_config_decode_hex_pairwise_swapped(const std::string &BufferSwapped, std::string *oDecoded);

int gs_config_key_ex(const GsConfMap *KeyVal, const char *Key, std::string *oVal);
int gs_config_key_ex_interpret_relative_current_executable(
	const GsConfMap *KeyVal, const char *Key, std::string *oVal);
int gs_config_key_ex_interpret_java_class_path_special(
	const GsConfMap *KeyVal,
	const char *SeparatorBuf, size_t LenSeparator,
	const char *Key, std::string *oVal);
int gs_config_key_ex_interpret_subst(
	const GsConfMap *KeyVal, const char *Key, std::string *oVal);

int gs_config_pattern_subst(const std::string &RawVal, std::string *oResultVal)
{
	int r = 0;

	std::string ResultVal;

	size_t Offset = 0;
	size_t OffsetAlpha = 0;
	size_t OffsetAlphaClosing = 0;
	std::string Pattern;

	while (Offset < RawVal.size()) {
		Pattern = "";
		/* OffsetAlpha - offset of alpha OR std::string::npos (means 'end' in substr) */
		OffsetAlpha = RawVal.find_first_of(GS_CONFIG_SUBST_PATTERN_ALPHA, Offset);
		OffsetAlphaClosing = RawVal.find_first_of(GS_CONFIG_SUBST_PATTERN_ALPHA, OffsetAlpha == std::string::npos ? OffsetAlpha : OffsetAlpha + 1);
		/* unclosed pattern */
		if (OffsetAlpha != std::string::npos && OffsetAlphaClosing == std::string::npos)
			return 1;
		if (OffsetAlpha != std::string::npos && OffsetAlphaClosing != std::string::npos)
			Pattern = RawVal.substr(OffsetAlpha, (OffsetAlphaClosing + 1) - OffsetAlpha);
		ResultVal.append(RawVal.substr(Offset, OffsetAlpha == std::string::npos ? OffsetAlpha : OffsetAlpha - Offset));
		if (Pattern == GS_CONFIG_SUBST_PATTERN_SEP) {
			ResultVal.append("\0", 1);
		} else if (Pattern == GS_CONFIG_SUBST_PATTERN_EXEDIR) {
			char ExeBuf[512] = {};
			size_t LenExe = 0;
			if (!! gs_get_current_executable_directory(ExeBuf, sizeof ExeBuf, &LenExe))
				return 1;
			ResultVal.append(std::string(ExeBuf, LenExe));
		}
		Offset = OffsetAlphaClosing == std::string::npos ? OffsetAlphaClosing : OffsetAlphaClosing + 1;
	}

	if (oResultVal)
		oResultVal->swap(ResultVal);

	return r;
}

size_t gs_config_decode_hex_char_(const char *pHexChar, size_t *oIsError)
{

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

int gs_config_decode_hex(const std::string &BufferSwapped, std::string *oDecoded)
{
	int r = 0;

	std::string Decoded(BufferSwapped.size() / 2, '\0');

	std::string Buffer(BufferSwapped);

	size_t IsError = 0;

	/* one full byte is a hex pair of characters - better be divisible by two */

	if (Buffer.size() % 2 != 0)
	{ r = 1; goto clean; }

	/* decode */

	for (size_t i = 0; i < Buffer.size(); i += 2)
		Decoded[i / 2] =
		(gs_config_decode_hex_char_(&Buffer[i], &IsError) & 0xF) << 0 |
		(gs_config_decode_hex_char_(&Buffer[i + 1], &IsError) & 0xF) << 4;

	if (IsError)
	{ r = 1; goto clean; }

	if (oDecoded)
		oDecoded->swap(Decoded);

clean:

	return r;
}

int gs_config_decode_hex_pairwise_swapped(const std::string &BufferSwapped, std::string *oDecoded)
{
	/* originally designed to decode string, as obtained by CMAKE's FILE(READ ... HEX) command.
	*  because CMAKE is designed by web developers (ex same as have brought us Base64 encoding),
	*  it will of course encode, say, 'G' (ASCII hex 0x47) as "47" instead of "74".
	*  such that : DECODEDBYTE = (BITPATTERN(HEX[0]) << 4) + (BITPATTERN(HEX[1]) << 0)
	*  instead of: DECODEDBYTE = (BITPATTERN(HEX[0]) << 0) + (BITPATTERN(HEX[1]) << 4)
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
		(gs_config_decode_hex_char_(&Buffer[i], &IsError) & 0xF) << 0 |
		(gs_config_decode_hex_char_(&Buffer[i + 1], &IsError) & 0xF) << 4;

	if (IsError)
	{ r = 1; goto clean; }

	if (oDecoded)
		oDecoded->swap(Decoded);

clean:

	return r;
}

/* returned value copied */
int gs_config_key_ex(const GsConfMap *KeyVal, const char *Key, std::string *oVal)
{
	const confmap_t::const_iterator &it = KeyVal->mMap.find(Key);
	if (it == KeyVal->mMap.end())
		return 1;
	{
		std::string Val(it->second);
		if (oVal)
			oVal->swap(Val);
	}
	return 0;
}

int gs_config_key_ex_interpret_relative_current_executable(
	const GsConfMap *KeyVal, const char *Key, std::string *oVal)
{

	const confmap_t::const_iterator &it = KeyVal->mMap.find(Key);

	size_t LenPath = 0;
	char PathBuf[512] = {};

	if (it == KeyVal->mMap.end())
		return 1;

	{
		std::string RawVal = it->second;

		/* NOTE: empty RawVal thus leaves PathBuf empty */
		if (! RawVal.empty()) {
			if (!!(gs_build_path_interpret_relative_current_executable(
				RawVal.c_str(), RawVal.size(), PathBuf, sizeof PathBuf, &LenPath)))
			{
				return 1;
			}
		}
	}

	if (oVal)
		*oVal = std::string(PathBuf, LenPath);

	return 0;
}

int gs_config_key_ex_interpret_java_class_path_special(
	const GsConfMap *KeyVal,
	const char *SeparatorBuf, size_t LenSeparator,
	const char *Key, std::string *oVal)
{

	const confmap_t::const_iterator &it = KeyVal->mMap.find(Key);

	char PathBuf[512];
	size_t LenPath = 0;

	char ExtBuf[] = "*.jar";
	size_t LenExt = (sizeof ExtBuf) - 1;

	char ExpandedBuf[32768];
	size_t LenExpanded = 0;

	if (it == KeyVal->mMap.end())
		return 1;

	{
		std::string RawVal = it->second;

		if (!!(gs_build_path_interpret_relative_current_executable(
			RawVal.c_str(), RawVal.size(), PathBuf, sizeof PathBuf, &LenPath)))
		{
			return 1;
		}

		if (!!(gs_build_path_expand_separated(
			PathBuf, LenPath,
			ExtBuf, LenExt,
			SeparatorBuf, LenSeparator,
			ExpandedBuf, sizeof ExpandedBuf, &LenExpanded)))
		{
			return 1;
		}
	}

	if (oVal)
		*oVal = std::string(ExpandedBuf, LenExpanded);

	return 0;
}

int gs_config_key_ex_interpret_subst(
	const GsConfMap *KeyVal, const char *Key, std::string *oVal)
{
	const confmap_t::const_iterator &it = KeyVal->mMap.find(Key);

	if (it == KeyVal->mMap.end())
		return 1;

	if (!!gs_config_pattern_subst(it->second, oVal))
		return 1;

	return 0;
}

int gs_conf_map_create(struct GsConfMap **oConfMap)
{
	*oConfMap = new GsConfMap();
	return 0;
}

int gs_conf_map_destroy(struct GsConfMap *ConfMap)
{
	if (ConfMap)
		delete ConfMap;
	return 0;
}

int gs_config_parse_find_next_newline(const char *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew)
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

int gs_config_parse_skip_newline(const char *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew)
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

int gs_config_parse(
	const char *BufferBuf, size_t LenBuffer,
	GsConfMap **oKeyVal)
{
	int r = 0;

	GsConfMap *KeyVal = NULL;

	uint32_t Offset = 0;
	uint32_t OldOffset = 0;
	const char *DataStart = BufferBuf;
	uint32_t DataLength = (uint32_t) LenBuffer;

	const char hashsign = '#';
	const char equals = '=';
	const char hdr_nulterm_expected[] = GS_CONFIG_HEADER_STRING;
	const size_t hdr_raw_size = sizeof(hdr_nulterm_expected) - 1;

	if (!!(r = gs_conf_map_create(&KeyVal)))
		GS_GOTO_CLEAN();

	OldOffset = Offset;
	if (!!(r = gs_config_parse_find_next_newline(DataStart, DataLength, Offset, &Offset)))
		goto clean;
	/* hdr_raw_size of ASCII letters and 1 of NEWLINE */
	if (hdr_raw_size < Offset - OldOffset)
	{ r = 1; goto clean; }
	if (memcmp(hdr_nulterm_expected, DataStart + OldOffset, hdr_raw_size) != 0)
	{ r = 1; goto clean; }
	if (!!(r = gs_config_parse_skip_newline(DataStart, DataLength, Offset, &Offset)))
		goto clean;

	while (Offset < DataLength) {

		/* find where the current line ends */

		OldOffset = Offset;
		if (!!(r = gs_config_parse_find_next_newline(DataStart, DataLength, Offset, &Offset)))
			goto clean;

		/* extract current line - line should be of format 'KKK=VVV' */

		std::string line(DataStart + OldOffset, DataStart + Offset);

		if (line.size() && line[0] == hashsign) {
			/* comment */

			if (!!(r = gs_config_parse_skip_newline(DataStart, DataLength, Offset, &Offset)))
				goto clean;
		}
		else {
			/* split extracted line into KKK and VVV parts by equal sign */

			size_t equalspos = line.npos;
			if ((equalspos = line.find_first_of(equals, 0)) == line.npos)
			{ r = 1; goto clean; }
			std::string key(line.data() + 0, line.data() + equalspos);
			std::string val(line.data() + equalspos + 1, line.data() + line.size());

			/* record the gotten key value pair */

			KeyVal->mMap[key] = val;

			/* skip to the next line (or end of buffer) */

			if (!!(r = gs_config_parse_skip_newline(DataStart, DataLength, Offset, &Offset)))
				goto clean;
		}
	}

	if (oKeyVal)
		*oKeyVal = KeyVal;

clean:
	if (!!r) {
		GS_DELETE_F(&KeyVal, gs_conf_map_destroy);
	}

	return r;
}

/* returned value scoped not even to map lifetime - becomes invalid on map modification so do not do that */
const char * gs_config_key(const GsConfMap *KeyVal, const char *Key)
{
	const confmap_t::const_iterator &it = KeyVal->mMap.find(Key);
	if (it == KeyVal->mMap.end())
		return NULL;
	return it->second.c_str();
}

int gs_config_key_uint32(const GsConfMap *KeyVal, const char *Key, uint32_t *oVal)
{
	assert(sizeof(uint32_t) <= sizeof(long long));
	const confmap_t::const_iterator &it = KeyVal->mMap.find(Key);
	if (it == KeyVal->mMap.end())
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

int gs_config_read_fullpath(
	const char *PathFullBuf, size_t LenPathFull,
	GsConfMap **oKeyVal)
{
	int r = 0;

	const char newline = '\n';
	const char equals = '=';
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

	if (!!(r = gs_config_parse(retbuffer.data(), retbuffer.size(), oKeyVal)))
		goto clean;

clean:
	if (f)
		fclose(f);

	return r;
}

int gs_config_read_builtin(GsConfMap **oKeyVal)
{
	int r = 0;

	std::string BufferBuiltinConfig(GS_CONFIG_BUILTIN_HEXSTRING);
	std::string DecodedConfig;

	if (!!(r = gs_config_decode_hex(BufferBuiltinConfig, &DecodedConfig)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_parse(
		DecodedConfig.data(), DecodedConfig.size(),
		oKeyVal)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_config_read_builtin_or_relative_current_executable(
	const char *ExpectedLocationBuf, size_t LenExpectedLocation,
	const char *ExpectedNameBuf, size_t LenExpectedName,
	GsConfMap **oKeyVal)
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

	// FIXME: EXIST_ENSURE
	if (!!(r = gs_file_exist(PathFullBuf, LenPathFull, &PathIsExist)))
		GS_GOTO_CLEAN();

	if (PathIsExist) {
		/* read from the file system */

		if (!!(r = gs_config_read_fullpath(
			PathFullBuf, LenPathFull,
			oKeyVal)))
		{
			GS_GOTO_CLEAN();
		}
	} else {
		/* use the builtin config (preprocessor definition) */

		if (!!(r = gs_config_read_builtin(oKeyVal)))
			GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_config_read_default_everything(GsConfMap **oKeyVal)
{
	int r = 0;

	const char LocBuf[] = GS_CONFIG_DEFAULT_RELATIVE_PATHNAME;
	size_t LenLoc = (sizeof(GS_CONFIG_DEFAULT_RELATIVE_PATHNAME)) - 1;
	const char NameBuf[] = GS_CONFIG_DEFAULT_RELATIVE_FILENAME;
	size_t LenName = (sizeof(GS_CONFIG_DEFAULT_RELATIVE_FILENAME)) - 1;

	if (!!(r = gs_config_read_builtin_or_relative_current_executable(
		LocBuf, LenLoc,
		NameBuf, LenName,
		oKeyVal)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_config_get_common_vars(
	GsConfMap *KeyVal,
	GsAuxConfigCommonVars *oCommonVars)
{
	int r = 0;

	GsAuxConfigCommonVars CommonVars = {};

	GS_CONFIG_COMMON_VAR_UINT32_NONUCF(KeyVal, CommonVars, ServPort);
	GS_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, ServHostName);
	GS_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, RefNameMain);
	GS_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, RefNameSelfUpdate);
	GS_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoMainPath);
	GS_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoSelfUpdatePath);
	GS_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoMasterUpdatePath);
	GS_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, RepoMasterUpdateCheckoutPath);
	GS_CONFIG_COMMON_VAR_UINT32_NONUCF(KeyVal, CommonVars, ServBlobSoftSizeLimit);
	GS_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, MaintenanceBkpPath);
	GS_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, MainDirPath);
	GS_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KeyVal, CommonVars, SelfUpdateExePath);
	GS_CONFIG_COMMON_VAR_STRING_NONUCF(KeyVal, CommonVars, SelfUpdateBlobName);

	if (oCommonVars)
		*oCommonVars = CommonVars;

clean:

	return r;
}
