#ifndef _GITTEST_CONFIG_H_
#define _GITTEST_CONFIG_H_

#include <stdint.h>

#include <string>
#include <map>


#define GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_PATHNAME "../data"
#define GS_SELFUPDATE_CONFIG_DEFAULT_RELATIVE_FILENAME "gittest_config_serv.conf"

#define GS_AUX_CONFIG_COMMON_VAR_UINT32_NONUCF(KEYVAL, COMVARS, NAME)                  \
	{                                                                                  \
		uint32_t Conf ## NAME = 0;                                                     \
		if (!!(r = aux_config_key_uint32((KEYVAL), "Conf" # NAME, & Conf ## NAME)))    \
			goto clean;                                                                \
		(COMVARS).NAME = Conf ## NAME;                                                 \
	}

#define GS_AUX_CONFIG_COMMON_VAR_STRING_NONUCF(KEYVAL, COMVARS, NAME)                                         \
		{                                                                                                     \
		std::string Conf ## NAME;                                                                             \
		if (!!(r = aux_config_key_ex((KEYVAL), "Conf" # NAME, & Conf ## NAME)))                               \
			goto clean;                                                                                       \
		if (!!(r = aux_char_from_string_alloc(Conf ## NAME, &(COMVARS).NAME ## Buf, &(COMVARS).Len ## NAME))) \
			goto clean;                                                                                       \
		}

#define GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KEYVAL, COMVARS, NAME)                                                    \
	{                                                                                                                    \
		std::string Conf ## NAME;                                                                                        \
		if (!!(r = aux_config_key_ex_interpret_relative_current_executable((KEYVAL), "Conf" # NAME, & Conf ## NAME)))    \
			goto clean;                                                                                                  \
		if (!!(r = aux_char_from_string_alloc(Conf ## NAME, &(COMVARS).NAME ## Buf, &(COMVARS).Len ## NAME)))            \
			goto clean;                                                                                                  \
	}

typedef ::std::map<::std::string, ::std::string> confmap_t;

/** value struct
    manual-init struct
*/
struct GsAuxConfigCommonVars {
	uint32_t ServPort;
	char *ServHostNameBuf; size_t LenServHostName;
	char *RefNameMainBuf; size_t LenRefNameMain;
	char *RefNameSelfUpdateBuf; size_t LenRefNameSelfUpdate;
	char *RepoMainPathBuf; size_t LenRepoMainPath;
	char *RepoSelfUpdatePathBuf; size_t LenRepoSelfUpdatePath;
	char *RepoMasterUpdatePathBuf; size_t LenRepoMasterUpdatePath;
	char *RepoMasterUpdateCheckoutPathBuf; size_t LenRepoMasterUpdateCheckoutPath;
};

size_t aux_config_decode_hex_char_(const char *pHexChar, size_t *oIsError);
int aux_config_decode_hex_pairwise_swapped(const std::string &BufferSwapped, std::string *oDecoded);
int aux_config_parse_find_next_newline(const char *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew);
int aux_config_parse_skip_newline(const char *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew);
int aux_config_parse(
	const char *BufferBuf, size_t LenBuffer,
	std::map<std::string, std::string> *oKeyVal);
int aux_config_read_fullpath(
	const char *PathFullBuf, size_t LenPathFull,
	std::map<std::string, std::string> *oKeyVal);
const char * aux_config_key(const confmap_t &KeyVal, const char *Key);
int aux_config_key_ex(const confmap_t &KeyVal, const char *Key, std::string *oVal);
int aux_config_key_uint32(const confmap_t &KeyVal, const char *Key, uint32_t *oVal);

int aux_config_read_default_everything(std::map<std::string, std::string> *oKeyVal);
int aux_config_read_builtin(std::map<std::string, std::string> *oKeyVal);
int aux_config_read_builtin_or_relative_current_executable(
	const char *ExpectedLocationBuf, size_t LenExpectedLocation,
	const char *ExpectedNameBuf, size_t LenExpectedName,
	std::map<std::string, std::string> *oKeyVal);
int aux_config_key_ex_interpret_relative_current_executable(
	const confmap_t &KeyVal, const char *Key, std::string *oVal);
int aux_config_get_common_vars(
	const confmap_t &KeyVal,
	GsAuxConfigCommonVars *oCommonVars);

#endif /* _GITTEST_CONFIG_H_ */
