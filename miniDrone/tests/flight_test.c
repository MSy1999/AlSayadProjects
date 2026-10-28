
#include "as_flight.h"
#include "as_protocol.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
 as_flight_t f;as_flight_init(&f);as_sample_t level={.accel={0,0,-9.80665f}};float m[4];
 as_command_t c={.kind=AS_INPUT,.session=42,.sequence=1};uint8_t packet[AS_PACKET_SIZE];as_protocol_encode(&c,packet);as_command_t d;assert(as_protocol_decode(packet,sizeof(packet),&d));assert(!as_protocol_decode(packet,23,&d));packet[20]=1;assert(!as_protocol_decode(packet,24,&d));
 as_flight_step(&f,&level,.004f,1000,true,m);assert(f.estimated);assert(as_flight_command(&f,&c,1,2000,true));assert(!as_flight_command(&f,&c,1,2001,true));
 c.kind=AS_ARM;c.sequence++;assert(as_flight_command(&f,&c,1,3000,true));assert(f.state==AS_ARMED);
 c.kind=AS_INPUT;c.sequence++;c.throttle=.4f;assert(!as_flight_command(&f,&c,2,4000,true));assert(as_flight_command(&f,&c,1,4000,true));
 as_flight_step(&f,&level,.004f,8000,true,m);for(int i=0;i<4;i++)assert(m[i]>.25f&&m[i]<.27f);
 as_flight_step(&f,&level,.004f,255001,true,m);assert(f.state==AS_FAULT);for(int i=0;i<4;i++)assert(m[i]==0);
 as_flight_init(&f);c.kind=AS_ARM;c.sequence=1;c.throttle=0;assert(!as_flight_command(&f,&c,1,1000,true));
 as_mix(.3f,.05f,0,0,m);assert(m[0]>m[1]&&m[3]>m[2]);as_mix(.3f,0,.05f,0,m);assert(m[0]>m[3]&&m[1]>m[2]);
 as_flight_init(&f);as_flight_step(&f,&level,.02f,1,true,m);assert(f.state==AS_FAULT);
 as_flight_init(&f);level.gyro[0]=NAN;as_flight_step(&f,&level,.004f,1,true,m);assert(f.state==AS_FAULT);
 puts("flight/protocol tests passed");return 0;
}
