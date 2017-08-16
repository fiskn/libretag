#include "libretag_ir.h"

/*IRsend irsend(16);

#define LT_TICK                     560U
#define LT_HDR_MARK_TICKS            16U
#define LT_HDR_MARK                 (LT_HDR_MARK_TICKS * LT_TICK)
#define LT_HDR_SPACE_TICKS            8U
#define LT_HDR_SPACE                (LT_HDR_SPACE_TICKS * LT_TICK)
#define LT_BIT_MARK_TICKS             1U
#define LT_BIT_MARK                 (LT_BIT_MARK_TICKS * LT_TICK)
#define LT_ONE_SPACE_TICKS            3U
#define LT_ONE_SPACE                (LT_TICK * LT_ONE_SPACE_TICKS)
#define LT_ZERO_SPACE_TICKS           1U
#define LT_ZERO_SPACE               (LT_TICK * LT_ZERO_SPACE_TICKS)
#define LT_MIN_COMMAND_LENGTH       (20 * LT_TICK)
#define LT_MIN_GAP                  (LT_TICK*20)

uint8_t calcIRChecksum(uint32_t data) {
  return(((data >> 12) + ((data >> 8) & 0xF) + ((data >> 4) & 0xF) +
         (data & 0xF)) & 0xF);
}

uint32_t buildShotIR(uint16_t type, uint16_t playerID, uint16_t teamID, uint16_t gunType) {
  return (type << 20) + (playerID << 8) + (teamID << 4) + gunType;
}

void sendIR(uint64_t data, uint16_t nbits){
  calcIRChecksum(data);
  irsend.enableIROut(38, 33);
  IRtimer usecs = IRtimer();
  // Header
  irsend.mark(LT_HDR_MARK);
  irsend.space(LT_HDR_SPACE);
  irsend.sendData(LT_BIT_MARK, LT_ONE_SPACE, LT_BIT_MARK, LT_ZERO_SPACE, data, nbits, true);
  irsend.mark(LT_BIT_MARK);
  // Gap to next command.
  irsend.space(std::max(LT_MIN_GAP, LT_MIN_COMMAND_LENGTH - usecs.elapsed()));
}

void sendIRBullet(uint16_t playerID, uint16_t teamID, uint16_t gunType) {
  uint32_t data=buildShotIR(0,playerID,teamID,gunType);
  sendIR(data,22);
}*/
