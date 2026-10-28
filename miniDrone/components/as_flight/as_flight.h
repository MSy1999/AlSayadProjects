/** @file as_flight.h @brief Platform-independent command, safety and flight control. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { AS_INPUT=1, AS_ARM=2, AS_DISARM=3, AS_STOP=4 } as_command_kind_t;
typedef struct { as_command_kind_t kind; uint32_t session,sequence; float throttle,roll,pitch,yaw; } as_command_t;
typedef enum { AS_DISARMED, AS_ARMED, AS_FAULT } as_state_t;
typedef struct { float gyro[3],accel[3]; } as_sample_t;
typedef struct { float kp,ki,kd,integral,previous,derivative; } as_pid_t;
typedef struct {
    as_state_t state; uint32_t owner,session,sequence; bool have_sequence,have_input,estimated;
    int64_t received_us; as_command_t input; float roll,pitch; as_pid_t rate[3];
} as_flight_t;
void as_flight_init(as_flight_t *f);
bool as_flight_command(as_flight_t *f,const as_command_t *c,uint32_t source,int64_t now,bool healthy);
void as_flight_fault(as_flight_t *f);
/* Right-handed body axes: X forward, Y right, Z down; gyro rad/s, accel m/s^2.
 * Level rest specific force = [0,0,-9.80665]. Outputs FL,FR,RR,RL; CW,CCW,CW,CCW. */
void as_flight_step(as_flight_t *f,const as_sample_t *sample,float dt,int64_t now,bool healthy,float motors[4]);
void as_mix(float throttle,float roll,float pitch,float yaw,float motors[4]);
