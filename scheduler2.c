#include "sensor_interaction.h"
#include <stdio.h>
#include <wiringPi.h>
#include <stdlib.h>

#include <sys/timeb.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>

void (*body[NUMSENSORS]) (SharedVariable*) = {body_button, body_encoder,
    body_twocolor, body_aled, body_buzzer, body_temphumid, body_air,
   body_accel, body_lcd, body_camera};

static int queue[NUMSENSORS];

unsigned long long get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long curTime = (unsigned long long)tv.tv_sec * 1000 * 1000 + (unsigned long long)tv.tv_usec;
    return curTime;
}

void learn_exectimes(SharedVariable* sv) {
    int i;
    for (i = 0; i < NUMSENSORS; i++) {
        queue[i] = -1;
        sv->alive[i] = 0;
    }
    delay(1000);
    for (i = 0; i < NUMSENSORS; i++) {
        unsigned int beginTime = millis();
        (*body[i]) (sv);
        unsigned int endTime = millis();
        sv->execTimes[i] = endTime - beginTime;
        sv->deadlines[i] = (endTime - beginTime) * NUMSENSORS * 2;
        delay(200);
    }
    unsigned int currTime = millis();
    for (i = 0; i < NUMSENSORS; i++) {
        sv->nextArrive[i] = currTime + sv->deadlines[i];
    }
    init_shared_variable(sv);
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

void run_task(SharedVariable* sv) {
    unsigned int currTime = millis();
    int i;
    for (i = 0; i < NUMSENSORS; i++) {
        if (sv->nextArrive[i] <= currTime) {
            if (sv->alive[i] == 0) {
                sv->alive[i] = 1;
                sv->currDeadline[i] = sv->nextArrive[i] + sv->deadlines[i];
                enqueue(sv, i);
            } else {
                printf("task %d deadline missed", i);
            }
            sv->nextArrive[i] += sv->deadlines[i];
        }
    }
    if (queue[0] != -1) {
        int p = dequeue();
        (*body[p]) (sv);
        sv->alive[p] = 0;
    }
}