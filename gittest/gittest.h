#ifndef _GITTEST_H_
#define _GITTEST_H_

#include <cstdint>

#include <vector>
#include <map>
#include <set>
#include <list>
#include <string>

#include <git2.h>

struct oid_comparator_t {
	bool operator()(const git_oid * const &a, const git_oid * const &b) {
		return git_oid_cmp(a, b) < 0;
	}
};

typedef ::std::map<::std::string, ::std::string> confmap_t;

typedef ::std::set<const git_oid *, oid_comparator_t> toposet_t;
typedef ::std::list<git_tree *> topolist_t;

int tree_toposort_visit(git_repository *Repository, toposet_t *MarkSet, topolist_t *NodeList, git_tree *Tree);
int tree_toposort(git_repository *Repository, git_tree *Tree, topolist_t *oNodeList);

int aux_gittest_init();
int aux_config_read(const char *ExpectedLocation, const char *ExpectedName, std::map<std::string, std::string> *oKeyVal);
const char * aux_config_key(const confmap_t &KeyVal, const char *Key);
void aux_uint32_to_LE(uint32_t a, char *oBuf, size_t bufsize);
void aux_LE_to_uint32(uint32_t *oA, const char *buf, size_t bufsize);
void aux_topolist_print(const topolist_t &NodeListTopo);
int aux_oid_latest_commit_tree(git_repository *Repository, const char *RefName, git_oid *oCommitHeadOid, git_oid *oTreeHeadOid);
int serv_latest_commit_tree_oid(git_repository *Repository, const char *RefName, git_oid *oCommitHeadOid, git_oid *oTreeHeadOid);
int clnt_latest_commit_tree_oid(git_repository *RepositoryT, const char *RefName, git_oid *oCommitHeadOid, git_oid *oTreeHeadOid);
int serv_oid_treelist(git_repository *Repository, git_oid *TreeOid, std::vector<git_oid> *oOutput);
int aux_serialize_objects(
	git_repository *Repository, std::vector<git_oid> *ObjectOid, git_otype ObjectWantedType,
	std::string *oSizeBuffer, std::string *oObjectBuffer);
int serv_serialize_trees(git_repository *Repository, std::vector<git_oid> *TreeOid, std::string *oSizeBuffer, std::string *oTreeBuffer);
int serv_serialize_blobs(git_repository *Repository, std::vector<git_oid> *BlobOid, std::string *oSizeBuffer, std::string *oBlobBuffer);
int aux_deserialize_sizebuffer(std::string *SizeBuffer, std::vector<uint32_t> *oSizeVector, size_t *oCumulativeSize);
int aux_deserialize_objects_odb(
	git_odb *OdbT, std::string *SizeBuffer, std::string *ObjectBuffer, git_otype WrittenObjectType,
	std::vector<git_oid> *oWrittenObjectOid);
int aux_deserialize_objects(
	git_repository *RepositoryT, std::string *SizeBuffer, std::string *ObjectBuffer, git_otype WrittenObjectType,
	std::vector<git_oid> *oWrittenObjectOid);
int aux_clnt_deserialize_trees(git_odb *OdbT, std::string *SizeBuffer, std::string *TreeBuffer, std::vector<git_oid> *oDeserializedTree);
int clnt_deserialize_trees(git_repository *RepositoryT, std::string *SizeBuffer, std::string *TreeBuffer, std::vector<git_oid> *oDeserializedTree);
int clnt_deserialize_blobs(git_repository *RepositoryT, std::string *SizeBuffer, std::string *BlobBuffer, std::vector<git_oid> *oDeserializedBlob);
int clnt_missing_trees(git_repository *RepositoryT, std::vector<git_oid> *Treelist, std::vector<git_oid> *oMissingTreeList);
int aux_memory_repository_new(git_repository **oRepositoryMemory);
int aux_clnt_dual_lookup_expect_missing(
	git_repository *RepositoryMemory, git_repository *RepositoryT,
	git_oid *TreeOid,
	git_tree **oTreeMem, git_tree **oTreeT);
int clnt_missing_blobs(git_repository *RepositoryT, std::string *SizeBuffer, std::string *TreeBuffer, std::vector<git_oid> *oMissingBloblist);
int aux_commit_buffer_checkexist_dummy(git_odb *OdbT, git_buf *CommitBuf, uint32_t *oExists, git_oid *oCommitOid);
int aux_commit_buffer_dummy(git_repository *RepositoryT, git_oid *TreeOid, git_buf *oCommitBuf);
int aux_commit_commit_dummy(git_odb *OdbT, git_buf *CommitBuf, git_oid *oCommitOid);
int clnt_commit_ensure_dummy(git_repository *RepositoryT, git_oid *TreeOid, git_oid *oCommitOid);
int clnt_commit_setref(git_repository *RepositoryT, const char *RefName, git_oid *CommitOid);
int aux_repository_open(const char *RepoOpenPath, git_repository **oRepository);
int aux_repository_discover_open(const char *RepoDiscoverPath, git_repository **oRepository);

int stuff(const confmap_t &KeyVal);
int gittest_main(int argc, char **argv);

#endif /* _GITTEST_H_ */
