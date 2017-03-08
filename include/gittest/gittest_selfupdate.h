#ifndef _GITTEST_GITTEST_SELFUPDATE_H_
#define _GITTEST_GITTEST_SELFUPDATE_H_

#include <cstddef>
#include <cstdint>

#include <gittest/gittest.h>

#define GS_SELFUPDATE_ARG_UPDATEMODE "--gsselfupdate"
#define GS_SELFUPDATE_ARG_PARENT "--xparent"
#define GS_SELFUPDATE_ARG_CHILD "--xchild"
#define GS_SELFUPDATE_ARG_MAIN "--xmain"
#define GS_SELFUPDATE_ARG_VERSUB "--xversub"

#define GS_STR_PARENT_EXPECTED_SUFFIX "gittest_clnt.exe"
#define GS_STR_PARENT_EXPECTED_EXTENSION ".exe"
#define GS_STR_PARENT_EXTRA_SUFFIX "_helper"

/* for use in GetTempFileName. GetTempFileName uses only 'up to the first three' chars */
#define GS_STR_TEMP_FILE_PREFIX_STRING "gst"

#define GS_AUX_CONFIG_COMMON_VAR_UINT32_NONUCF(KEYVAL, COMVARS, NAME)                  \
	{                                                                                  \
		uint32_t Conf ## NAME = 0;                                                     \
		if (!!(r = aux_config_key_uint32((KEYVAL), "Conf" # NAME, & Conf ## NAME))) \
			goto clean;                                                                \
		(COMVARS).NAME = Conf ## NAME;                                                 \
	}

#define GS_AUX_CONFIG_COMMON_VAR_STRING_NONUCF(KEYVAL, COMVARS, NAME)                                         \
		{                                                                                                     \
		std::string Conf ## NAME;                                                                             \
		if (!!(r = aux_config_key_ex((KEYVAL), "Conf" # NAME, & Conf ## NAME)))                            \
			goto clean;                                                                                       \
		if (!!(r = aux_char_from_string_alloc(Conf ## NAME, &(COMVARS).NAME ## Buf, &(COMVARS).Len ## NAME))) \
			goto clean;                                                                                       \
		}

#define GS_AUX_CONFIG_COMMON_VAR_STRING_INTERPRET_RELATIVE_CURRENT_EXECUTABLE_NONUCF(KEYVAL, COMVARS, NAME)                                                    \
	{                                                                                                                    \
		std::string Conf ## NAME;                                                                                        \
		if (!!(r = aux_config_key_ex_interpret_relative_current_executable((KEYVAL), "Conf" # NAME, & Conf ## NAME))) \
			goto clean;                                                                                                  \
		if (!!(r = aux_char_from_string_alloc(Conf ## NAME, &(COMVARS).NAME ## Buf, &(COMVARS).Len ## NAME)))            \
			goto clean;                                                                                                  \
	}


struct GsAuxConfigCommonVars {
	uint32_t ServPort;
	char *ServHostNameBuf; size_t LenServHostName;
	char *RefNameMainBuf; size_t LenRefNameMain;
	char *RefNameSelfUpdateBuf; size_t LenRefNameSelfUpdate;
	char *RepoMainPathBuf; size_t LenRepoMainPath;
	char *RepoSelfUpdatePathBuf; size_t LenRepoSelfUpdatePath;
	char *RepoMasterUpdatePathBuf; size_t LenRepoMasterUpdatePath;

	GS_AUX_MARKER_STRUCT_IS_COPYABLE;
};

int aux_config_read_interpret_relative_current_executable(
	const char *ExpectedLocation, const char *ExpectedName, std::map<std::string, std::string> *oKeyVal);
int aux_config_key_ex_interpret_relative_current_executable(
	const confmap_t &KeyVal, const char *Key, std::string *oVal);
int aux_config_get_common_vars(
	const confmap_t &KeyVal,
	GsAuxConfigCommonVars *oCommonVars);

void gs_debug_break();

int gs_file_exist_ensure(const char *FileNameBuf, size_t LenFileName);

int gs_get_current_executable_filename(char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName);

int gs_build_current_executable_relative_filename(
	const char *RelativeBuf, size_t LenRelativeBuf,
	char *ioCombinedBuf, size_t CombinedBufSize, size_t *LenCombinedBuf);

int gs_build_current_executable_relative_filename_(
	const char *RelativeBuf, size_t LenRelativeBuf,
	char *ioCombinedBuf, size_t CombinedBufSize, size_t *LenCombinedBuf);

int gs_build_path_interpret_relative_current_executable(
	const char *PossiblyRelativePathBuf, size_t LenPossiblyRelativePath,
	char *ioPathBuf, size_t PathBufSize, size_t *oLenPathBuf);

int gs_build_child_filename(
	const char *ParentFileNameBuf, size_t LenParentFileName,
	const char *ExpectedSuffix, size_t LenExpectedSuffix,
	const char *ExpectedExtension, size_t LenExpectedExtension,
	const char *ExtraSuffix, size_t LenExtraSuffix,
	char *ioChildFileNameBuf, size_t ChildFileNameSize, size_t *oLenChildFileName);

int aux_selfupdate_create_child(
	const char *FileNameChildBuf, size_t LenFileNameChild,
	uint8_t *BufferUpdateData, uint32_t BufferUpdateSize);

int aux_selfupdate_fork_and_quit(const char *FileNameChildBuf, size_t LenFileNameChild);

int aux_selfupdate_overwrite_parent(
	const char *ArgvHandleSerialized, size_t ArgvHandleSerializedSize,
	const char *ArgvParentFileName, size_t ArgvParentFileNameSize,
	const char *ArgvChildFileName, size_t ArgvChildFileNameSize);

int aux_selfupdate_main_mode_parent(uint32_t *oHaveUpdateShouldQuit);
int aux_selfupdate_main_mode_child(
	const char *ArgvHandleSerialized, size_t ArgvHandleSerializedSize,
	const char *ArgvParentFileName, size_t ArgvParentFileNameSize,
	const char *ArgvChildFileName, size_t ArgvChildFileNameSize);
int aux_selfupdate_main_mode_main();

int aux_selfupdate_main(int argc, char **argv, const char *DefVerSub, uint32_t *oHaveUpdateShouldQuit);

#endif /* _GITTEST_GITTEST_SELFUPDATE_H_ */
