#include <gittest/frame.h>

#include <gittest/net2_aux.h>

int gs_packet_create(
	struct GsPacket **oPacket,
	struct GsPacketSurrogate *valPacketSurrogate)
{
	struct GsPacket *Packet = new GsPacket();
	Packet->mPacket = *valPacketSurrogate;
	Packet->data = valPacketSurrogate->mPacket->data;
	Packet->dataLength = valPacketSurrogate->mPacket->dataLength;

	if (oPacket)
		*oPacket = Packet;

	return 0;
}

int gs_packet_release(
	struct GsPacket *ioPacket)
{
	int r = 0;

	if (ioPacket->mPacket.mPacket) {
		enet_packet_destroy(ioPacket->mPacket.mPacket);
		ioPacket->mPacket.mPacket = NULL;
		ioPacket->data = NULL;
		ioPacket->dataLength = 0;
	}

clean:

	return r;
}

int gs_packet_with_offset_get_veclen(
	struct GsPacketWithOffset *PacketWithOffset,
	uint32_t *oVecLen)
{
	GS_ASSERT(GS_FRAME_SIZE_LEN == sizeof(uint32_t));
	if (PacketWithOffset->mOffsetObject < PacketWithOffset->mOffsetSize ||
		(PacketWithOffset->mOffsetObject - PacketWithOffset->mOffsetSize) % GS_FRAME_SIZE_LEN != 0)
		return 1;
	if (oVecLen)
		*oVecLen = (PacketWithOffset->mOffsetObject - PacketWithOffset->mOffsetSize) / GS_FRAME_SIZE_LEN;
	return 0;
}

int gs_ctrl_con_create(
	uint32_t NumNtwks,
	uint32_t NumWorkers,
	struct GsCtrlCon **oCtrlCon)
{
	struct GsCtrlCon *CtrlCon = new GsCtrlCon();

	CtrlCon->mNumNtwks = NumNtwks;
	CtrlCon->mNumWorkers = NumWorkers;
	CtrlCon->mExitedSignalLeft = NumNtwks + NumWorkers;
	CtrlCon->mCtrlConMutex = sp<std::mutex>(new std::mutex);
	CtrlCon->mCtrlConCondExited = sp<std::condition_variable>(new std::condition_variable);

	if (oCtrlCon)
		*oCtrlCon = CtrlCon;

	return 0;
}

int gs_ctrl_con_destroy(struct GsCtrlCon *CtrlCon)
{
	GS_DELETE(&CtrlCon, GsCtrlCon);
	return 0;
}

int gs_ctrl_con_signal_exited(struct GsCtrlCon *CtrlCon)
{
	bool WantNotify = false;
	{
		std::unique_lock<std::mutex> lock(*CtrlCon->mCtrlConMutex);
		if (CtrlCon->mExitedSignalLeft)
			CtrlCon->mExitedSignalLeft -= 1;
		if (!CtrlCon->mExitedSignalLeft)
			WantNotify = true;
	}
	if (WantNotify)
		CtrlCon->mCtrlConCondExited->notify_all();
	return 0;
}

int gs_ctrl_con_wait_exited(struct GsCtrlCon *CtrlCon)
{
	{
		std::unique_lock<std::mutex> lock(*CtrlCon->mCtrlConMutex);
		CtrlCon->mCtrlConCondExited->wait(lock, [&]() { return ! CtrlCon->mExitedSignalLeft; });
	}
	return 0;
}

int gs_ctrl_con_get_num_workers(struct GsCtrlCon *CtrlCon, uint32_t *oNumWorkers)
{
	{
		std::unique_lock<std::mutex> lock(*CtrlCon->mCtrlConMutex);
		*oNumWorkers = CtrlCon->mNumWorkers;
	}
	return 0;
}
