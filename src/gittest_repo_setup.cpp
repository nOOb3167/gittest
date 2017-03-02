#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <cstring>

#include <filesystem>  // FIXME: NONSTANDARD

#include <git2.h>
#include <git2/sys/memes.h>

#include <gittest/misc.h>
#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h>

#define GS_REPO_SETUP_ARG_UPDATEMODE            "--gsreposetup"
#define GS_REPO_SETUP_ARG_COMMIT_SELFUPDATE     "--xcommit_selfupdate"
#define GS_REPO_SETUP_ARG_COMMIT_MAIN           "--xcommit_main"
#define GS_REPO_SETUP_ARG_CREATE_MASTER_UPDATE  "--xcreate_master_update"

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

int gs_repo_init(const char *RepoPathBuf, size_t LenRepoPath, const char *OptHardcodedSanityCheck) {
	int r = 0;

	git_repository *Repository = NULL;
	int errR = 0;
	int InitFlags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR | GIT_REPOSITORY_INIT_BARE;
	git_repository_init_options InitOptions = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	
	assert(InitOptions.version == 1 && GIT_REPOSITORY_INIT_OPTIONS_VERSION == 1);

	GS_LOG(I, PF, "Repository initializing [%.*s]", LenRepoPath, RepoPathBuf);

	if (OptHardcodedSanityCheck && (strstr(RepoPathBuf, OptHardcodedSanityCheck) == NULL))
		GS_ERR_CLEAN(1);

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

	GS_LOG(I, PF, "Repository creating", LenRepoPath, RepoPathBuf);

	if (!!(r = git_repository_init_ext(&Repository, RepoPathBuf, &InitOptions)))
		GS_GOTO_CLEAN();

noclean:

clean:
	if (Repository)
		git_repository_free(Repository);

	return r;
}

int gs_repo_setup_main_mode_commit_selfupdate(
	const char *RepoPathBuf, size_t LenRepoPath,
	const char *RefNameSelfUpdate, size_t LenRefNameSelfUpdate,
	const char *ExecutableFileNameBuf, size_t LenExecutableFileName)
{
	int r = 0;

	git_repository *Repository = NULL;
	git_oid BlobOid = {};
	git_treebuilder *TreeBuilder = NULL;
	git_oid TreeOid = {};
	git_oid CommitOid = {};

	if (!!(r = gs_repo_init(RepoPathBuf, LenRepoPath, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoPathBuf, &Repository)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "creating blob and tree");

	if (!!(r = gs_file_exist_ensure(ExecutableFileNameBuf, LenExecutableFileName)))
		GS_ERR_CLEAN(1);

	if (!!(r = git_blob_create_fromdisk(&BlobOid, Repository, ExecutableFileNameBuf)))
		GS_GOTO_CLEAN();

	if (!!(r = git_treebuilder_new(&TreeBuilder, Repository, NULL)))
		GS_GOTO_CLEAN();

	// FIXME: really GIT_FILEMODE_BLOB_EXECUTABLE? makes sense but what about just GIT_FILEMODE_BLOB?
	if (!!(r = git_treebuilder_insert(NULL, TreeBuilder, GS_STR_PARENT_EXPECTED_SUFFIX, &BlobOid, GIT_FILEMODE_BLOB_EXECUTABLE)))
		GS_GOTO_CLEAN();

	if (!!(r = git_treebuilder_write(&TreeOid, TreeBuilder)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "creating commit");

	if (!!(r = clnt_commit_ensure_dummy(Repository, &TreeOid, &CommitOid)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "updating ref [%.*s]", LenRefNameSelfUpdate, RefNameSelfUpdate);

	if (!!(r = clnt_commit_setref(Repository, RefNameSelfUpdate, &CommitOid)))
		GS_GOTO_CLEAN();

clean:
	if (TreeBuilder)
		git_treebuilder_free(TreeBuilder);

	if (Repository)
		git_repository_free(Repository);

	return r;
}

int gs_repo_setup_main_mode_commit_main(
	const char *RepoPathBuf, size_t LenRepoPath,
	const char *RefNameMain, size_t LenRefNameMain,
	const char *DirectoryFileNameBuf, size_t LenDirectoryFileName)
{
	int r = 0;

	git_repository *Repository = NULL;
	const char *OldWorkDir = NULL;
	git_index *Index = NULL;
	
	const char *PathSpecAllC = "*";
	const char **ArrPathSpecAllC = &PathSpecAllC;
	git_strarray PathSpecAll = {};
	PathSpecAll.count = 1;
	PathSpecAll.strings = (char **)ArrPathSpecAllC;

	git_oid TreeOid = {};
	git_oid CommitOid = {};

	if (!!(r = gs_repo_init(RepoPathBuf, LenRepoPath, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoPathBuf, &Repository)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "better way to commit a directory once libgit2 multiple worktree support lands");

	OldWorkDir = git_repository_workdir(Repository);

	GS_LOG(I, PF, "old workdir [%s]", OldWorkDir ? OldWorkDir : "(bare)");

	// FIXME: consider git_repository_state_cleanup
	// FIXME: consider setting an in-memory index, all all entries, then updating the main index

	if (!!(r = git_repository_set_workdir(Repository, DirectoryFileNameBuf, 0)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "updating index");

	// FIXME: it would be nice to use an in-memory index - but git_index_add_all is a filesystem operation
	if (!!(r = git_repository_index(&Index, Repository)))
		GS_GOTO_CLEAN();

	if (!!(r = git_index_clear(Index)))
		GS_GOTO_CLEAN();

	if (!!(r = git_index_add_all(Index, &PathSpecAll, GIT_INDEX_ADD_FORCE, NULL, NULL)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "creating tree");

	if (!!(r = git_index_write_tree_to(&TreeOid, Index, Repository)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "creating commit");

	if (!!(r = clnt_commit_ensure_dummy(Repository, &TreeOid, &CommitOid)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "updating ref [%.*s]", LenRefNameMain, RefNameMain);

	if (!!(r = clnt_commit_setref(Repository, RefNameMain, &CommitOid)))
		GS_GOTO_CLEAN();

clean:
	if (Index)
		git_index_free(Index);

	if (Repository)
		git_repository_free(Repository);

	return r;
}

int gs_repo_setup_main_mode_create_master_update(
	const char *RepoMasterUpdatePathBuf, size_t LenRepoMasterUpdatePath)
{
	int r = 0;

	if (!!(r = gs_repo_init(RepoMasterUpdatePathBuf, LenRepoMasterUpdatePath, "RepoMasterUpdate")))
		GS_GOTO_CLEAN();

clean:

	return r;
}

int gs_repo_setup_main(
	int argc, char **argv,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	const char *RepoMasterUpdatePathBuf, size_t LenRepoMasterUpdatePath,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *RefNameMainBuf, size_t LenRefNameMain)
{
	int r = 0;

	size_t LenRepoPath = 0;
	char RepoPathBuf[512] = {};

	GS_LOG(I, S, "start");

	if (argc < 2)
		GS_ERR_NO_CLEAN_L(0, I, PF, "no update done ([argc=%d])", argc);

	if (strcmp(argv[1], GS_REPO_SETUP_ARG_UPDATEMODE) != 0)
		GS_ERR_NO_CLEAN_L(0, I, PF, "no update done ([arg=%s])", argv[1]);

	if (argc < 3)
		GS_ERR_CLEAN_L(1, I, PF, "args ([argc=%d])", argc);

	if (strcmp(argv[2], GS_REPO_SETUP_ARG_COMMIT_SELFUPDATE) == 0) {
		GS_LOG(I, S, "commit_selfupdate start");
		if (argc != 4)
			GS_ERR_CLEAN(1);
		const size_t LenArgvExecutableFileName = strlen(argv[3]);
		if (!!(r = gs_repo_setup_main_mode_commit_selfupdate(
			RepoSelfUpdatePathBuf, LenRepoSelfUpdatePath,
			RefNameSelfUpdateBuf, LenRefNameSelfUpdate,
			argv[3], LenArgvExecutableFileName)))
		{
			GS_GOTO_CLEAN();
		}
	} else if (strcmp(argv[2], GS_REPO_SETUP_ARG_COMMIT_MAIN) == 0) {
		GS_LOG(I, S, "commit_main start");
		if (argc != 4)
			GS_ERR_CLEAN(1);
		const size_t LenArgvDirectoryFileName = strlen(argv[3]);
		if (!!(r = gs_repo_setup_main_mode_commit_main(
			RepoMainPathBuf, LenRepoMainPath,
			RefNameMainBuf, LenRefNameMain,
			argv[3], LenArgvDirectoryFileName)))
		{
			GS_GOTO_CLEAN();
		}
	} else if (strcmp(argv[2], GS_REPO_SETUP_ARG_CREATE_MASTER_UPDATE) == 0) {
		GS_LOG(I, S, "create_master_update start");
		if (argc != 3)
			GS_ERR_CLEAN(1);
		if (!!(r = gs_repo_setup_main_mode_create_master_update(
			RepoMasterUpdatePathBuf, LenRepoMasterUpdatePath)))
		{
			GS_GOTO_CLEAN();
		}
	} else {
		GS_LOG(I, PF, "unrecognized argument [%.s]", argv[2]);
		GS_ERR_CLEAN(1);
	}

noclean:

clean:

	return r;
}

int main(int argc, char **argv) {
	int r = 0;

	confmap_t KeyVal;

	GsAuxConfigCommonVars CommonVars = {};

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_crash_handler_setup()))
		GS_GOTO_CLEAN();

	GS_LOG_ADD(gs_log_create_ret("repo_setup"));

	if (!!(r = aux_config_read_interpret_relative_current_executable("../data", "gittest_config_serv.conf", &KeyVal)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_get_common_vars(KeyVal, &CommonVars)))
		GS_GOTO_CLEAN();

	{
		log_guard_t log(GS_LOG_GET("repo_setup"));			
			
		if (!!(r = gs_repo_setup_main(
			argc, argv,
			CommonVars.RepoMainPathBuf, CommonVars.LenRepoMainPath,
			CommonVars.RepoSelfUpdatePathBuf, CommonVars.LenRepoSelfUpdatePath,
			CommonVars.RepoMasterUpdatePathBuf, CommonVars.LenRepoMasterUpdatePath,
			CommonVars.RefNameSelfUpdateBuf, CommonVars.LenRefNameSelfUpdate,
			CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain)))
		{
			GS_GOTO_CLEAN();
		}
	}

clean:
	if (!!(r = gs_log_crash_handler_dump_global_log_list()))
		assert(0);

	if (!!r)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}