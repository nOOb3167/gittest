#include <vector>

#include <git2.h>

#include <gittest/misc.h>
#include <gittest/frame.h>

#include <gittest/bypart.h>

int gs_strided_for_struct_member(
	uint8_t *DataStart, uint32_t DataOffset, uint32_t OffsetOfMember,
	uint32_t EltNum, uint32_t EltSize, uint32_t EltStride,
	GsStrided *oStrided)
{
	int r = 0;

	uint32_t DataOffsetPlusOffset = DataOffset + OffsetOfMember;

	GsStrided Strided = {
		DataStart,
		DataOffsetPlusOffset,
		EltNum,
		EltSize,
		EltStride,
	};

	uint32_t DataLength = EltNum * EltSize;

	if (EltSize > EltStride || DataOffset + EltStride * EltNum > DataLength)
		GS_ERR_CLEAN(1);

	if (oStrided)
		*oStrided = Strided;

clean:

	return r;
}

int gs_bypart_cb_String(void *ctx, const char *d, int64_t l) {
	int r = 0;

	git_oid Oid = {};
	GS_BYPART_DATA_VAR_CTX_NONUCF(String, Data, ctx);

	Data->m0Buffer->append(d, l);

clean:

	return r;
}

int gs_bysize_cb_String(void *ctx, int64_t l, uint8_t **od) {
	int r = 0;

	GS_BYPART_DATA_VAR_CTX_NONUCF(String, Data, ctx);

	Data->m0Buffer->resize(l);

	if (od)
		*od = (uint8_t *)Data->m0Buffer->data();

clean:

	return r;
}

int gs_bypart_cb_OidVector(void *ctx, const char *d, int64_t l) {
	int r = 0;

	git_oid Oid = {};
	GS_BYPART_DATA_VAR_CTX_NONUCF(OidVector, Data, ctx);

	if (!!(r = aux_frame_read_oid((uint8_t *)d, (uint32_t)l, 0, NULL, (uint8_t *)Oid.id, GIT_OID_RAWSZ)))
		GS_GOTO_CLEAN();

	Data->m0OidVec->push_back(Oid);

clean:

	return r;
}

int gs_strided_for_oid_vec_cpp(std::vector<git_oid> *OidVec, GsStrided *oStrided) {
	int r = 0;

	uint8_t *DataStart = (uint8_t *)OidVec->data();
	uint32_t DataOffset = 0;
	uint32_t DataOffsetPlusOffset = DataOffset + offsetof(git_oid, id);
	uint32_t EltNum = OidVec->size();
	uint32_t EltSize = sizeof *OidVec->data();
	uint32_t EltStride = GIT_OID_RAWSZ;

	GsStrided Strided = {
		DataStart,
		DataOffsetPlusOffset,
		EltNum,
		EltSize,
		EltStride,
	};

	uint32_t DataLength = OidVec->size() * sizeof *OidVec->data();

	if (EltSize > EltStride || DataOffset + EltStride * EltNum > DataLength)
		GS_ERR_CLEAN(1);

	if (oStrided)
		*oStrided = Strided;

clean:

	return r;
}
