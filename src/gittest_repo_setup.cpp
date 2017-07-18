#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <cstring>

#include <algorithm>

#include <git2.h>
#include <git2/sys/memes.h>

#include <gittest/misc.h>
#include <gittest/config.h>
#include <gittest/filesys.h>
#include <gittest/log.h>
#include <gittest/gittest.h>
#include <gittest/gittest_selfupdate.h>

#include <sqlite3.h>

#define GS_REPO_SETUP_ARG_UPDATEMODE            "--gsreposetup"
#define GS_REPO_SETUP_ARG_COMMIT_SELFUPDATE     "--xcommit_selfupdate"
#define GS_REPO_SETUP_ARG_COMMIT_MAIN           "--xcommit_main"
#define GS_REPO_SETUP_ARG_CREATE_MASTER_UPDATE  "--xcreate_master_update"
#define GS_REPO_SETUP_ARG_MAINTENANCE           "--xmaintenance"
#define GS_REPO_SETUP_ARG_DUMMYPREP             "--xdummyprep"

GsLogList *g_gs_log_list_global = gs_log_list_global_create_cpp();

int gs_repo_init(const char *RepoPathBuf, size_t LenRepoPath, const char *OptHardcodedSanityCheck)
{
	int r = 0;

	git_repository *Repository = NULL;
	int errR = 0;
	int InitFlags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR | GIT_REPOSITORY_INIT_BARE;
	git_repository_init_options InitOptions = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	
	GS_ASSERT(InitOptions.version == 1 && GIT_REPOSITORY_INIT_OPTIONS_VERSION == 1);

	if (!!(r = gs_buf_ensure_haszero(RepoPathBuf, LenRepoPath + 1)))
		GS_GOTO_CLEAN();

	GS_LOG(I, PF, "Repository initializing [%.*s]", LenRepoPath, RepoPathBuf);

	if (OptHardcodedSanityCheck && std::string(RepoPathBuf, LenRepoPath).find(OptHardcodedSanityCheck) == std::string::npos)
		GS_ERR_CLEAN_L(1, E, PF, "Repository path suspicious [%.*s]", (int)LenRepoPath, RepoPathBuf);

	errR = git_repository_open_ext(NULL, RepoPathBuf, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL);
	if (!!errR && errR != GIT_ENOTFOUND)
		GS_ERR_CLEAN_L(1, E, PF, "Repository open error [%.*s]", (int)LenRepoPath, RepoPathBuf);
	if (errR == 0)
		GS_ERR_NO_CLEAN_L(0, I, PF, "Repository already exists [%.*s]", (int)LenRepoPath, RepoPathBuf);
	GS_ASSERT(errR == GIT_ENOTFOUND);

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

	if (!!(r = aux_repository_open(RepoPathBuf, LenRepoPath, &Repository)))
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

	if (!!(r = aux_repository_open(RepoPathBuf, LenRepoPath, &Repository)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "better way to commit a directory once libgit2 multiple worktree support lands");

	OldWorkDir = git_repository_workdir(Repository);

	GS_LOG(I, PF, "old workdir [%s]", OldWorkDir ? OldWorkDir : "(bare)");

	if (!!(r = git_repository_set_workdir(Repository, DirectoryFileNameBuf, 0)))
		GS_GOTO_CLEAN();

	GS_LOG(I, S, "updating index");

	/* NOTE: consider setting an in-memory index, adding all entries, then updating the main index
	         it would be nice to use an in-memory index - but git_index_add_all is a filesystem operation */
	if (!!(r = git_repository_index(&Index, Repository)))
		GS_GOTO_CLEAN();

	if (!!(r = git_index_clear(Index)))
		GS_GOTO_CLEAN();

	// FIXME: consider git_repository_state_cleanup

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

int gs_repo_setup_main_mode_maintenance(
	const char *RepoPathBuf, size_t LenRepoPath,
	const char *MaintenanceBkpPathBuf, size_t LenMaintenanceBkpPath)
{
	int r = 0;

	git_repository *Repository = NULL;

	std::vector<git_oid> ReachableOid;

	GS_BYPART_DATA_VAR(OidVector, BypartReachableOid);
	GS_BYPART_DATA_INIT(OidVector, BypartReachableOid, &ReachableOid);

	if (!!(r = gs_repo_init(RepoPathBuf, LenRepoPath, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoPathBuf, LenRepoPath, &Repository)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_reach_refs(Repository, gs_bypart_cb_OidVector, &BypartReachableOid)))
		GS_GOTO_CLEAN();

	std::sort(ReachableOid.begin(), ReachableOid.end(), oid_comparator_v_t());

	if (!!(r = git_memes_thunderdome(
		RepoPathBuf, LenRepoPath,
		ReachableOid.data(), ReachableOid.size(),
		MaintenanceBkpPathBuf, LenMaintenanceBkpPath,
		MaintenanceBkpPathBuf, LenMaintenanceBkpPath)))
	{
		GS_GOTO_CLEAN();
	}

clean:
	git_repository_free(Repository);

	return r;
}

int gs_repo_setup_main_mode_dummyprep(
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	const char *RepoMasterUpdatePathBuf, size_t LenRepoMasterUpdatePath,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *MainDirectoryFileNameBuf, size_t LenMainDirectoryFileName,
	const char *SelfUpdateExecutableFileNameBuf, size_t LenSelfUpdateExecutableFileName)
{
	int r = 0;

	const char SelfUpdateExecutableHardcodedName[] = GS_STR_PARENT_EXPECTED_SUFFIX;
	size_t LenSelfUpdateExecutableHardcodedName = sizeof SelfUpdateExecutableFileNameBuf - 1;

	size_t IsDir = 0;
	size_t IsNotDir = 0;

	git_repository *RepoMain = NULL;
	git_repository *RepoSelfUpdate = NULL;
	git_repository *RepoMasterUpdate = NULL;

	git_oid BlobSelfUpdateOid = {};
	git_oid TreeOid = {};
	git_oid TreeMainOid = {};
	git_oid TreeSelfUpdateOid = {};
	git_oid CommitOid = {};
	git_oid CommitMainOid = {};
	git_oid CommitSelfUpdateOid = {};

	if (!!(r = gs_file_is_directory(MainDirectoryFileNameBuf, LenMainDirectoryFileName, &IsDir)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_file_is_directory(SelfUpdateExecutableFileNameBuf, LenSelfUpdateExecutableFileName, &IsNotDir)))
		GS_GOTO_CLEAN();

	if (!IsDir || !IsNotDir)
		GS_ERR_CLEAN(1);

	if (!!(r = gs_repo_init(RepoMainPathBuf, LenRepoMainPath, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_repo_init(RepoSelfUpdatePathBuf, LenRepoSelfUpdatePath, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_repo_init(RepoMasterUpdatePathBuf, LenRepoMasterUpdatePath, NULL)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoMainPathBuf, LenRepoMainPath, &RepoMain)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoSelfUpdatePathBuf, LenRepoSelfUpdatePath, &RepoSelfUpdate)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_repository_open(RepoMasterUpdatePathBuf, LenRepoMasterUpdatePath, &RepoMasterUpdate)))
		GS_GOTO_CLEAN();

	/* dummy clnt (RepoMasterUpdate @ main and selfupdate) */

	if (!!(r = clnt_tree_ensure_dummy(RepoMasterUpdate, &TreeOid)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_commit_ensure_dummy(RepoMasterUpdate, &TreeOid, &CommitOid)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_commit_setref(RepoMasterUpdate, RefNameMainBuf, &CommitOid)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_commit_setref(RepoMasterUpdate, RefNameSelfUpdateBuf, &CommitOid)))
		GS_GOTO_CLEAN();

	/* nondummy serv (RepoMain @ main and RepoSelfUpdate @ selfupdate) */

	if (!!(r = clnt_tree_ensure_from_workdir(
		RepoMain,
		MainDirectoryFileNameBuf, LenMainDirectoryFileName,
		&TreeMainOid)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_commit_ensure_dummy(RepoMasterUpdate, &TreeMainOid, &CommitMainOid)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_commit_setref(RepoMasterUpdate, RefNameMainBuf, &CommitMainOid)))
		GS_GOTO_CLEAN();

	if (!!(r = git_blob_create_fromdisk(&BlobSelfUpdateOid, RepoMasterUpdate, SelfUpdateExecutableFileNameBuf)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_tree_ensure_single(
		RepoMasterUpdate,
		SelfUpdateExecutableHardcodedName, LenSelfUpdateExecutableHardcodedName,
		&BlobSelfUpdateOid,
		&TreeSelfUpdateOid)))
	{
		GS_GOTO_CLEAN();
	}

	if (!!(r = clnt_commit_ensure_dummy(RepoMasterUpdate, &TreeSelfUpdateOid, &CommitSelfUpdateOid)))
		GS_GOTO_CLEAN();

	if (!!(r = clnt_commit_setref(RepoMasterUpdate, RefNameSelfUpdateBuf, &CommitSelfUpdateOid)))
		GS_GOTO_CLEAN();

clean:
	git_repository_free(RepoMain);
	git_repository_free(RepoSelfUpdate);
	git_repository_free(RepoMasterUpdate);

	return r;
}

int gs_repo_setup_main(
	int argc, char **argv,
	const char *RepoMainPathBuf, size_t LenRepoMainPath,
	const char *RepoSelfUpdatePathBuf, size_t LenRepoSelfUpdatePath,
	const char *RepoMasterUpdatePathBuf, size_t LenRepoMasterUpdatePath,
	const char *RefNameMainBuf, size_t LenRefNameMain,
	const char *RefNameSelfUpdateBuf, size_t LenRefNameSelfUpdate,
	const char *MaintenanceBkpPathBuf, size_t LenMaintenanceBkpPath)
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
	} else if (strcmp(argv[2], GS_REPO_SETUP_ARG_MAINTENANCE) == 0) {
		GS_LOG(I, S, "maintenance start");
		if (argc != 3)
			GS_ERR_CLEAN(1);
		if (!!(r = gs_repo_setup_main_mode_maintenance(
			RepoMasterUpdatePathBuf, LenRepoMasterUpdatePath,
			MaintenanceBkpPathBuf, LenMaintenanceBkpPath)))
		{
			GS_GOTO_CLEAN();
		}
	} else if (strcmp(argv[2], GS_REPO_SETUP_ARG_DUMMYPREP) == 0) {
		GS_LOG(I, S, "dummyprep start");
		if (argc != 5)
			GS_ERR_CLEAN(1);
		const size_t LenArgvDirectoryFileName = strlen(argv[3]);
		const size_t LenArgvExecutableFileName = strlen(argv[4]);
		if (!!(r = gs_repo_setup_main_mode_dummyprep(
			RepoMainPathBuf, LenRepoMainPath,
			RepoSelfUpdatePathBuf, LenRepoSelfUpdatePath,
			RepoMasterUpdatePathBuf, LenRepoMasterUpdatePath,
			RefNameMainBuf, LenRefNameMain,
			RefNameSelfUpdateBuf, LenRefNameSelfUpdate,
			argv[3], LenArgvDirectoryFileName,
			argv[4], LenArgvExecutableFileName)))
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

	struct GsConfMap *ConfMap = NULL;

	GsAuxConfigCommonVars CommonVars = {};

	if (!!(r = aux_gittest_init()))
		GS_GOTO_CLEAN();

	if (!!(r = gs_log_crash_handler_setup()))
		GS_GOTO_CLEAN();

	GS_LOG_ADD(gs_log_create_ret("repo_setup"));

	if (!!(r = gs_config_read_default_everything(&ConfMap)))
		GS_GOTO_CLEAN();

	if (!!(r = gs_config_get_common_vars(ConfMap, &CommonVars)))
		GS_GOTO_CLEAN();

	{
		log_guard_t log(GS_LOG_GET("repo_setup"));

		if (!!(r = gs_repo_setup_main(
			argc, argv,
			CommonVars.RepoMainPathBuf, CommonVars.LenRepoMainPath,
			CommonVars.RepoSelfUpdatePathBuf, CommonVars.LenRepoSelfUpdatePath,
			CommonVars.RepoMasterUpdatePathBuf, CommonVars.LenRepoMasterUpdatePath,
			CommonVars.RefNameMainBuf, CommonVars.LenRefNameMain,
			CommonVars.RefNameSelfUpdateBuf, CommonVars.LenRefNameSelfUpdate,
			CommonVars.MaintenanceBkpPathBuf, CommonVars.LenMaintenanceBkpPath)))
		{
			GS_GOTO_CLEAN();
		}
	}

clean:
	GS_DELETE_F(&ConfMap, gs_conf_map_destroy);

	/* always dump logs. not much to do about errors here though */
	gs_log_crash_handler_dump_global_log_list_suffix("_log", strlen("_log"));

	if (!!r)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}