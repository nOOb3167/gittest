#ifndef _NET2_AUX_H_
#define _NET2_AUX_H_

#include <stdint.h>

#include <mutex>
#include <condition_variable>

#include <gittest/misc.h>
#include <gittest/net2_surrogate.h>

/** @sa
       ::gs_packet_create
	   ::gs_packet_release
*/
struct GsPacket {
	struct GsPacketSurrogate mPacket;

	uint8_t *data;
	size_t   dataLength;
};

/** manual-init struct
    value struct

	@sa
	   ::gs_packet_with_offset_get_veclen
*/
struct GsPacketWithOffset {
	struct GsPacket *mPacket;
	uint32_t  mOffsetSize;
	uint32_t  mOffsetObject;
};

/** @sa
       ::gs_ctrl_con_create
	   ::gs_ctrl_con_destroy
	   ::gs_ctrl_con_signal_exited
	   ::gs_ctrl_con_wait_exited
	   ::gs_ctrl_con_get_num_workers
*/
struct GsCtrlCon
{
	uint32_t mNumNtwks;
	uint32_t mNumWorkers;
	uint32_t mExitedSignalLeft;
	sp<std::mutex> mCtrlConMutex;
	sp<std::condition_variable> mCtrlConCondExited;
};

int gs_packet_create(
	struct GsPacket **oPacket,
	struct GsPacketSurrogate *valPacketSurrogate);
int gs_packet_release(
	struct GsPacket *ioPacket);
int gs_packet_with_offset_get_veclen(
	struct GsPacketWithOffset *PacketWithOffset,
	uint32_t *oVecLen);

int gs_ctrl_con_create(
	uint32_t NumNtwks,
	uint32_t NumWorkers,
	struct GsCtrlCon **oCtrlCon);
int gs_ctrl_con_destroy(struct GsCtrlCon *CtrlCon);
int gs_ctrl_con_signal_exited(struct GsCtrlCon *CtrlCon);
int gs_ctrl_con_wait_exited(struct GsCtrlCon *CtrlCon);
int gs_ctrl_con_get_num_workers(struct GsCtrlCon *CtrlCon, uint32_t *oNumWorkers);

#endif /* _NET2_AUX_H_ */
