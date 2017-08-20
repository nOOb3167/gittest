#include <vector>

#include <gittest/misc.h>
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
