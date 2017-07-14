#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */

#include <cstddef>
#include <cstring>

#include <windows.h>
#include <shlwapi.h> // PathAppend etc

#include <gittest/misc.h>
#include <gittest/filesys.h>
#include <gittest/filesys_win.h>

int gs_win_path_directory(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath);
int gs_win_path_canonicalize(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath);

int gs_win_path_directory(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath)
{
	int r = 0;

	char Drive[_MAX_DRIVE] = {};
	char Dir[_MAX_DIR] = {};
	char FName[_MAX_FNAME] = {};
	char Ext[_MAX_EXT] = {};

	/* http://www.flounder.com/msdn_documentation_errors_and_omissions.htm
	*    see for _splitpath: """no more than this many characters will be written to each buffer""" */
	_splitpath(InputPathBuf, Drive, Dir, FName, Ext);

	if (!!(r = _makepath_s(ioOutputPathBuf, OutputPathBufSize, Drive, Dir, NULL, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_buf_strnlen(ioOutputPathBuf, OutputPathBufSize, oLenOutputPath)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_win_path_canonicalize(
	const char *InputPathBuf, size_t LenInputPath,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath)
{
	int r = 0;

	/** required length for PathCanonicalize **/
	if (OutputPathBufSize < MAX_PATH || LenInputPath > MAX_PATH)
		GS_ERR_CLEAN(1);

	/** this does fucking nothing (ex retains mixed slash backslash) **/
	if (! PathCanonicalize(ioOutputPathBuf, InputPathBuf))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_buf_strnlen(ioOutputPathBuf, OutputPathBufSize, oLenOutputPath)))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_build_path_expand_separated(
	const char *PathBuf, size_t LenPath,
	const char *ExtBuf, size_t LenExt,
	const char *SeparatorBuf, size_t LenSeparator,
	char *ExpandedBuf, size_t ExpandedSize, size_t *oLenExpanded)
{
	int r = 0;

	char PatternBuf[1024] = {};
	size_t LenPattern = 0;

	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA FindData = {};
	DWORD dwError = ERROR_FILE_NOT_FOUND;

	std::string Accum;

	if (!!(r = gs_path_append_abs_rel(
		PathBuf, LenPath,
		ExtBuf, LenExt,
		PatternBuf, sizeof PatternBuf, &LenPattern)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_buf_ensure_haszero(PatternBuf, LenPattern + 1)))
		GS_GOTO_CLEAN();

	if ((INVALID_HANDLE_VALUE == (hFind = FindFirstFile(PatternBuf, &FindData))) &&
		(ERROR_FILE_NOT_FOUND != (dwError = GetLastError())))
	{
		GS_ERR_CLEAN(1);
	}

	if (INVALID_HANDLE_VALUE != hFind) {
		DWORD dwError2 = 0;

		do {
			size_t LenFileName = 0;

			char TmpBuf[1024] = {};
			size_t LenTmp = 0;

			if (!!(r = gs_buf_strnlen(FindData.cFileName, MAX_PATH, &LenFileName)))
				GS_GOTO_CLEAN();

			if (!!(r = gs_path_append_abs_rel(
				PathBuf, LenPath,
				FindData.cFileName, LenFileName,
				TmpBuf, sizeof TmpBuf, &LenTmp)))
			{
				GS_GOTO_CLEAN();
			}

			Accum.append(TmpBuf, LenTmp);
			Accum.append(SeparatorBuf, LenSeparator);
		} while (FindNextFile(hFind, &FindData) != 0);

		if (ERROR_NO_MORE_FILES != (dwError2 = GetLastError()))
			GS_ERR_CLEAN(1);
	}
	else {
		assert(ERROR_FILE_NOT_FOUND == dwError);
		Accum.append("");
	}

	if (Accum.size() + 1 >= ExpandedSize)
		GS_ERR_CLEAN(1);

	memmove(ExpandedBuf, Accum.data(), Accum.size());
	memset(ExpandedBuf + Accum.size(), '\0', 1);

	if (oLenExpanded)
		*oLenExpanded = Accum.size();

clean:
	if (hFind == INVALID_HANDLE_VALUE)
		FindClose(hFind);

	return r;
}

int gs_build_current_executable_relative_filename(
	const char *RelativeBuf, size_t LenRelative,
	char *ioCombinedBuf, size_t CombinedBufSize, size_t *oLenCombined)
{
	int r = 0;

	size_t LenPathCurrentExecutableDir = 0;
	char PathCurrentExecutableDirBuf[512] = {};
	size_t LenPathModification = 0;
	char PathModificationBuf[512] = {};

	/* get directory */
	if (!!(r = gs_get_current_executable_directory(
		PathCurrentExecutableDirBuf, sizeof PathCurrentExecutableDirBuf, &LenPathCurrentExecutableDir)))
	{
		GS_ERR_CLEAN(1);
	}

	/* ensure relative and append */

	if (!!(r = gs_path_append_abs_rel(
		PathCurrentExecutableDirBuf, LenPathCurrentExecutableDir,
		RelativeBuf, LenRelative,
		PathModificationBuf, sizeof PathModificationBuf, &LenPathModification)))
	{
		GS_GOTO_CLEAN();
	}

	/* canonicalize into output */

	if (!!(r = gs_win_path_canonicalize(
		PathModificationBuf, LenPathModification,
		ioCombinedBuf, CombinedBufSize, oLenCombined)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_get_current_executable_filename(char *ioFileNameBuf, size_t FileNameSize, size_t *oLenFileName)
{
	int r = 0;

	DWORD LenFileName = 0;

	LenFileName = GetModuleFileName(NULL, ioFileNameBuf, FileNameSize);
	if (!(LenFileName != 0 && LenFileName < FileNameSize))
		GS_ERR_CLEAN(1);

	if (oLenFileName)
		*oLenFileName = LenFileName;

clean:

	return r;
}

int gs_get_current_executable_directory(
	char *ioCurrentExecutableDirBuf, size_t CurrentExecutableDirSize, size_t *oLenCurrentExecutableDir)
{
	int r = 0;

	size_t LenCurrentExecutable = 0;
	char CurrentExecutableBuf[512] = {};

	if (!!(r = gs_get_current_executable_filename(
		CurrentExecutableBuf, sizeof CurrentExecutableBuf, &LenCurrentExecutable)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = gs_win_path_directory(
		CurrentExecutableBuf, LenCurrentExecutable,
		ioCurrentExecutableDirBuf, CurrentExecutableDirSize, oLenCurrentExecutableDir)))
	{
		GS_GOTO_CLEAN();
	}

clean:

	return r;
}

int gs_file_exist(
	const char *FileNameBuf, size_t LenFileName,
	size_t *oIsExist)
{
	int r = 0;

	int IsExist = 0;

	if (!!(r = gs_buf_ensure_haszero(FileNameBuf, LenFileName + 1)))
		GS_GOTO_CLEAN();

	/* https://blogs.msdn.microsoft.com/oldnewthing/20071023-00/?p=24713/ */
	/* INVALID_FILE_ATTRIBUTES if file does not exist, apparently */
	IsExist = !(INVALID_FILE_ATTRIBUTES == GetFileAttributes(FileNameBuf));

	if (oIsExist)
		*oIsExist = IsExist;

clean:

	return r;
}

int gs_file_exist_ensure(const char *FileNameBuf, size_t LenFileName)
{
	int r = 0;

	if (!!(r = gs_buf_ensure_haszero(FileNameBuf, LenFileName + 1)))
		GS_GOTO_CLEAN();

	/* https://blogs.msdn.microsoft.com/oldnewthing/20071023-00/?p=24713/ */
	/* INVALID_FILE_ATTRIBUTES if file does not exist, apparently */
	if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(FileNameBuf))
		GS_ERR_CLEAN(1);

clean:

	return r;
}

int gs_path_is_absolute(const char *PathBuf, size_t LenPath, size_t *oIsAbsolute)
{
	int r = 0;

	size_t IsAbsolute = false;

	if (!!(r = gs_buf_strnlen(PathBuf, LenPath + 1, NULL)))
		GS_GOTO_CLEAN();

	/* maximum length for PathIsRelative */
	if (LenPath > MAX_PATH)
		GS_ERR_CLEAN(1);

	IsAbsolute = ! PathIsRelative(PathBuf);

	if (oIsAbsolute)
		*oIsAbsolute = IsAbsolute;

clean:

	return r;
}

int gs_path_append_abs_rel(
	const char *AbsoluteBuf, size_t LenAbsolute,
	const char *RelativeBuf, size_t LenRelative,
	char *ioOutputPathBuf, size_t OutputPathBufSize, size_t *oLenOutputPath)
{
	int r = 0;

	size_t LenOutputPathTmp = 0;

	/** maximum length for PathIsRelative and PathAppend **/
	if (LenAbsolute > MAX_PATH || LenRelative > MAX_PATH)
		GS_ERR_CLEAN(1);

	if (PathIsRelative(AbsoluteBuf))
		GS_GOTO_CLEAN();

	if (! PathIsRelative(RelativeBuf))
		GS_GOTO_CLEAN();

	/* prep output buffer with absolute path */

	if (!!(r = gs_buf_copy_zero_terminate(
		AbsoluteBuf, LenAbsolute,
		ioOutputPathBuf, OutputPathBufSize, &LenOutputPathTmp)))
	{
		GS_GOTO_CLEAN();
	}

	/* append */

	if (! PathAppend(ioOutputPathBuf, RelativeBuf))
		GS_ERR_CLEAN(1);

	if (!!(r = gs_buf_strnlen(ioOutputPathBuf, OutputPathBufSize, oLenOutputPath)))
		GS_GOTO_CLEAN();

clean:

	return r;
}
