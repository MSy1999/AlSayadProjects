#include "as_protocol.h"
#include <string.h>
static uint16_t r16(const uint8_t*p){return p[0]|((uint16_t)p[1]<<8);}
static uint32_t r32(const uint8_t*p){return r16(p)|((uint32_t)r16(p+2)<<16);}
static void w16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t*p,uint32_t v){w16(p,(uint16_t)v);w16(p+2,(uint16_t)(v>>16));}
bool as_protocol_decode(const uint8_t*p,size_t n,as_command_t*c){
 if(!p||!c||n!=AS_PACKET_SIZE||p[0]!='A'||p[1]!='D'||p[2]!=1||p[3]<1||p[3]>4||r32(p+20))return false;
 *c=(as_command_t){.kind=p[3],.session=r32(p+4),.sequence=r32(p+8),.throttle=r16(p+12)/1000.f,.roll=(int16_t)r16(p+14)/1000.f,.pitch=(int16_t)r16(p+16)/1000.f,.yaw=(int16_t)r16(p+18)/1000.f};
 return c->session&&c->throttle<=1&&c->roll>=-1&&c->roll<=1&&c->pitch>=-1&&c->pitch<=1&&c->yaw>=-1&&c->yaw<=1;
}
void as_protocol_encode(const as_command_t*c,uint8_t p[AS_PACKET_SIZE]){memset(p,0,AS_PACKET_SIZE);p[0]='A';p[1]='D';p[2]=1;p[3]=c->kind;w32(p+4,c->session);w32(p+8,c->sequence);w16(p+12,(uint16_t)(c->throttle*1000));w16(p+14,(int16_t)(c->roll*1000));w16(p+16,(int16_t)(c->pitch*1000));w16(p+18,(int16_t)(c->yaw*1000));}
