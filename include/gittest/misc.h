#ifndef _GITTEST_MISC_H_
#define _GITTEST_MISC_H_

#include <cassert>

#include <memory>
#include <string>
#include <map>

#include <gittest/config_defs.h>
#include <gittest/log_defs.h>

/*
* = Visual Studio debugger function call expression evaluation (variable watch) =
*   for whatever reason, it seems function calls (ex call function with side effects from a variable watch expression)
*   fail / stall out if the currently selected thread (ex shown current in threads window) is inside certain callstacks.
*   in particular std::this_thread::sleep_for.
*   the workaround enabling function evaluation is to first step out of such calls (back into app source code),
*   and only then attempt to trigger reevaluation.
*/

#define GS_STR_EXECUTABLE_EXPECTED_EXTENSION ".exe"

#if defined (_MSC_VER)
#define GS_THREAD_LOCAL_DESIGNATOR __declspec( thread )
#else
#define GS_THREAD_LOCAL_DESIGNATOR __thread
#endif

/* nulling destruction - delete P */
#define GS_DELETE(PTR_PTR_ALLOCATED_WITH_NEW, TYPE) \
  do {                                              \
    TYPE **ptr_ptr = (PTR_PTR_ALLOCATED_WITH_NEW);  \
	if (*ptr_ptr) {                                 \
	  delete *ptr_ptr;                              \
	  *ptr_ptr = NULL;                              \
	}                                               \
  } while (0)

/* nulling destruction - DELETER(P) */
#define GS_DELETE_F(PTR_PTR_VARNAME, FNAME)                \
  do {                                                     \
    decltype(PTR_PTR_VARNAME) ptr_ptr = (PTR_PTR_VARNAME); \
	if (*ptr_ptr) {                                        \
      if (!!((FNAME)(*ptr_ptr))) GS_ASSERT(0);             \
      *ptr_ptr = NULL;                                     \
	}                                                      \
  } while (0)

/* nulling destruction - DELETER(&P->base) */
#define GS_DELETE_BASE_F(PTR_PTR_VARNAME)                  \
  do {                                                     \
    decltype(PTR_PTR_VARNAME) ptr_ptr = (PTR_PTR_VARNAME); \
	if (*ptr_ptr) {                                        \
      if (!!((FNAME)(&(*ptr_ptr)->base))) GS_ASSERT(0);    \
      *ptr_ptr = NULL;                                     \
	}                                                      \
  } while (0)

/* nulling destruction - P->DELETER(P) */
#define GS_DELETE_VF(PTR_PTR_VARNAME, VFNAME)              \
  do {                                                     \
    decltype(PTR_PTR_VARNAME) ptr_ptr = (PTR_PTR_VARNAME); \
	if (*ptr_ptr) {                                        \
      if (!!((*ptr_ptr)->VFNAME(*ptr_ptr))) GS_ASSERT(0);  \
      *ptr_ptr = NULL;                                     \
	}                                                      \
  } while (0)

/* nulling destruction - (&P->base)->DELETER(&P->base) */
#define GS_DELETE_BASE_VF(PTR_PTR_VARNAME, VFNAME)         \
  do {                                                     \
    decltype(PTR_PTR_VARNAME) ptr_ptr = (PTR_PTR_VARNAME); \
	decltype(*ptr_ptr) ptr = *ptr_ptr;                     \
	decltype(&ptr->base) ptr_base = (decltype(&ptr->base))(&ptr->base); \
	if (*ptr_ptr) {                                        \
      if (!!(ptr_base->VFNAME(ptr_base))) GS_ASSERT(0);    \
      *ptr_ptr = NULL;                                     \
	}                                                      \
  } while (0)

/* non-nulling destruction / release - DELETER(P) */
#define GS_RELEASE_F(VARNAME, FNAME) do { decltype(VARNAME) ptr = (VARNAME); GS_DELETE_F(&ptr, FNAME); } while (0)

#define GS_PP_BASE_DECL(PTR_VARNAME)                                            \
  decltype(PTR_VARNAME) * PTR_VARNAME ## PP = &(PTR_VARNAME);                   \
  /* PTR_VARNAME ## Dummy: at least check that 'base' exists as member field */ \
  decltype(&(PTR_VARNAME)->base) PTR_VARNAME ## Dummy = &(PTR_VARNAME)->base;   \
  /* check base is at struct offset zero basically */                           \
  GS_ASSERT((void *)((PTR_VARNAME)) == (void *)(&(PTR_VARNAME)->base));         \
  decltype(&(PTR_VARNAME)->base) * PTR_VARNAME ## PPBase = (decltype(&(PTR_VARNAME)->base) *)(&(PTR_VARNAME))

#define GS_ARGOWN(PTR_PTR, TYPE) ((TYPE *)gs_aux_argown((void **)(PTR_PTR)))


#define GS_DEBUG_BREAK() gs_debug_break()

#define GS_ASSERT(x) \
	do { bool the_x = (x); if (! the_x) { GS_DEBUG_BREAK(); assert(0); } } while (0)

#define GS_DBG_CLEAN() GS_CONFIG_DEFS_MISC_GS_GOTO_CLEAN_HANDLING

#define GS_DBG_LOG() GS_LOG(CLEAN, S, "CLEAN");

#define GS_ERR_NO_CLEAN(THE_R) do { r = (THE_R); GS_DBG_LOG(); goto noclean; } while(0)
#define GS_ERR_CLEAN(THE_R) do { r = (THE_R); GS_DBG_LOG(); GS_DBG_CLEAN(); goto clean; } while(0)
#define GS_GOTO_CLEAN() do { GS_DBG_LOG(); GS_DBG_CLEAN(); goto clean; } while(0)
#define GS_ERR_CLEANSUB(THE_R) do { r = (THE_R); GS_DBG_LOG(); GS_DBG_CLEAN(); goto cleansub; } while(0)
#define GS_GOTO_CLEANSUB() do { GS_DBG_LOG(); GS_DBG_CLEAN(); goto cleansub; } while(0)

#define GS_ERR_NO_CLEAN_L(THE_R, LEVEL, TT, ...) do { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_ERR_NO_CLEAN(THE_R); } while(0)
#define GS_ERR_CLEAN_L(THE_R, LEVEL, TT, ...) do { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_ERR_CLEAN(THE_R); } while(0)
#define GS_GOTO_CLEAN_L(LEVEL, TT, ...) do { GS_LOG(LEVEL, TT, __VA_ARGS__); GS_GOTO_CLEAN(); } while(0)

/* should not clash with other error codes etc - just used random.org */
#define GS_ERRCODE_RECONNECT 0x7BDD6EAF
#define GS_ERRCODE_EXIT      0x7BDD6EB0
#define GS_ERRCODE_TIMEOUT   0x7BDD6EB1

#define GS_AUX_MARKER_STRUCT_IS_COPYABLE /* dummy (marker / documentation purpose) */

#define GS_DUMMY_BLOCK() ((void) 0)

/* WARNING: evaluates arguments multiple times. rework using block with decltype assignment. */
#define GS_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define GS_MIN(x, y) (((x) < (y)) ? (x) : (y))

#define GS_SP_SET_RAW_NULLING(VARNAME_SP, VARNAME_PRAW, TYPENAME) \
	do { VARNAME_SP = std::shared_ptr<TYPENAME>(VARNAME_PRAW); VARNAME_PRAW = NULL; } while(0)

// FIXME: evil? two character identifier inside header..
template<typename T>
using sp = ::std::shared_ptr<T>;

void * gs_aux_argown(void **ptr);

int gs_build_modified_filename(
	const char *BaseFileNameBuf, size_t LenBaseFileName,
	const char *ExpectedSuffix, size_t LenExpectedSuffix,
	const char *ExpectedExtension, size_t LenExpectedExtension,
	const char *ExtraSuffix, size_t LenExtraSuffix,
	const char *ExtraExtension, size_t LenExtraExtension,
	char *ioModifiedFileNameBuf, size_t ModifiedFileNameSize, size_t *oLenModifiedFileName);

int gs_buf_copy_zero_terminate(
	const char *SrcBuf, size_t LenSrc,
	char *ioDstBuf, size_t DstBufSize, size_t *oLenDst);

int gs_buf_strnlen(const char *Buf, size_t BufSize, size_t *oLenBuf);

int gs_buf_ensure_haszero(const char *Buf, size_t BufSize);

int aux_char_from_string_alloc(const std::string &String, char **oStrBuf, size_t *oLenStr);

int gs_path_kludge_filenameize(char *ioPathBuf, size_t *ioLenPath);

int gs_build_path_interpret_relative_current_executable(
	const char *PossiblyRelativePathBuf, size_t LenPossiblyRelativePath,
	char *ioPathBuf, size_t PathBufSize, size_t *oLenPathBuf);

void gs_current_thread_name_set_cstr(
	const char *NameCStr);
void gs_current_thread_name_set_cstr_2(
	const char *BaseNameCStr,
	const char *optExtraNameCStr);

/* to be implemented per platform */

void gs_current_thread_name_set(
	const char *NameBuf,
	size_t LenName);

void gs_debug_break();

int gs_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute);

int gs_path_append_abs_rel(
	const char *AbsoluteBuf, size_t LenAbsolute,
	const char *RelativeBuf, size_t LenRelative,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath);

int gs_file_exist(
	const char *FileNameBuf, size_t LenFileName,
	size_t *oIsExist);

int gs_file_exist_ensure(const char *FileNameBuf, size_t LenFileName);

int gs_get_current_executable_filename(char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName);

int gs_get_current_executable_directory(
	char *ioCurrentExecutableDirBuf, size_t CurrentExecutableDirSize, size_t *oLenCurrentExecutableDir);

int gs_build_current_executable_relative_filename(
	const char *RelativeBuf, size_t LenRelative,
	char *ioCombinedBuf, size_t CombinedBufSize, size_t *oLenCombined);

#endif /* _GITTEST_MISC_H_ */
