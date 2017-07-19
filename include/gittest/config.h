#ifndef _GITTEST_CONFIG_H_
#define _GITTEST_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#define GS_CONFIG_DEFAULT_RELATIVE_PATHNAME "."
#define GS_CONFIG_DEFAULT_RELATIVE_FILENAME "GsConfig.conf"

struct GsConfMap;

/** value struct
    manual-init struct
*/
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
	uint32_t ServBlobSoftSizeLimit;
	char *MaintenanceBkpPathBuf; size_t LenMaintenanceBkpPath;
	char *MainDirPathBuf; size_t LenMainDirPath;
	char *SelfUpdateExePathBuf; size_t LenSelfUpdateExePath;
};

int gs_conf_map_create(struct GsConfMap **oConfMap);
int gs_conf_map_destroy(struct GsConfMap *ConfMap);

int gs_config_parse_find_next_newline(const char *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew);
int gs_config_parse_skip_newline(const char *DataStart, uint32_t DataLength, uint32_t Offset, uint32_t *OffsetNew);
int gs_config_parse(
	const char *BufferBuf, size_t LenBuffer,
	GsConfMap **oKeyVal);

const char * gs_config_key(const GsConfMap *KeyVal, const char *Key);
int gs_config_key_uint32(const GsConfMap *KeyVal, const char *Key, uint32_t *oVal);

int gs_config_read_fullpath(
	const char *PathFullBuf, size_t LenPathFull,
	GsConfMap **oKeyVal);
int gs_config_read_builtin(GsConfMap **oKeyVal);
int gs_config_read_builtin_or_relative_current_executable(
	const char *ExpectedLocationBuf, size_t LenExpectedLocation,
	const char *ExpectedNameBuf, size_t LenExpectedName,
	GsConfMap **oKeyVal);
int gs_config_read_default_everything(GsConfMap **oKeyVal);

int gs_config_get_common_vars(
	GsConfMap *KeyVal,
	GsAuxConfigCommonVars *oCommonVars);

#endif /* _GITTEST_CONFIG_H_ */
