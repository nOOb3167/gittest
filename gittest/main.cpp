#include <cstdlib>
#include <cassert>
#include <cstdio>

#include <vector>
#include <set>
#include <list>

#include <git2.h>

/*
= git init =
fresh repositories have no "refs/heads/master" ref
*/

struct oid_comparator_t {
	bool operator()(const git_oid * const &a, const git_oid * const &b) {
		return git_oid_cmp(a, b) < 0;
	}
};

typedef ::std::set<const git_oid *, oid_comparator_t> toposet_t;
typedef ::std::list<git_tree *> topolist_t;

/* takes ownership of 'Tree' on success (list responsible for disposal) */
int tree_toposort_visit(git_repository *Repository, toposet_t *MarkSet, topolist_t *NodeList, git_tree *Tree) {
	int r = 0;
	/* = if n is not marked (i.e. has not been visited yet) then = */
	if (MarkSet->find(git_tree_id(Tree)) == MarkSet->end()) {
		/* = mark n = */
		MarkSet->insert(git_tree_id(Tree));
		/* = for each node m with an edge from n to m do = */
		size_t entrycount = git_tree_entrycount(Tree);
		for (int i = 0; i < entrycount; i++) {
			git_tree *TreeSubtree = NULL;
			if (git_tree_entry_type(git_tree_entry_byindex(Tree, i)) != GIT_OBJ_TREE)
				continue;
			if (!!(r = git_tree_lookup(&TreeSubtree, Repository, git_tree_entry_id(git_tree_entry_byindex(Tree, i)))))
				goto cleansub;
			/* = visit(m) = */
			/* ownership of 'Tree' by recursive call below taken only on success. therefore on failure we can free. */
			if (!!(r = tree_toposort_visit(Repository, MarkSet, NodeList, TreeSubtree)))
				goto cleansub;
			continue;
		cleansub:
			if (TreeSubtree)
				git_tree_free(TreeSubtree);
			return r;
		}
		/* = add n to head of L = */
		NodeList->push_front(Tree);
	}
	return r;
}

int tree_toposort(git_repository *Repository, git_tree *Tree, topolist_t *oNodeList) {
	/* https://en.wikipedia.org/wiki/Topological_sorting#Depth-first_search */
	int r = 0;
	toposet_t MarkSet; /* filled by tree-scoped git_oid - no need to free */
	topolist_t NodeList; /* filled by owned git_tree - must free */
	if (!!(r = tree_toposort_visit(Repository, &MarkSet, &NodeList, Tree)))
		goto clean;
	oNodeList->swap(NodeList);

clean:
	if (!!r) {
		for (topolist_t::iterator it = NodeList.begin(); it != NodeList.end(); it++)
			git_tree_free(*it);
	}

	return r;
}

int serv_oid_latest(git_repository *Repository, git_oid *OidLatest) {
	int r = 0;

	git_oid OidHead ={};

	if (!!(r = git_reference_name_to_id(&OidHead, Repository, "refs/heads/master")))
		goto clean;

	if (OidLatest)
		*OidLatest = OidHead;

clean:

	return r;
}

int serv_oid_treelist(git_repository *Repository, git_oid *Oid) {
	int r = 0;

	git_odb *Odb = NULL;
	git_odb_object *ObjectOid = NULL;
	git_commit *CommitOid = NULL;
	git_tree *TreeOid = NULL;
	
	topolist_t NodeList;

	if (!!(r = git_repository_odb(&Odb, Repository)))
		goto clean;

	if (!git_odb_exists(Odb, Oid))
	    { r = 1; goto clean; }

	// FIXME: really poor header-only read support (read_header_loose @ https://github.com/libgit2/libgit2/blob/master/src/odb_loose.c)
	//git_odb_read_header
	if (!!(r = git_odb_read(&ObjectOid, Odb, Oid)))
		goto clean;
	if (git_odb_object_type(ObjectOid) != GIT_OBJ_COMMIT)
	    { r = 2; goto clean; }

	if (!!(r = git_commit_lookup(&CommitOid, Repository, Oid)))
		goto clean;

	const char *CommitMessage = git_commit_message(CommitOid);
	printf("CommitMessage [%s]\n", CommitMessage);

	if (!!(r = git_commit_tree(&TreeOid, CommitOid)))
		goto clean;

	if (!!(r = tree_toposort(Repository, TreeOid, &NodeList)))
		goto clean;

	for (topolist_t::iterator it = NodeList.begin(); it != NodeList.end(); it++) {
		char buf[GIT_OID_HEXSZ] ={};
		git_oid_fmt(buf, git_tree_id(*it));
		printf("tree [%.*s]\n", GIT_OID_HEXSZ, buf);
	}

clean:
	if (TreeOid)
		git_tree_free(TreeOid);
	if (CommitOid)
		git_commit_free(CommitOid);
	if (ObjectOid)
		git_odb_object_free(ObjectOid);
	if (Odb)
		git_odb_free(Odb);

	return r;
}

int stuff() {
	int r = 0;

	git_buf RepoPath ={};
	git_repository *Repository = NULL;
	git_repository *RepositoryT = NULL;
	git_odb *Odb = NULL;
	git_oid OidHeadT ={};
	git_oid OidLatest ={};

	git_oid OidZero ={};
	assert(git_oid_iszero(&OidZero));

	if (!!(r = git_repository_discover(&RepoPath, ".", 0, NULL)))
		goto clean;
	if (!!(r = git_repository_open(&Repository, RepoPath.ptr)))
		goto clean;
	if (!!(r = git_repository_open(&RepositoryT, "../data/repo0/.git")))
		goto clean;

	if (!!(r = git_repository_odb(&Odb, Repository)))
		goto clean;

	int errNameToOidT = git_reference_name_to_id(&OidHeadT, RepositoryT, "refs/heads/master");
	assert(errNameToOidT == 0 || errNameToOidT == GIT_ENOTFOUND);
	if (errNameToOidT == GIT_ENOTFOUND)
		git_oid_cpy(&OidHeadT, &OidZero);

	if (!!(r = serv_oid_latest(Repository, &OidLatest)))
		goto clean;

	if (git_oid_cmp(&OidHeadT, &OidLatest) == 0) {
		char buf[GIT_OID_HEXSZ] ={};
		git_oid_fmt(buf, &OidLatest);
		printf("Have latest [%.*s]\n", GIT_OID_HEXSZ, buf);
		goto clean;
	}

	if (!!(r = serv_oid_treelist(Repository, &OidLatest)))
		goto clean;

clean:
	if (Odb)
		git_odb_free(Odb);
	if (RepositoryT)
		git_repository_free(RepositoryT);
	if (Repository)
		git_repository_free(Repository);

	return r;
}

int main(int argc, char **argv) {

	git_libgit2_init();

	int r = stuff();
	assert(!r);

	return EXIT_SUCCESS;
}
