#ifndef _SCHEDULER_BODY_
#define _SCHEDULER_BODY_

#include <stdint.h>

#define NUMDISPLAY 2
#define NUMREAD 4
#define NUMTUNE 2
#define NUMALARM 2
//body_button, body_encoder,body_twocolor, body_aled, body_buzzer, body_temphumid, body_air, body_accel, body_camera, body_lcd
#define IBUTTON 0
#define IENCODER 1
#define ISMD 2
#define IALED 3
#define IBUZZER 4
#define ITEMP 5
#define IAIR 6
#define IACCEL 7
#define ICAM 8
#define ILCD 9

#define MAX_UTIL 0.9
#define ON_TUNE_UTIL 4.0
#define OFF_TUNE_UTIL 0.25
#define ON_ALARM_UTIL 1.0
#define OFF_ALARM_UTIL 0.25 
#define DISPLAY_UTIL 0.25

unsigned long long get_current_time_us();

void learn_exectimes(SharedVariable* sv);
void run_task(SharedVariable* sv);

void tuneOn(SharedVariable *sv);
void tuneOff(SharedVariable *sv);

void alarmOn(SharedVariable *sv);
void alarmOff(SharedVariable *sv);

#endif