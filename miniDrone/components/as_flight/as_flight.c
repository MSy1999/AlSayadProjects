#include "as_flight.h"
#include <math.h>
#include <string.h>
static float limit(float x,float lo,float hi){return fminf(hi,fmaxf(lo,x));}
void as_flight_init(as_flight_t *f){memset(f,0,sizeof(*f)); for(int i=0;i<3;i++) f->rate[i]=(as_pid_t){.kp=.08f,.ki=.02f,.kd=.001f};}
void as_flight_fault(as_flight_t *f){f->state=AS_FAULT;f->have_input=false;}
bool as_flight_command(as_flight_t *f,const as_command_t *c,uint32_t source,int64_t now,bool healthy){
    if(!f || !c || !source || !c->session || c->kind<AS_INPUT || c->kind>AS_STOP) return false;
    if(!isfinite(c->throttle)||!isfinite(c->roll)||!isfinite(c->pitch)||!isfinite(c->yaw)||c->throttle<0||c->throttle>1||fabsf(c->roll)>1||fabsf(c->pitch)>1||fabsf(c->yaw)>1) return false;
    if(f->state==AS_FAULT) return false;
    if(f->owner && (source!=f->owner || c->session!=f->session)) {
        if(f->state==AS_ARMED || now-f->received_us<500000) return false;
        f->owner=0;f->have_sequence=false;f->have_input=false;
    }
    if(f->have_sequence){uint32_t delta=c->sequence-f->sequence;if(!delta||delta>=0x80000000u)return false;}
    if(c->kind==AS_ARM && (f->state!=AS_DISARMED||!healthy||!f->estimated||!f->have_input||now-f->received_us>250000||f->input.throttle>.02f||c->throttle>.02f||fabsf(f->roll)>.35f||fabsf(f->pitch)>.35f))return false;
    f->owner=source;f->session=c->session;f->sequence=c->sequence;f->have_sequence=true;
    if(c->kind==AS_STOP){as_flight_fault(f);return true;}
    if(c->kind==AS_DISARM){f->state=AS_DISARMED;f->have_input=false;}
    if(c->kind==AS_ARM){f->state=AS_ARMED;for(int i=0;i<3;i++){f->rate[i].integral=0;f->rate[i].previous=0;f->rate[i].derivative=0;}}
    if(c->kind==AS_INPUT){f->input=*c;f->received_us=now;f->have_input=true;}
    return true;
}
void as_mix(float t,float r,float p,float y,float m[4]){
    m[0]=t+r+p-y; m[1]=t-r+p+y; m[2]=t-r-p-y; m[3]=t+r-p+y;
    for(int i=0;i<4;i++)m[i]=limit(m[i],0,.65f);
}
void as_flight_step(as_flight_t *f,const as_sample_t *s,float dt,int64_t now,bool healthy,float m[4]){
    memset(m,0,4*sizeof(float));
    if(!healthy||!s||!isfinite(dt)||dt<.001f||dt>.012f){as_flight_fault(f);return;}
    for(int i=0;i<3;i++)if(!isfinite(s->gyro[i])||!isfinite(s->accel[i])){as_flight_fault(f);return;}
    if(f->state==AS_ARMED && (!f->have_input||now-f->received_us>250000)){as_flight_fault(f);return;}
    float norm=sqrtf(s->accel[0]*s->accel[0]+s->accel[1]*s->accel[1]+s->accel[2]*s->accel[2]);
    bool gravity=norm>7.0f&&norm<12.5f;
    float ar=atan2f(-s->accel[1],-s->accel[2]);
    float ap=atan2f(s->accel[0],hypotf(s->accel[1],s->accel[2]));
    if(!f->estimated){if(!gravity)return;f->roll=ar;f->pitch=ap;f->estimated=true;}
    else {
        /* Euler kinematics; yaw coupling included. Flight restricted to modest tilt. */
        float sr=sinf(f->roll),cr=cosf(f->roll),tp=tanf(f->pitch);
        f->roll+=dt*(s->gyro[0]+sr*tp*s->gyro[1]+cr*tp*s->gyro[2]);
        f->pitch+=dt*(cr*s->gyro[1]-sr*s->gyro[2]);
        if(gravity){float a=.5f/(.5f+dt);f->roll=a*f->roll+(1-a)*ar;f->pitch=a*f->pitch+(1-a)*ap;}
    }
    if(fabsf(f->roll)>1.05f||fabsf(f->pitch)>1.05f){as_flight_fault(f);return;}
    if(f->state!=AS_ARMED || f->input.throttle<.03f){for(int i=0;i<3;i++){f->rate[i].integral=0;f->rate[i].previous=s->gyro[i];f->rate[i].derivative=0;}return;}
    float desired[3]={4*(f->input.roll*.35f-f->roll),4*(f->input.pitch*.35f-f->pitch),f->input.yaw*1.5f},u[3];
    for(int i=0;i<3;i++){
        as_pid_t *p=&f->rate[i];float e=limit(desired[i],-2,2)-s->gyro[i];
        p->derivative+=.2f*((s->gyro[i]-p->previous)/dt-p->derivative);p->previous=s->gyro[i];
        float raw=p->kp*e+p->integral-p->kd*p->derivative;
        if(fabsf(raw)<.20f && f->input.throttle>.15f && f->input.throttle<.5f)p->integral=limit(p->integral+p->ki*e*dt,-.10f,.10f);
        u[i]=limit(raw,-.20f,.20f);
    }
    as_mix(f->input.throttle*.65f,u[0],u[1],u[2],m);
}
