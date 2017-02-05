#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstdint>

#include <vector>
#include <set>
#include <list>

#include <git2.h>
#include <git2/sys/repository.h>  /* git_repository_new (no backends so custom may be added) */
#include <git2/sys/mempack.h>     /* in-memory backend */

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
		cleansub:
			if (!!r) {
				if (TreeSubtree)
					git_tree_free(TreeSubtree);
			}
			if (!!r)
				goto clean;
		}
		/* = add n to head of L = */
		NodeList->push_front(Tree);
	}

clean:

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

void aux_uint32_t_LE(uint32_t a, char *buf, size_t bufsize) {
	assert(sizeof(uint32_t) == 4 && bufsize == 4);
	buf[0] = (a >> 0) & 0xFF;
	buf[1] = (a >> 8) & 0xFF;
	buf[2] = (a >> 16) & 0xFF;
	buf[3] = (a >> 24) & 0xFF;
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

int serv_oid_treelist(git_repository *Repository, git_oid *Oid, std::vector<git_oid> *oOutput) {
	int r = 0;

	git_odb *Odb = NULL;
	git_odb_object *ObjectOid = NULL;
	git_commit *CommitOid = NULL;
	git_tree *TreeOid = NULL;
	
	topolist_t NodeListTopo;
	std::vector<git_oid> Output;

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

	if (!!(r = tree_toposort(Repository, TreeOid, &NodeListTopo)))
		goto clean;

	for (topolist_t::iterator it = NodeListTopo.begin(); it != NodeListTopo.end(); it++) {
		char buf[GIT_OID_HEXSZ] ={};
		git_oid_fmt(buf, git_tree_id(*it));
		printf("tree [%.*s]\n", GIT_OID_HEXSZ, buf);
	}

	/* output in reverse topological order */
	Output.resize(NodeListTopo.size());  // FIXME: inefficient list size operation?
	int OutputIdx = 0;
	for (topolist_t::reverse_iterator it = NodeListTopo.rbegin(); it != NodeListTopo.rend(); it++, OutputIdx++)
		git_oid_cpy(Output.data() + OutputIdx,  git_tree_id(*it));

	if (oOutput)
		oOutput->swap(Output);

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

int serv_serialize_trees(git_repository *Repository, std::vector<git_oid> *TreeOid, std::string *oSizeBuffer, std::string *oTreeBuffer) {
	int r = 0;

	git_odb *Odb = NULL;

	std::vector<git_odb_object *> ObjectTree;
	std::vector<uint32_t>         ObjectTreeSize;
	size_t ObjectTreeCumulativeSize = 0;
	std::string SizeBuffer;
	std::string TreeBuffer;

	if (!!(r = git_repository_odb(&Odb, Repository)))
		goto clean;

	ObjectTree.resize(TreeOid->size());
	for (uint32_t i = 0; i < TreeOid->size(); i++) {
		if (!!(r == git_odb_read(&ObjectTree[i], Odb, TreeOid->data() + i)))
			goto clean;
		if (git_odb_object_type(ObjectTree[i]) != GIT_OBJ_TREE)
			{ r = 1; goto clean; }
	}

	ObjectTreeSize.resize(TreeOid->size());
	for (uint32_t i = 0; i < TreeOid->size(); i++) {
		ObjectTreeSize[i] = git_odb_object_size(ObjectTree[i]);
		ObjectTreeCumulativeSize += ObjectTreeSize[i];
	}

	assert(sizeof(uint32_t) == 4);
	SizeBuffer.reserve(ObjectTree.size() * sizeof(uint32_t));
	TreeBuffer.reserve(ObjectTreeCumulativeSize);
	for (uint32_t i = 0; i < ObjectTree.size(); i++) {
		char sizebuf[4] ={};
		aux_uint32_t_LE(ObjectTreeSize[i], sizebuf, 4);
		SizeBuffer.append(sizebuf,
			4);
		TreeBuffer.append(static_cast<const char *>(git_odb_object_data(ObjectTree[i])),
			ObjectTreeSize[i]);
	}

	if (oSizeBuffer)
		oSizeBuffer->swap(SizeBuffer);
	if (oTreeBuffer)
		oTreeBuffer->swap(TreeBuffer);

clean:
	for (uint32_t i = 0; i < ObjectTree.size(); i++)
		if (ObjectTree[i])
			git_odb_object_free(ObjectTree[i]);

	if (Odb)
		git_odb_free(Odb);

	return r;
}

int clnt_missing_trees(git_repository *RepositoryT, std::vector<git_oid> *Treelist, std::vector<git_oid> *oMissingTreeList) {
	int r = 0;

	std::vector<git_oid> MissingTree;

	for (uint32_t i = 0; i < Treelist->size(); i++) {
		git_tree *TmpTree = NULL;
		int errTree = (r = git_tree_lookup(&TmpTree, RepositoryT, &(*Treelist)[i]));
		// FIXME: not sure if GIT_ENOUTFOUND counts as official API but reading code, it is throws
		if (errTree == GIT_ENOTFOUND)
			MissingTree.push_back((*Treelist)[i]);
		if (!!r)
			goto cleansub;
	cleansub:
		if (TmpTree)
			git_tree_free(TmpTree);
		if (!!r)
			goto clean;
	}

	if (oMissingTreeList)
		oMissingTreeList->swap(MissingTree);

clean:

	return r;
}

int clnt_missing_blobs(git_repository *RepositoryT, std::string *SizeBuffer, std::string *TreeBuffer, std::vector<git_oid> *oMissingBlobList) {
	int r = 0;

	std::vector<git_oid> DeserializedTree;

	git_repository *RepositoryMemory = NULL;
	git_odb_backend *BackendMemory = NULL;
	git_odb *RepositoryOdb = NULL;

	/* https://github.com/libgit2/libgit2/blob/master/include/git2/sys/repository.h */
	if (!!(r = git_repository_new(&RepositoryMemory)))
		goto clean;

	/* https://github.com/libgit2/libgit2/blob/master/include/git2/sys/mempack.h */

	if (!!(r = git_mempack_new(&BackendMemory)))
		goto clean;

	if (!!(r = git_repository_odb(&RepositoryOdb, RepositoryMemory)))
		goto clean;

	if (!!(r = git_odb_add_backend(RepositoryOdb, BackendMemory, 999)))
		goto clean;

	assert(SizeBuffer->size() == TreeBuffer->size());
	DeserializedTree.resize(TreeBuffer->size());
	for (uint32_t idx = 0, i = 0; i < TreeBuffer->size(); idx+=(*SizeBuffer)[i], i++) {
		git_oid FreshOid ={};
		/* supposedly git_odb_stream_write recommended */
		// FIXME: assuming contiguous std::string etc
		if (!!(r = git_odb_write(&FreshOid, RepositoryOdb, TreeBuffer->data() + idx, (*SizeBuffer)[i], GIT_OBJ_TREE)))
			goto clean;
		git_oid_cpy(&DeserializedTree[i], &FreshOid);
	}

	for (uint32_t i = 0; i < DeserializedTree.size(); i++) {
		git_tree *TreeMemory = NULL;
		git_tree *TreeT = NULL;
		int errM = git_tree_lookup(&TreeMemory, RepositoryMemory, &DeserializedTree[i]);
		int errT = git_tree_lookup(&TreeT, RepositoryT, &DeserializedTree[i]);
		if (!!errM)  // we just inserted into memory repository. must be present.
			{ r = 1; goto clean; }
		if (errT != GIT_ENOTFOUND)  // must be non-present (missing tree)
			{ r = 1; goto clean; }
		assert(errM == 0 && errT == GIT_ENOTFOUND);

		size_t entrycount = git_tree_entrycount(TreeMemory);
		for (uint32_t i = 0; i < entrycount; i++) {
			const git_tree_entry *EntryMemory = git_tree_entry_byindex(TreeMemory, i);
			//git_tree_entry_id()
			assert(0); // FIXME:
		}
	}

clean:
	// FIXME: afaik other objects are attached and owned by git_repository and destroyed automatically
	if (RepositoryMemory)
		git_repository_free(RepositoryMemory);

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

	std::vector<git_oid> Treelist;

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

	if (!!(r = serv_oid_treelist(Repository, &OidLatest, &Treelist)))
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
