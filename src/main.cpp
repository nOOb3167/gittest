#ifdef _MSC_VER
#pragma warning(disable : 4267 4102)  // conversion from size_t, unreferenced label
#endif /* _MSC_VER */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif /* _MSC_VER */

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include <vector>
#include <map>
#include <set>
#include <list>
#include <string>
#include <utility>
#include <sstream>

#include <git2.h>
#include <git2/sys/repository.h>  /* git_repository_new (no backends so custom may be added) */
#include <git2/sys/mempack.h>     /* in-memory backend */

#include <gittest/gittest.h>

/*
= git init =
fresh repositories have no "refs/heads/master" ref
= resetting the git repo (nuke loose objects but packs remain) =
git gc --prune=all
*/

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
	
	if (oNodeList)
		oNodeList->swap(NodeList);

clean:
	if (!!r) {
		for (topolist_t::iterator it = NodeList.begin(); it != NodeList.end(); it++)
			git_tree_free(*it);
	}

	return r;
}

int aux_gittest_init() {
	git_libgit2_init();
	return 0;
}

int aux_config_read(const char *ExpectedLocation, const char *ExpectedName, std::map<std::string, std::string> *oKeyVal) {
	int r = 0;

	std::map<std::string, std::string> KeyVal;

	const char newline = '\n';
	const char equals  = '=';
	const char hdr_nulterm_expected[] = "GITTEST_CONF";
	const size_t hdr_raw_size = sizeof(hdr_nulterm_expected) - 1;

	const size_t ArbitraryBufferSize = 4096;
	char buf[ArbitraryBufferSize];

	std::stringstream locationss;
	std::string retbuffer;
	
	FILE *f = NULL;
    
    size_t ret = 0;
    size_t idx = 0;

	locationss << ExpectedLocation << "/" << ExpectedName;

	if (!(f = fopen(locationss.str().c_str(), "rb")))
		{ r = 1; goto clean; }

	while ((ret = fread(buf, 1, ArbitraryBufferSize, f)) > 0)
		retbuffer.append(buf, ret);

	if (ferror(f) || !feof(f))
		{ r = 1; goto clean; }

	/* hdr_raw_size of ASCII letters and 1 of NEWLINE */
	if (retbuffer.size() < hdr_raw_size + 1)
		{ r = 2; goto clean; }
	// FIXME: relying on string data() contiguity
	if (memcmp(hdr_nulterm_expected, retbuffer.data(), hdr_raw_size) != 0)
		{ r = 2; goto clean; }
	if (retbuffer.at(hdr_raw_size) != newline)
		{ r = 2; goto clean; }

	idx = hdr_raw_size + 1;
	while (idx < retbuffer.size()) {
		size_t newlinepos = retbuffer.npos;
		if ((newlinepos = retbuffer.find_first_of(newline, idx)) == retbuffer.npos)
			{ r = 3; goto clean; }
		
		std::string line(retbuffer.data() + idx, retbuffer.data() + newlinepos);
		size_t equalspos = line.npos;
		if ((equalspos = line.find_first_of(equals, 0)) == line.npos)
			{ r = 3; goto clean; }
		std::string key(line.data() + 0, line.data() + equalspos);
		std::string val(line.data() + equalspos + 1, line.data() + line.size());

		KeyVal[key] = val;

		idx = newlinepos + 1;
	}

	if (oKeyVal)
		oKeyVal->swap(KeyVal);

clean:
	if (f)
		fclose(f);

	return r;
}

/* returned value scoped not even to map lifetime - becomes invalid on map modification so do not do that */
const char * aux_config_key(const confmap_t &KeyVal, const char *Key) {
	const confmap_t::const_iterator &it = KeyVal.find(Key);
	if (it == KeyVal.end())
		return NULL;
	return it->second.c_str();
}

/* returned value copied */
int aux_config_key_ex(const confmap_t &KeyVal, const char *Key, std::string *oVal) {
	const confmap_t::const_iterator &it = KeyVal.find(Key);
	if (it == KeyVal.end())
		return 1;
	{
		std::string Val(it->second);
		if (oVal)
			oVal->swap(Val);
	}
	return 0;
}

int aux_config_key_uint32(const confmap_t &KeyVal, const char *Key, uint32_t *oVal) {
	assert(sizeof(uint32_t) <= sizeof(long long));
	const confmap_t::const_iterator &it = KeyVal.find(Key);
	if (it == KeyVal.end())
		return 1;
	{
		const char *startPtr = it->second.c_str();
		char *endPtr = 0;
		errno = 0;
		long long valLL = strtoll(startPtr, &endPtr, 10);
		if (errno = ERANGE && (valLL == LONG_MIN || valLL == LONG_MAX))
			return 2;
		if (endPtr != startPtr + it->second.size())
			return 2;
		if (oVal)
			*oVal = (uint32_t)valLL;
	}
	return 0;
}

void aux_uint32_to_LE(uint32_t a, char *oBuf, size_t bufsize) {
	assert(sizeof(uint32_t) == 4 && bufsize == 4);
	oBuf[0] = (a >> 0) & 0xFF;
	oBuf[1] = (a >> 8) & 0xFF;
	oBuf[2] = (a >> 16) & 0xFF;
	oBuf[3] = (a >> 24) & 0xFF;
}

void aux_LE_to_uint32(uint32_t *oA, const char *buf, size_t bufsize) {
	assert(sizeof(uint32_t) == 4 && bufsize == 4);
	uint32_t w = 0;
	w |= (buf[0] & 0xFF) << 0;
	w |= (buf[1] & 0xFF) << 8;
	w |= (buf[2] & 0xFF) << 16;
	w |= (buf[3] & 0xFF) << 24;
	*oA = w;
}

void aux_topolist_print(const topolist_t &NodeListTopo) {
	for (topolist_t::const_iterator it = NodeListTopo.begin(); it != NodeListTopo.end(); it++) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, git_tree_id(*it));
		printf("tree [%.*s]\n", sizeof buf, buf);
	}
}

int aux_oid_tree_blob_byname(git_repository *Repository, git_oid *TreeOid, const char *WantedBlobName, git_oid *oBlobOid) {
	int r = 0;

	git_tree *Tree = NULL;

	const git_tree_entry *Entry = NULL;
	git_otype EntryType = GIT_OBJ_BAD;
	const git_oid *BlobOid = NULL;

	if (!!(r = git_tree_lookup(&Tree, Repository, TreeOid)))
		goto clean;

	Entry = git_tree_entry_byname(Tree, WantedBlobName);

	if (! Entry)
		{ r = 1; goto clean; }

	EntryType = git_tree_entry_type(Entry);

	if (EntryType != GIT_OBJ_BLOB)
		{ r = 1; goto clean; }

	BlobOid = git_tree_entry_id(Entry);

	if (oBlobOid)
		git_oid_cpy(oBlobOid, BlobOid);

clean:
	if (Tree)
		git_tree_free(Tree);

	return r;
}

int aux_oid_latest_commit_tree(git_repository *Repository, const char *RefName, git_oid *oCommitHeadOid, git_oid *oTreeHeadOid) {
	/* return value GIT_ENOTFOUND is part of the API for this function */

	int r = 0;
	int errC = 0;

	git_oid CommitHeadOid = {};
	git_commit *CommitHead = NULL;
	git_tree *TreeHead = NULL;

	if (!!(r = git_reference_name_to_id(&CommitHeadOid, Repository, RefName)))
		goto clean;

	// FIXME: not sure if GIT_ENOTFOUND return counts as official API for git_commit_lookup
	errC = git_commit_lookup(&CommitHead, Repository, &CommitHeadOid);
	if (!!errC && errC != GIT_ENOTFOUND)
		{ r = errC; goto clean; }

	if (!!(r = git_commit_tree(&TreeHead, CommitHead)))
		goto clean;

	if (oCommitHeadOid)
		git_oid_cpy(oCommitHeadOid, &CommitHeadOid);

	if (oTreeHeadOid)
		git_oid_cpy(oTreeHeadOid, git_tree_id(TreeHead));

clean:
	if (TreeHead)
		git_tree_free(TreeHead);

	if (CommitHead)
		git_commit_free(CommitHead);

	return r;
}

int serv_latest_commit_tree_oid(git_repository *Repository, const char *RefName, git_oid *oCommitHeadOid, git_oid *oTreeHeadOid) {
	return aux_oid_latest_commit_tree(Repository, RefName, oCommitHeadOid, oTreeHeadOid);
}

int clnt_latest_commit_tree_oid(git_repository *RepositoryT, const char *RefName, git_oid *oCommitHeadOid, git_oid *oTreeHeadOid) {
	/* if the latest commit is not found, return success, setting output oids to zero */

	int r = 0;

	git_oid CommitHeadOid = {};
	git_oid TreeHeadOid = {};

	git_oid OidZero = {};
	assert(git_oid_iszero(&OidZero));

	int errX = aux_oid_latest_commit_tree(RepositoryT, RefName, &CommitHeadOid, &TreeHeadOid);
	if (!!errX && errX != GIT_ENOTFOUND)
	{
		r = errX; goto clean;
	}
	assert(errX == 0 || errX == GIT_ENOTFOUND);
	if (errX == GIT_ENOTFOUND) {
		git_oid_cpy(&CommitHeadOid, &OidZero);
		git_oid_cpy(&TreeHeadOid, &OidZero);
	}

	if (oCommitHeadOid)
		git_oid_cpy(oCommitHeadOid, &CommitHeadOid);

	if (oTreeHeadOid)
		git_oid_cpy(oTreeHeadOid, &TreeHeadOid);

clean:

	return r;
}

int serv_oid_treelist(git_repository *Repository, git_oid *TreeOid, std::vector<git_oid> *oOutput) {
	int r = 0;

	git_tree *Tree = NULL;
	
	topolist_t NodeListTopo;
	std::vector<git_oid> Output;

	int OutputIdx = 0;

	if (!!(r = git_tree_lookup(&Tree, Repository, TreeOid)))
		goto clean;

	if (!!(r = tree_toposort(Repository, Tree, &NodeListTopo)))
		goto clean;

	/* output in reverse topological order */
	Output.resize(NodeListTopo.size());  // FIXME: inefficient list size operation?
	for (topolist_t::reverse_iterator it = NodeListTopo.rbegin(); it != NodeListTopo.rend(); OutputIdx++, it++)
		git_oid_cpy(Output.data() + OutputIdx,  git_tree_id(*it));

	if (oOutput)
		oOutput->swap(Output);

clean:
	if (Tree)
		git_tree_free(Tree);

	return r;
}

int aux_serialize_objects(
	git_repository *Repository, std::vector<git_oid> *ObjectOid, git_otype ObjectWantedType,
	std::string *oSizeBuffer, std::string *oObjectBuffer)
{
	int r = 0;

	git_odb *Odb = NULL;

	std::vector<git_odb_object *> Object;
	std::vector<uint32_t>         ObjectSize;
	size_t ObjectCumulativeSize = 0;
	std::string SizeBuffer;
	std::string ObjectBuffer;

	if (!!(r = git_repository_odb(&Odb, Repository)))
		goto clean;

	Object.resize(ObjectOid->size());
	for (uint32_t i = 0; i < ObjectOid->size(); i++) {
		if (!!(r = git_odb_read(&Object[i], Odb, ObjectOid->data() + i)))
			goto clean;
		if (git_odb_object_type(Object[i]) != ObjectWantedType)
			{ r = 1; goto clean; }
	}

	ObjectSize.resize(ObjectOid->size());
	for (uint32_t i = 0; i < ObjectOid->size(); i++) {
		ObjectSize[i] = (uint32_t)git_odb_object_size(Object[i]);
		ObjectCumulativeSize += ObjectSize[i];
	}

	assert(sizeof(uint32_t) == 4);
	SizeBuffer.reserve(Object.size() * sizeof(uint32_t));
	ObjectBuffer.reserve(ObjectCumulativeSize);
	for (uint32_t i = 0; i < Object.size(); i++) {
		char sizebuf[sizeof(uint32_t)] = {};
		aux_uint32_to_LE(ObjectSize[i], sizebuf, sizeof sizebuf);
		SizeBuffer.append(sizebuf,
			sizeof sizebuf);
		ObjectBuffer.append(static_cast<const char *>(git_odb_object_data(Object[i])),
			ObjectSize[i]);
	}

	if (oSizeBuffer)
		oSizeBuffer->swap(SizeBuffer);

	if (oObjectBuffer)
		oObjectBuffer->swap(ObjectBuffer);

clean:
	for (uint32_t i = 0; i < Object.size(); i++)
		if (Object[i])
			git_odb_object_free(Object[i]);

	if (Odb)
		git_odb_free(Odb);

	return r;
}

int serv_serialize_trees(git_repository *Repository, std::vector<git_oid> *TreeOid, std::string *oSizeBuffer, std::string *oTreeBuffer) {
	return aux_serialize_objects(Repository, TreeOid, GIT_OBJ_TREE, oSizeBuffer, oTreeBuffer);
}

int serv_serialize_blobs(git_repository *Repository, std::vector<git_oid> *BlobOid, std::string *oSizeBuffer, std::string *oBlobBuffer) {
	return aux_serialize_objects(Repository, BlobOid, GIT_OBJ_BLOB, oSizeBuffer, oBlobBuffer);
}

int aux_deserialize_sizebuffer(uint8_t *DataStart, uint32_t DataLength, uint32_t OffsetSizeBuffer, uint32_t SizeVecLen, std::vector<uint32_t> *oSizeVector, size_t *oCumulativeSize) {
	int r = 0;

	std::vector<uint32_t> SizeVector;
	size_t CumulativeSize = 0;

	assert(DataStart + OffsetSizeBuffer + SizeVecLen * sizeof(uint32_t) <= DataStart + DataLength);

	SizeVector.resize(SizeVecLen);
	for (uint32_t i = 0; i < SizeVecLen; i++) {
		aux_LE_to_uint32(&SizeVector[i], (char *)(DataStart + OffsetSizeBuffer + i * sizeof(uint32_t)), 4);
		CumulativeSize += SizeVector[i];
	}

	if (oSizeVector)
		oSizeVector->swap(SizeVector);
	if (oCumulativeSize)
		*oCumulativeSize = CumulativeSize;

clean:

	return r;
}

int aux_deserialize_objects_odb(
	git_odb *OdbT,
	uint8_t *DataStartSizeBuffer, uint32_t DataLengthSizeBuffer, uint32_t OffsetSizeBuffer,
	uint8_t *DataStartObjectBuffer, uint32_t DataLengthObjectBuffer, uint32_t OffsetObjectBuffer,
	uint32_t PairedVecLen, git_otype WrittenObjectType, std::vector<git_oid> *oWrittenObjectOid)
{
	int r = 0;

	std::vector<git_oid> WrittenObjectOid;
	std::vector<uint32_t> SizeVector;
	size_t CumulativeSize = 0;

	if (!!(r = aux_deserialize_sizebuffer(DataStartSizeBuffer, DataLengthSizeBuffer, OffsetSizeBuffer, PairedVecLen, &SizeVector, &CumulativeSize)))
		goto clean;

	assert(DataStartObjectBuffer + OffsetObjectBuffer + CumulativeSize <= DataStartObjectBuffer + DataLengthObjectBuffer);

	WrittenObjectOid.resize(SizeVector.size());
	for (uint32_t idx = 0, i = 0; i < SizeVector.size(); idx+=SizeVector[i], i++) {
		git_oid FreshOid = {};
		/* supposedly git_odb_stream_write recommended */
		// FIXME: assuming contiguous std::string etc
		if (!!(r = git_odb_write(&FreshOid, OdbT, DataStartObjectBuffer + OffsetObjectBuffer + idx, SizeVector[i], WrittenObjectType)))
			goto clean;
		git_oid_cpy(&WrittenObjectOid[i], &FreshOid);
	}

	if (oWrittenObjectOid)
		oWrittenObjectOid->swap(WrittenObjectOid);

clean:

	return r;
}

int aux_deserialize_objects(
	git_repository *RepositoryT,
	uint8_t *DataStartSizeBuffer, uint32_t DataLengthSizeBuffer, uint32_t OffsetSizeBuffer,
	uint8_t *DataStartObjectBuffer, uint32_t DataLengthObjectBuffer, uint32_t OffsetObjectBuffer,
	uint32_t PairedVecLen, git_otype WrittenObjectType, std::vector<git_oid> *oWrittenObjectOid)
{
	int r = 0;

	git_odb *OdbT = NULL;

	std::vector<git_oid> WrittenObjectOid;

	if (!!(r = git_repository_odb(&OdbT, RepositoryT)))
		goto clean;

	if (!!(r = aux_deserialize_objects_odb(
		OdbT,
		DataStartSizeBuffer, DataLengthSizeBuffer, OffsetSizeBuffer,
		DataStartObjectBuffer, DataLengthObjectBuffer, OffsetObjectBuffer,
		PairedVecLen, WrittenObjectType, &WrittenObjectOid)))
	{
		goto clean;
	}

	if (oWrittenObjectOid)
		oWrittenObjectOid->swap(WrittenObjectOid);

clean:
	if (OdbT)
		git_odb_free(OdbT);

	return r;
}

int aux_clnt_deserialize_trees(
	git_odb *OdbT,
	uint8_t *DataStartSizeBuffer, uint32_t DataLengthSizeBuffer, uint32_t OffsetSizeBuffer,
	uint8_t *DataStartObjectBuffer, uint32_t DataLengthObjectBuffer, uint32_t OffsetObjectBuffer,
	uint32_t PairedVecLen, std::vector<git_oid> *oDeserializedTree)
{
	return aux_deserialize_objects_odb(OdbT,
		DataStartSizeBuffer, DataLengthSizeBuffer, OffsetSizeBuffer,
		DataStartObjectBuffer, DataLengthObjectBuffer, OffsetObjectBuffer,
		PairedVecLen, GIT_OBJ_TREE, oDeserializedTree);
}

int clnt_deserialize_trees(
	git_repository *RepositoryT,
	uint8_t *DataStartSizeBuffer, uint32_t DataLengthSizeBuffer, uint32_t OffsetSizeBuffer,
	uint8_t *DataStartObjectBuffer, uint32_t DataLengthObjectBuffer, uint32_t OffsetObjectBuffer,
	uint32_t PairedVecLen, std::vector<git_oid> *oDeserializedTree)
{
	return aux_deserialize_objects(
		RepositoryT,
		DataStartSizeBuffer, DataLengthSizeBuffer, OffsetSizeBuffer,
		DataStartObjectBuffer, DataLengthObjectBuffer, OffsetObjectBuffer,
		PairedVecLen, GIT_OBJ_TREE, oDeserializedTree);
}

int clnt_deserialize_blobs(
	git_repository *RepositoryT,
	uint8_t *DataStartSizeBuffer, uint32_t DataLengthSizeBuffer, uint32_t OffsetSizeBuffer,
	uint8_t *DataStartObjectBuffer, uint32_t DataLengthObjectBuffer, uint32_t OffsetObjectBuffer,
	uint32_t PairedVecLen, std::vector<git_oid> *oDeserializedBlob)
{
	return aux_deserialize_objects(
		RepositoryT,
		DataStartSizeBuffer, DataLengthSizeBuffer, OffsetSizeBuffer,
		DataStartObjectBuffer, DataLengthObjectBuffer, OffsetObjectBuffer,
		PairedVecLen, GIT_OBJ_BLOB, oDeserializedBlob);
}

int clnt_missing_trees(git_repository *RepositoryT, std::vector<git_oid> *Treelist, std::vector<git_oid> *oMissingTreeList) {
	int r = 0;

	std::vector<git_oid> MissingTree;
	std::vector<git_tree *> TmpTree;

	TmpTree.resize(Treelist->size());
	for (uint32_t i = 0; i < Treelist->size(); i++) {
		// FIXME: not sure if GIT_ENOUTFOUND return counts as official API for git_tree_lookup
		int errTree = git_tree_lookup(&TmpTree[i], RepositoryT, &(*Treelist)[i]);
		if (errTree != 0 && errTree != GIT_ENOTFOUND)
			{ r = 1; goto clean; }
		if (errTree == 0)
			continue;
		assert(errTree == GIT_ENOTFOUND);
		MissingTree.push_back((*Treelist)[i]);
	}

	if (oMissingTreeList)
		oMissingTreeList->swap(MissingTree);

clean:
	for (uint32_t i = 0; i < TmpTree.size(); i++)
		if (TmpTree[i])
			git_tree_free(TmpTree[i]);

	return r;
}

int aux_memory_repository_new(git_repository **oRepositoryMemory) {
	int r = 0;

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

	if (oRepositoryMemory)
		*oRepositoryMemory = RepositoryMemory;

clean:
	// FIXME: afaik backend is owned by git_repository and destroyed automatically

	if (RepositoryOdb)
		git_odb_free(RepositoryOdb);

	if (!!r) {
		if (RepositoryMemory)
			git_repository_free(RepositoryMemory);
	}

	return r;
}

int aux_clnt_dual_lookup_expect_missing(
	git_repository *RepositoryMemory, git_repository *RepositoryT,
	git_oid *TreeOid,
	git_tree **oTreeMem, git_tree **oTreeT)
{
	/*
	* meant for use while processing missing trees,
	* where trees are first inserted into a memory backend-ed repository.
	* therefore tree is expected to be missing in the other repository.
	*/

	int r = 0;

	git_tree *TreeMem = NULL;
	git_tree *TreeT   = NULL;

	int errM = git_tree_lookup(&TreeMem, RepositoryMemory, TreeOid);
	int errT = git_tree_lookup(&TreeT, RepositoryT, TreeOid);
	if (!!errM)  // should have been inserted into memory repository. must be present.
		{ r = 1; goto clean; }
	if (errT != GIT_ENOTFOUND)  // must be non-present (missing tree)
		{ r = 1; goto clean; }
	assert(errM == 0 && errT == GIT_ENOTFOUND);

	if (oTreeMem)
		*oTreeMem = TreeMem;
	if (oTreeT)
		*oTreeT = TreeT;

clean:
	if (!!r) {
		if (TreeT)
			git_tree_free(TreeT);

		if (TreeMem)
			git_tree_free(TreeMem);
	}

	return r;
}

int clnt_missing_blobs_bare(
	git_repository *RepositoryT,
	uint8_t *DataStartSizeBuffer, uint32_t DataLengthSizeBuffer, uint32_t OffsetSizeBuffer,
	uint8_t *DataStartObjectBuffer, uint32_t DataLengthObjectBuffer, uint32_t OffsetObjectBuffer,
	uint32_t PairedVecLen, std::vector<git_oid> *oMissingBloblist)
{
	int r = 0;

	std::vector<git_oid> DeserializedTree;
	std::vector<std::pair<git_tree *, git_tree *> > TreeMem_TreeT;
	std::vector<git_oid> MissingBloblist;

	git_repository *RepositoryMemory = NULL;
	git_odb *RepositoryMemoryOdb = NULL;
	git_odb *RepositoryTOdb = NULL;

	git_oid OidZero = {};
	assert(git_oid_iszero(&OidZero));

	if (!!(r = aux_memory_repository_new(&RepositoryMemory)))
		goto clean;

	if (!!(r = git_repository_odb(&RepositoryMemoryOdb, RepositoryMemory)))
		goto clean;

	if (!!(r = git_repository_odb(&RepositoryTOdb, RepositoryT)))
		goto clean;

	if (!!(r = aux_clnt_deserialize_trees(
		RepositoryMemoryOdb,
		DataStartSizeBuffer, DataLengthSizeBuffer, OffsetSizeBuffer,
		DataStartObjectBuffer, DataLengthObjectBuffer, OffsetObjectBuffer,
		PairedVecLen, &DeserializedTree)))
	{
		goto clean;
	}

	TreeMem_TreeT.resize(DeserializedTree.size());
	for (uint32_t i = 0; i < DeserializedTree.size(); i++) {
		if (!!(r = aux_clnt_dual_lookup_expect_missing(RepositoryMemory, RepositoryT, &DeserializedTree[i],
			&TreeMem_TreeT[i].first, &TreeMem_TreeT[i].second)))
		{
			goto clean;
		}
		size_t entrycount = git_tree_entrycount(TreeMem_TreeT[i].first);
		for (uint32_t j = 0; j < entrycount; j++) {
			const git_tree_entry *EntryMemory = git_tree_entry_byindex(TreeMem_TreeT[i].first, j);
			const git_oid *EntryMemoryOid = git_tree_entry_id(EntryMemory);
			const git_otype EntryMemoryType = git_tree_entry_type(EntryMemory);
			if (EntryMemoryType == GIT_OBJ_TREE)
				continue;
			assert(EntryMemoryType == GIT_OBJ_BLOB);
			if (git_odb_exists(RepositoryTOdb, EntryMemoryOid))
				continue;
			MissingBloblist.push_back(OidZero);
			git_oid_cpy(&MissingBloblist[MissingBloblist.size() - 1], EntryMemoryOid);
		}
	}

	if (oMissingBloblist)
		oMissingBloblist->swap(MissingBloblist);

clean:
	for (uint32_t i = 0; i < TreeMem_TreeT.size(); i++) {
		if (TreeMem_TreeT[i].first)
			git_tree_free(TreeMem_TreeT[i].first);
		if (TreeMem_TreeT[i].second)
			git_tree_free(TreeMem_TreeT[i].second);
	}

	if (RepositoryTOdb)
		git_odb_free(RepositoryTOdb);

	if (RepositoryMemoryOdb)
		git_odb_free(RepositoryMemoryOdb);

	if (RepositoryMemory)
		git_repository_free(RepositoryMemory);

	return r;
}

int clnt_missing_blobs(git_repository *RepositoryT, uint32_t PairedVecLen, std::string *SizeBuffer, std::string *TreeBuffer, std::vector<git_oid> *oMissingBloblist) {
	return clnt_missing_blobs_bare(
		RepositoryT,
		(uint8_t *)SizeBuffer->data(), SizeBuffer->size(), 0,
		(uint8_t *)TreeBuffer->data(), TreeBuffer->size(), 0,
		PairedVecLen, oMissingBloblist);
}

int aux_commit_buffer_checkexist_dummy(git_odb *OdbT, git_buf *CommitBuf, uint32_t *oExists, git_oid *oCommitOid) {
	int r = 0;

	git_oid CommitOid = {};

	uint32_t Exists = 0;

	if (!!(r = git_odb_hash(&CommitOid, CommitBuf->ptr, CommitBuf->size, GIT_OBJ_COMMIT)))
		goto clean;

	Exists = git_odb_exists(OdbT, &CommitOid);

	if (oExists)
		*oExists = Exists;

	if (oCommitOid)
		git_oid_cpy(oCommitOid, &CommitOid);

clean:

	return r;
}

int aux_commit_buffer_dummy(git_repository *RepositoryT, git_oid *TreeOid, git_buf *oCommitBuf) {
	int r = 0;

	git_tree *Tree = NULL;
	git_signature *Signature = NULL;
	git_buf CommitBuf = {};

	if (!!(r = git_tree_lookup(&Tree, RepositoryT, TreeOid)))
		goto clean;

	if (!!(r = git_signature_new(&Signature, "DummyName", "DummyEMail", 0, 0)))
		goto clean;
	
	if (!!(r = git_commit_create_buffer(&CommitBuf, RepositoryT, Signature, Signature, "UTF-8", "Dummy", Tree, 0, NULL)))
		goto clean;

	if (oCommitBuf)
		*oCommitBuf = CommitBuf;

clean:
	if (!!r) {
		git_buf_free(&CommitBuf);
	}

	if (Signature)
		git_signature_free(Signature);

	if (Tree)
		git_tree_free(Tree);

	return r;
}

int aux_commit_commit_dummy(git_odb *OdbT, git_buf *CommitBuf, git_oid *oCommitOid) {
	int r = 0;

	git_oid CommitOid = {};

	if (!!(r = git_odb_write(&CommitOid, OdbT, CommitBuf->ptr, CommitBuf->size, GIT_OBJ_COMMIT)))
		goto clean;

	if (oCommitOid)
		git_oid_cpy(oCommitOid, &CommitOid);

clean:

	return r;
}

int clnt_commit_ensure_dummy(git_repository *RepositoryT, git_oid *TreeOid, git_oid *oCommitOid) {
	int r = 0;

	git_odb *OdbT = NULL;
	git_buf CommitBuf = {};
	uint32_t Exists = 0;
	git_oid CommitOid = {};

	if (!!(r = git_repository_odb(&OdbT, RepositoryT)))
		goto clean;

	if (!!(r = aux_commit_buffer_dummy(RepositoryT, TreeOid, &CommitBuf)))
		goto clean;

	if (!!(r = aux_commit_buffer_checkexist_dummy(OdbT, &CommitBuf, &Exists, &CommitOid)))
		goto clean;

	if (!Exists) {
		if (!!(r = aux_commit_commit_dummy(OdbT, &CommitBuf, &CommitOid)))
			goto clean;
	}

	if (oCommitOid)
		git_oid_cpy(oCommitOid, &CommitOid);

clean:
	git_buf_free(&CommitBuf);

	if (OdbT)
		git_odb_free(OdbT);

	return r;
}

int clnt_commit_setref(git_repository *RepositoryT, const char *RefName, git_oid *CommitOid) {
	int r = 0;

	git_reference *Reference = NULL;

	const char DummyLogMessage[] = "DummyLogMessage";

	int errC = git_reference_create(&Reference, RepositoryT, RefName, CommitOid, true, DummyLogMessage);
	if (!!errC && errC != GIT_EEXISTS)
		{ r = errC; goto clean; }
	assert(errC == 0 || errC == GIT_EEXISTS);
	/* if we are forcing creation (force=true), existing reference is fine and will be overwritten */

clean:
	if (Reference)
		git_reference_free(Reference);

	return r;
}

int aux_repository_open(const char *RepoOpenPath, git_repository **oRepository) {
	int r = 0;

	git_repository *Repository = NULL;

	if (!!(r = git_repository_open(&Repository, RepoOpenPath)))
		goto clean;

	if (oRepository)
		*oRepository = Repository;

clean:
	if (!!r) {
		if (Repository)
			git_repository_free(Repository);
	}

	return r;
}

int aux_repository_discover_open(const char *RepoDiscoverPath, git_repository **oRepository) {
	int r = 0;

	git_buf RepoPath = {};
	git_repository *Repository = NULL;

	if (!!(r = git_repository_discover(&RepoPath, RepoDiscoverPath, 0, NULL)))
		goto clean;

	if (!!(r = git_repository_open(&Repository, RepoPath.ptr)))
		goto clean;

	if (oRepository)
		*oRepository = Repository;

clean:
	if (!!r) {
		if (Repository)
			git_repository_free(Repository);
	}

	git_buf_free(&RepoPath);

	return r;
}

int stuff(
	const char *RefName, size_t LenRefName,
	const char *RepoOpenPath, size_t LenRepoOpenPath,
	const char *RepoTOpenPath, size_t LenRepoTOpenPath)
{
	int r = 0;

	git_buf RepoPath = {};
	git_repository *Repository = NULL;
	git_repository *RepositoryT = NULL;
	git_oid CommitHeadOid = {};
	git_oid TreeHeadOid = {};
	git_oid CommitHeadOidT = {};
	git_oid TreeHeadOidT = {};

	std::vector<git_oid> Treelist;
	std::vector<git_oid> MissingTreelist;

	std::string SizeBufferTree;
	std::string ObjectBufferTree;

	std::vector<git_oid> MissingBloblist;

	std::string SizeBufferBlob;
	std::string ObjectBufferBlob;

	std::vector<git_oid> WrittenTree;
	std::vector<git_oid> WrittenBlob;

	git_oid LastReverseToposortAkaFirstToposort = {};
	git_oid CreatedCommitOid = {};

	if (!!(r = aux_repository_open(RepoOpenPath, &Repository)))
		goto clean;

	if (!!(r = aux_repository_open(RepoTOpenPath, &RepositoryT)))
		goto clean;

	if (!!(r = serv_latest_commit_tree_oid(Repository, RefName, &CommitHeadOid, &TreeHeadOid)))
		goto clean;

	if (!!(r = clnt_latest_commit_tree_oid(RepositoryT, RefName, &CommitHeadOidT, &TreeHeadOidT)))
		goto clean;

	if (git_oid_cmp(&TreeHeadOidT, &TreeHeadOid) == 0) {
		char buf[GIT_OID_HEXSZ] = {};
		git_oid_fmt(buf, &CommitHeadOid);
		printf("Have latest [%.*s]\n", GIT_OID_HEXSZ, buf);
	}

	if (!!(r = serv_oid_treelist(Repository, &TreeHeadOid, &Treelist)))
		goto clean;

	if (!!(r = clnt_missing_trees(RepositoryT, &Treelist, &MissingTreelist)))
		goto clean;

	if (!!(r = serv_serialize_trees(Repository, &MissingTreelist, &SizeBufferTree, &ObjectBufferTree)))
		goto clean;

	if (!!(r = clnt_missing_blobs(RepositoryT, MissingTreelist.size(), &SizeBufferTree, &ObjectBufferTree, &MissingBloblist)))
		goto clean;

	if (!!(r = serv_serialize_blobs(Repository, &MissingBloblist, &SizeBufferBlob, &ObjectBufferBlob)))
		goto clean;

	if (!!(r = clnt_deserialize_trees(
		RepositoryT,
		(uint8_t *)SizeBufferTree.data(), SizeBufferTree.size(), 0,
		(uint8_t *)ObjectBufferTree.data(), ObjectBufferTree.size(), 0,
		MissingTreelist.size(), &WrittenTree)))
	{
		goto clean;
	}

	if (!!(r = clnt_deserialize_blobs(RepositoryT,
		(uint8_t *)SizeBufferBlob.data(), SizeBufferBlob.size(), 0,
		(uint8_t *)ObjectBufferBlob.data(), ObjectBufferBlob.size(), 0,
		MissingBloblist.size(), &WrittenBlob)))
	{
		goto clean;
	}

	assert(!Treelist.empty());
	git_oid_cpy(&LastReverseToposortAkaFirstToposort, &Treelist[Treelist.size() - 1]);
	if (!!(r = clnt_commit_ensure_dummy(RepositoryT, &LastReverseToposortAkaFirstToposort, &CreatedCommitOid)))
		goto clean;
	if (!!(r = clnt_commit_setref(RepositoryT, RefName, &CreatedCommitOid)))
		goto clean;

clean:
	if (RepositoryT)
		git_repository_free(RepositoryT);
	if (Repository)
		git_repository_free(Repository);

	return r;
}

int gittest_main(int argc, char **argv) {
	int r = 0;

	confmap_t KeyVal;

	std::string ConfRefName;
	std::string ConfRepoOpenPath;
	std::string ConfRepoTOpenPath;

	if (!!(r = aux_gittest_init()))
		goto clean;

	if (!!(r = aux_config_read("../data/", "gittest_config.conf", &KeyVal)))
		goto clean;

	if (!!(r = aux_config_key_ex(KeyVal, "ConfRefNameDUMMYINEXISTENT", &ConfRefName)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(KeyVal, "ConfRepoOpenPathDUMMYINEXISTENT", &ConfRepoOpenPath)))
		GS_GOTO_CLEAN();

	if (!!(r = aux_config_key_ex(KeyVal, "ConfRepoTOpenPathTDUMMYINEXISTENT", &ConfRepoTOpenPath)))
		GS_GOTO_CLEAN();

	if (!!(r = stuff(
		ConfRefName.c_str(), ConfRefName.size(),
		ConfRepoOpenPath.c_str(), ConfRepoOpenPath.size(),
		ConfRepoTOpenPath.c_str(), ConfRepoTOpenPath.size())))
	{
		goto clean;
	}

clean:
	if (!!r)
		assert(!r);

	return EXIT_SUCCESS;
}
