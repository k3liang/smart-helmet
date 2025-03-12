
#ifndef _ASSIGNMENT_BODY_
#define _ASSIGNMENT_BODY_

#include <stdint.h>
#include "read_serial.h"
#include <Python.h>

// Macros
#define TURN_ON(pin) digitalWrite(pin, 1)
#define TURN_OFF(pin) digitalWrite(pin, 0)

#define READ(pin) digitalRead(pin)
#define WRITE(pin, x) digitalWrite(pin, x)

// Constants
#define LOW 0
#define HIGH 1

#define CLOCKWISE 0
#define COUNTER_CLOCKWISE 1

//#define FALSE 0
//#define TRUE 1

#define LOWBOUND 0
#define HIGHBOUND 1
#define NUMVALUES 2

#define TEMP 0
#define HUMID 1
#define AIR 2
#define ACCEL 3
#define FACE 4
#define NUMINPUTS 5

#define NUMSENSORS 10

#define SENSORFILE "sensor_values.txt"

#define SAFE 1
#define DANGER 0

#define TUNINGDEBOUNCE 5000
#define DANGERCOOLDOWN_MAX 10000
#define DANGERTOCRITICAL 10000

#define STEPFACTOR 100

#define FACENUM 5

#define ACCELPERIOD 30000

#define CHANGETHRES 0.08
#define RELDANGERTHRES 0.9

#define MAXCOLOR 255

#define PIN_BUTTON 0
#define PIN_ROTARY_CLK 5
#define PIN_ROTARY_DT 6

//#define PIN_DIP_RED 8
//#define PIN_DIP_GRN 9
#define PIN_SMD_RED 28
#define PIN_SMD_GRN 27
#define PIN_SMD_BLU 29
#define PIN_ALED 12

#define PIN_BUZZER 13

// Shared structure
typedef struct shared_variable {
    int bProgramExit; // Once set to 1, the program will terminate.
    int rotation; // determined by rotary encoder
    int lastClk; // to figure out rotation
    unsigned int lastBeep; // timing of the buzzer
    int lastPress; // prevent button hold-presses from glitching
    double info[NUMINPUTS][NUMVALUES];
    double stable[NUMINPUTS]; // "stable" level; we assume an average of bounds
    double relDanger[NUMINPUTS]; // relative danger
    double prevRelDanger[NUMINPUTS];
    double temp;
    double humid;
    double air;
    double accel;
    double face;
    double faces[FACENUM];
    int faceIndex;
    double faceSum;
    unsigned int lastAccel;
    double accelSum;
    char prevlcdMsg[MAX_LINE_LENGTH];
    char lcdMsg[MAX_LINE_LENGTH];
    int safety;
    unsigned int dangerCooldown;
    unsigned int lastDanger;
    int tuning;
    unsigned int lastTune;
    int tuningIndex;
    int fd;
    PyObject* pyObjects[4];
    float eye_ratio;

    unsigned int deadlines[NUMSENSORS];
    unsigned int execTimes[NUMSENSORS];
    unsigned int nextArrive[NUMSENSORS];
    unsigned int currDeadline[NUMSENSORS];
    int alive[NUMSENSORS];
} SharedVariable;

int init_boundaries(SharedVariable* sv);
void init_shared_variable(SharedVariable* sv);
void init_sensors(SharedVariable* sv);

void body_button(SharedVariable* sv);     
void body_encoder(SharedVariable* sv);    
void body_twocolor(SharedVariable* sv);   
void body_aled(SharedVariable* sv);      
void body_buzzer(SharedVariable* sv);   

void body_temphumid(SharedVariable* sv); 
void body_air(SharedVariable* sv);
void body_accel(SharedVariable* sv);
void body_lcd(SharedVariable* sv);
void body_camera(SharedVariable* sv);

void saveCalib(SharedVariable* sv);
void clean_sensors(SharedVariable* sv);

int init_python(SharedVariable* sv);
void clean_python(SharedVariable* sv);

#endif
