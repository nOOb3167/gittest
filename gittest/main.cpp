#include <cstdlib>
#include <cassert>

#include <git2.h>

void stuff() {
	git_buf RepoPath ={};
	git_repository *Repository = NULL;
	git_odb *Odb = NULL;
	git_reference *ReferenceHead = NULL;

	if (!!git_repository_discover(&RepoPath, ".", 0, NULL))
		assert(0);
	if (!!git_repository_open(&Repository, RepoPath.ptr))
		assert(0);
	if (!!git_repository_odb(&Odb, Repository))
		assert(0);
	if (!!git_reference_lookup(&ReferenceHead, Repository, "refs/heads/master"))
		assert(0);
}

int main(int argc, char **argv) {

	git_libgit2_init();

	stuff();

	return EXIT_SUCCESS;
}
