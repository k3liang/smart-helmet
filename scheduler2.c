#include "sensor_interaction.h"
#include "scheduler2.h"
#include <stdio.h>
#include <wiringPi.h>
#include <stdlib.h>

#include <sys/timeb.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include "governor.h"

void (*body[NUMSENSORS]) (SharedVariable*) = {body_button, body_encoder,
    body_twocolor, body_aled, body_buzzer, body_temphumid, body_air,
   body_accel, body_camera, body_lcd};

static char* funcNames[] = {"button", "encoder", "twocolor", "aled",
"buzzer", "temphumid", "air", "accel", "camera", "lcd"};
static unsigned long long times[] = {0,0,0,0,0,0,0,0,0,0};
static unsigned long long prevTime = 0;
static unsigned long long sum = 0;

static unsigned long long idleTime = 0;
static unsigned long long lowTime = 0;
static unsigned long long highTime = 0;

static unsigned long long execTimes[] = { // microseconds
    15, 5, 5, 5, 5, 220000, 15000, 55000, 60000, 60
};

static unsigned long long camHighFreqTime = 30000; //microseconds

static int queue[NUMSENSORS];

static double displayUtil = DISPLAY_UTIL * MAX_UTIL / NUMSENSORS;
static double tuneUtilOff = OFF_TUNE_UTIL * MAX_UTIL / NUMSENSORS;
static double tuneUtilOn = ON_TUNE_UTIL * MAX_UTIL / NUMSENSORS;
static double alarmUtilOff = OFF_ALARM_UTIL * MAX_UTIL / NUMSENSORS;
static double alarmUtilOn = ON_ALARM_UTIL * MAX_UTIL / NUMSENSORS;

unsigned long long get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long curTime = (unsigned long long)tv.tv_sec * 1000 * 1000 + (unsigned long long)tv.tv_usec;
    return curTime;
}

void learn_exectimes(SharedVariable* sv) {
    /*int i;
    unsigned long long currTime = get_current_time_us(); 
    for (i = 0; i < NUMSENSORS; i++) {
        queue[i] = -1;
        sv->alive[i] = 0;
        sv->deadlines[i] = execTimes[i] * NUMSENSORS * 4;
        sv->nextArrive[i] = currTime + sv->deadlines[i];
    }

    sv->deadlines[IAIR] = 1000000; */
    // air quality sensor needs to be really delayed between polls

    /*
    sv->slackUtil = MAXUTIL;
    for (i = 0; i < NUMSENSORS; i++) {
        sv->utils[i] = (double)(execTimes[i]) / (double)(sv->deadlines[i]);
        sv->slackUtil -= sv->utils[i];
    }
    */
    sv->sensorUtil = MAX_UTIL - displayUtil - tuneUtilOff - alarmUtilOff;

    sv->deadlines[ILCD] = (unsigned long long)(execTimes[ILCD] * NUMSENSORS * NUMDISPLAY / (DISPLAY_UTIL * MAX_UTIL));
    sv->deadlines[ISMD] = (unsigned long long)(execTimes[ISMD] * NUMSENSORS * NUMDISPLAY / (DISPLAY_UTIL * MAX_UTIL));

    sv->deadlines[IBUTTON] = (unsigned long long)(execTimes[IBUTTON] * NUMSENSORS * NUMTUNE / (OFF_TUNE_UTIL * MAX_UTIL));
    sv->deadlines[IENCODER] = (unsigned long long)(execTimes[IENCODER] * NUMSENSORS * NUMTUNE / (OFF_TUNE_UTIL * MAX_UTIL));

    sv->deadlines[IALED] = (unsigned long long)(execTimes[IALED] * NUMSENSORS * NUMALARM / (OFF_ALARM_UTIL * MAX_UTIL));
    sv->deadlines[IBUZZER] = (unsigned long long)(execTimes[IBUZZER] * NUMSENSORS * NUMALARM / (OFF_ALARM_UTIL * MAX_UTIL));

    int i;
    for (i = ITEMP; i <= ICAM; i++) {
        sv->deadlines[i] = (unsigned long long)(execTimes[i] * NUMREAD / sv->sensorUtil);
    }

    unsigned long long currTime = get_current_time_us(); 
    for (i = 0; i < NUMSENSORS; i++) {
        queue[i] = -1;
        sv->alive[i] = 0;
        sv->nextArrive[i] = currTime + sv->deadlines[i];
    }

    sv->camHigh = FALSE;
    prevTime = get_current_time_us();
}

void tuneOn(SharedVariable *sv) {
    sv->sensorUtil -= tuneUtilOn - tuneUtilOff;
    sv->deadlines[IBUTTON] = (unsigned long long)(execTimes[IBUTTON] * NUMTUNE / tuneUtilOn);
    sv->deadlines[IENCODER] = (unsigned long long)(execTimes[IENCODER] * NUMTUNE / tuneUtilOn);
}

void tuneOff(SharedVariable *sv) {
    sv->sensorUtil += tuneUtilOn - tuneUtilOff;
    sv->deadlines[IBUTTON] = (unsigned long long)(execTimes[IBUTTON] * NUMTUNE / tuneUtilOff);
    sv->deadlines[IENCODER] = (unsigned long long)(execTimes[IENCODER] * NUMTUNE / tuneUtilOff);
}

void alarmOn(SharedVariable *sv) {
    sv->sensorUtil -= alarmUtilOn - alarmUtilOff;
    sv->deadlines[IALED] = (unsigned long long)(execTimes[IALED] * NUMALARM / alarmUtilOn);
    sv->deadlines[IBUZZER] = (unsigned long long)(execTimes[IBUZZER] * NUMALARM / alarmUtilOn);
}

void alarmOff(SharedVariable *sv) {
    sv->sensorUtil += alarmUtilOn - alarmUtilOff;
    sv->deadlines[IALED] = (unsigned long long)(execTimes[IALED] * NUMALARM / alarmUtilOff);
    sv->deadlines[IBUZZER] = (unsigned long long)(execTimes[IBUZZER] * NUMALARM / alarmUtilOff);
}

void enqueue(SharedVariable* sv, int p) {
    int i, j;
    for (i = 0; i < NUMSENSORS; i++) {
        if (queue[i] == -1) {
            queue[i] = p;
            return;
        }
        if (sv->currDeadline[p] < sv->currDeadline[queue[i]]) {
            for (j = NUMSENSORS-1; j > i; j--) {
                queue[j] = queue[j-1];
            }
            queue[i] = p;
            return;
        }
    }
}

int dequeue() {
    int answer = queue[0];
    int i;
    for (i = 0; i < NUMSENSORS-1; i++) {
        queue[i] = queue[i+1];
    }
    queue[NUMSENSORS-1] = -1;
    return answer;
}

void updateDeadline(SharedVariable* sv, int i) {
    if (i < ITEMP || i > ICAM) {
        return;
    }
    int newIndex = i - ITEMP + 1;
    if (sv->sumDanger <= 0) {
        sv->deadlines[i] = execTimes[i] * NUMREAD / sv->sensorUtil;
        return;
    }
    double danger = sv->relDanger[newIndex];
    if (newIndex == 1) {
        if (sv->relDanger[0] > danger) {
            danger = sv->relDanger[0];
        }
    }

    if (danger <= 0) {
        sv->deadlines[i] = execTimes[i] * (sv->sumDanger + 0.1) / (0.1 * sv->sensorUtil);
        return;
    }
    if (i == ICAM && danger >= RELDANGERTHRES) {
        sv->deadlines[i] = camHighFreqTime * sv->sumDanger / (danger * sv->sensorUtil);
        sv->camHigh = TRUE;
        return;
    }
    sv->deadlines[i] = execTimes[i] * sv->sumDanger / (danger * danger * danger * sv->sensorUtil);
}

void run_task(SharedVariable* sv) {
    unsigned long long currTime = get_current_time_us();
    int i;
    for (i = 0; i < NUMSENSORS; i++) {
        if (sv->nextArrive[i] <= currTime) {
            if (sv->alive[i] == 0) {
                updateDeadline(sv, i);
                sv->alive[i] = 1;
                sv->currDeadline[i] = sv->nextArrive[i] + sv->deadlines[i];
                enqueue(sv, i);
            } else {
                //printf("task %d deadline missed", i);
            }
            sv->nextArrive[i] += sv->deadlines[i];
        }
    }
    if (queue[0] != -1) {
        int p = dequeue();
        unsigned long long maxStart = 0;
        unsigned long long maxEnd = 0;
        if (DEBUG == 1) {
            //printf("%s\n", funcNames[p]);
        }
        if (p == ICAM && sv->camHigh == TRUE) {
            set_by_max_freq();
            maxStart = get_current_time_us();
            (*body[p]) (sv);
            sv->alive[p] = 0;
            sv->camHigh = FALSE;
            maxEnd = get_current_time_us();
            highTime += maxEnd - maxStart;
            set_by_min_freq();
        } else {
            (*body[p]) (sv);
            sv->alive[p] = 0;
        }
        //printf("%llu took this much microseconds for run task %d \n", get_current_time_us() - currTime, p);
        if (DEBUG == 1) {
            unsigned long long endTime = get_current_time_us();
            times[p] += endTime - currTime;
            sum += endTime - currTime;
            lowTime += endTime - currTime;
            if (maxStart > 0) {
                lowTime -= maxEnd - maxStart;
            }

            if (endTime - prevTime >= DPERIOD) {
                int k;
                printf("------------------\n------------------\n");
                for (k = 0; k < NUMSENSORS; k++) {
                    printf("%s %f\n", funcNames[k], ((double)times[k]) / sum);
                    times[k] = 0;
                }
                sum = 0;
                prevTime = endTime;
                printf("------------------\n");
                printf("Low Freq Time: %f\n", lowTime * 1e-6);
                printf("High Freq Time: %f\n", highTime * 1e-6);
                //printf("Idle Time: %f\n", idleTime * 1e-6);
                //printf("Energy consumed: %f\n", ((highTime * 2000.0) + (lowTime * 1200.0) + (idleTime * 50.0)) * 1e-9);
                printf("Energy consumed: %f\n", ((highTime * 2000.0) + (lowTime * 1200.0)) * 1e-9);
            }
        }
    } else {
        //idleTime += get_current_time_us() - currTime;
        lowTime += get_current_time_us() - currTime;
    }
    
}