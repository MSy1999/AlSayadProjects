#pragma once
#include <stddef.h>
#include "as_flight.h"
#define AS_PACKET_SIZE 24
/* LE: magic AD, version 1, kind; session u32, sequence u32;
 * throttle u16 0..1000; roll/pitch/yaw i16 -1000..1000; reserved u32=0. */
bool as_protocol_decode(const uint8_t *bytes,size_t size,as_command_t *command);
void as_protocol_encode(const as_command_t *command,uint8_t bytes[AS_PACKET_SIZE]);
