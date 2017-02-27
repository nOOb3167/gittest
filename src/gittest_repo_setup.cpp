#include <cassert>
#include <cstdlib>
#include <cstddef>

#include <filesystem>  // FIXME: NONSTANDARD

#include <git2.h>

#include <gittest/misc.h>
#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h>

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

int gs_repo_setup(const char *RepoRelativePathBuf, size_t LenRepoRelativePath) {
	int r = 0;

	size_t LenRepoPath = 0;
	char RepoPathBuf[512] = {};

	git_repository *Repository = NULL;
	int errR = 0;
	int InitFlags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR;
	git_repository_init_options InitOptions = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	
	assert(InitOptions.version == 1 && GIT_REPOSITORY_INIT_OPTIONS_VERSION == 1);

	GS_LOG(I, S, "gs_repo_setup");

	if (!!(r = gs_build_current_executable_relative_filename(
		RepoRelativePathBuf, LenRepoRelativePath,
		RepoPathBuf, sizeof RepoPathBuf, &LenRepoPath)))
	{
		GS_GOTO_CLEAN();
	}

	if (std::string(RepoPathBuf, LenRepoPath).find("RepoMasterUpdate") == std::string::npos)
		GS_ERR_CLEAN_L(1, E, PF, "Repository path suspicious [%.*s]", (int)LenRepoPath, RepoPathBuf);

	errR = git_repository_open_ext(NULL, RepoPathBuf, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL);
	if (!!errR && errR != GIT_ENOTFOUND)
		GS_ERR_CLEAN_L(1, E, PF, "Repository open error [%.*s]", (int)LenRepoPath, RepoPathBuf);
	if (errR == 0)
		GS_ERR_NO_CLEAN_L(0, I, PF, "Repository already exists [%.*s]", (int)LenRepoPath, RepoPathBuf);
	assert(errR == GIT_ENOTFOUND);

	/* MKPATH for whole path creation (MKDIR only the last component) */
	InitOptions.flags = InitFlags;
	InitOptions.mode = GIT_REPOSITORY_INIT_SHARED_UMASK;
	InitOptions.workdir_path = NULL;
	InitOptions.description = NULL;
	InitOptions.template_path = NULL;
	InitOptions.initial_head = NULL;
	InitOptions.origin_url = NULL;

	if (!!(r = git_repository_init_ext(&Repository, RepoPathBuf, &InitOptions)))
		GS_GOTO_CLEAN();

noclean:

clean:
	if (Repository)
		git_repository_free(Repository);

	return r;
}

int main(int argc, char **argv) {
	int r = 0;

	confmap_t KeyVal;
	std::string RepoRelativePath;

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_crash_handler_setup()))
		GS_GOTO_CLEAN();

	GS_LOG_ADD(gs_log_create_ret("repo_setup"));

	if (!!(r = aux_config_read("../data", "gittest_config_serv.conf", &KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(KeyVal, "ConfRepoMasterUpdateRelativePath", &RepoRelativePath)))
		GS_GOTO_CLEAN();

	{
		log_guard_t log(GS_LOG_GET("repo_setup"));

		if (!!(r = gs_repo_setup(RepoRelativePath.c_str(), RepoRelativePath.size())))
			GS_GOTO_CLEAN();
	}

clean:
	if (!!(r = gs_log_crash_handler_dump_global_log_list()))
		assert(0);

	if (!!r)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}