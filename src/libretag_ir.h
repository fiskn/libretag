#include <IRremoteESP8266.h>
#include <stdint.h>
#include <algorithm>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRtimer.h"
#include "IRutils.h"

void initIR();
void sendIR(uint64_t data, uint16_t nbits);
uint32_t buildShotIR(uint16_t type, uint16_t playerID, uint16_t teamID, uint16_t gunType);
