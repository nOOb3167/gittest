#ifndef _GITTEST_GITTEST_SELFUPDATE_H_
#define _GITTEST_GITTEST_SELFUPDATE_H_

#include <cstddef>
#include <cstdint>

#include <gittest/misc.h>
#include <gittest/gittest.h>

#define GS_SELFUPDATE_ARG_UPDATEMODE "--gsselfupdate"
#define GS_SELFUPDATE_ARG_PARENT "--xparent"
#define GS_SELFUPDATE_ARG_CHILD "--xchild"
#define GS_SELFUPDATE_ARG_MAIN "--xmain"
#define GS_SELFUPDATE_ARG_VERSUB "--xversub"

#define GS_STR_PARENT_EXPECTED_EXTENSION GS_STR_EXECUTABLE_EXPECTED_EXTENSION
#define GS_STR_PARENT_EXTRA_SUFFIX "_helper"
#define GS_STR_PARENT_EXTRA_SUFFIX_OLD "_helper_old"

/* to be implemented platform neutrally */

int gs_build_child_filename(
	const char *ParentFileNameBuf, size_t LenParentFileName,
	const char *ExpectedSuffix, size_t LenExpectedSuffix,
	const char *ExpectedExtension, size_t LenExpectedExtension,
	const char *ExtraSuffix, size_t LenExtraSuffix,
	char *ioChildFileNameBuf, size_t ChildFileNameSize, size_t *oLenChildFileName);

int gs_selfupdate_crash_handler_dump_global_log_list(
	const char *ArgStrBuf, size_t LenArgStr);

int aux_selfupdate_main_mode_parent(uint32_t *oHaveUpdateShouldQuit);

int aux_selfupdate_main_mode_main();

int aux_selfupdate_main(int argc, char **argv, const char *DefVerSub, uint32_t *oHaveUpdateShouldQuit);

/* to be implemented per platform */

int aux_selfupdate_create_child(
	const char *FileNameChildBuf, size_t LenFileNameChild,
	uint8_t *BufferUpdateData, uint32_t BufferUpdateSize);

int aux_selfupdate_fork_parent_mode_main_and_quit(
	const char *FileNameParentBuf, size_t LenFileNameParent);

int aux_selfupdate_fork_child_and_quit(
	const char *FileNameChildBuf, size_t LenFileNameChild);

int aux_selfupdate_main_prepare_mode_child(int argc, char **argv);

#endif /* _GITTEST_GITTEST_SELFUPDATE_H_ */
