#include "sensor_interaction.h"
#include "read_serial.h"
#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wiringSerial.h>

static char names[NUMINPUTS][16] = {"TEMP", "HUMID", "AIRQ", "ACCEL", "FACE"};

int readLine(SharedVariable* sv, int index, FILE* file) {
    char line[128];

    if (fgets(line, sizeof(line), file) == NULL) {
        return 1;
    }

    char* token;
    token = strtok(line, " ");
    token = strtok(NULL, " ");

    int i;
    for (i = 0; i < NUMVALUES; i++) {
        if (token == NULL) {
            return 1;
        }
        char* endptr;
        double value = strtod(token, &endptr);
        if (endptr == token) {
            return 1;
        }

        sv->info[index][i] = value;
        token = strtok(NULL, " \n");
    }

    return 0;
}

void saveCalib(SharedVariable* sv) {
    FILE* file;
    file = fopen(SENSORFILE, "w");
    if (file == NULL) {
        printf("ERROR: cannot write to sensor data file\n");
        return;
    }

    int i;
    for (i = 0; i < NUMINPUTS; i++) {
        fprintf(file, "%s: %f %f\n", names[i], sv->info[i][LOWBOUND], sv->info[i][HIGHBOUND]);
    }

    fclose(file);
}

int init_boundaries(SharedVariable* sv) {
    FILE* file;

    file = fopen(SENSORFILE, "r"); 
    if (file == NULL) {
        printf("ERROR: cannot open sensor data file\n");
        return 1;
    }

    int i;
    for (i = 0; i < NUMINPUTS; i++) {
        if (readLine(sv, i, file) != 0) {
            printf("ERROR: unable to parse sensor data file\n");
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

int init_python(SharedVariable* sv) {
    Py_Initialize();
    PyEval_InitThreads();
    PyRun_SimpleString("import sys; sys.path.append('.')");

    PyObject *pName, *pModule, *pInit, *pDetect, *pCleanup;

    pName = PyUnicode_FromString("eye_detector");
    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (!pModule) {
        PyErr_Print();
        printf("Failed to load Python module\n");
        return 1;
    }

    pInit = PyObject_GetAttrString(pModule, "initialize");
    pDetect = PyObject_GetAttrString(pModule, "detect_drowsiness");
    pCleanup = PyObject_GetAttrString(pModule, "cleanup");

    if (pInit && PyCallable_Check(pInit)) {
        PyGILState_STATE gstate = PyGILState_Ensure();
        
        PyObject *result = PyObject_CallObject(pInit, NULL);
        if (result == NULL) {
            PyErr_Print();
            printf("Python initialization failed.\n");
            return 1;
        } else {
            Py_DECREF(result);
        }

        PyGILState_Release(gstate);
    }

    sv->pyObjects[0] = pModule;
    sv->pyObjects[1] = pInit;
    sv->pyObjects[2] = pDetect;
    sv->pyObjects[3] = pCleanup;
    printf("Python initialized\n");
    PyEval_SaveThread();
    return 0;
}

void clean_python(SharedVariable* sv) {
    printf("running finalize");
    PyGILState_STATE gstate = PyGILState_Ensure();
    Py_XDECREF(sv->pyObjects[1]);
    Py_XDECREF(sv->pyObjects[2]);
    Py_XDECREF(sv->pyObjects[3]);
    Py_DECREF(sv->pyObjects[0]);
    PyGILState_Release(gstate);
    Py_Finalize();
}

void init_shared_variable(SharedVariable* sv) {
    sv->bProgramExit = 0;
    sv->rotation = CLOCKWISE;
    sv->lastClk = LOW;
    sv->lastBeep = 0;
    sv->lastPress = HIGH;
    sv->temp = (sv->info[TEMP][LOW]+sv->info[TEMP][HIGH]) / 2.0;
    sv->humid = (sv->info[HUMID][LOW]+sv->info[HUMID][HIGH]) / 2.0;
    sv->air = (sv->info[AIR][LOW]+sv->info[AIR][HIGH]) / 2.0;
    sv->accel = (sv->info[ACCEL][LOW]+sv->info[ACCEL][HIGH]) / 2.0;
    sv->face = (sv->info[FACE][LOW]+sv->info[FACE][HIGH]) / 2.0;
    
    int i;
    for (i = 0; i < FACENUM; i++) {
        sv->faces[i] = 0.5;
    }
    sv->faceIndex = 0;
    sv->faceSum = 0.5*FACENUM;

    sv->lastAccel = 0;
    sv->accelSum = 0.0;

    for (i = 0; i < NUMINPUTS; i++) {
        sv->stable[i] = (sv->info[i][LOW]+sv->info[i][HIGH]) / 2.0;
        sv->relDanger[i] = 0.0;
    }

    strcpy(sv->prevlcdMsg, "");
    strcpy(sv->lcdMsg, "");
    sv->safety = SAFE;
    sv->tuning = FALSE;
    sv->lastTune = 0;
    sv->tuningIndex = 0;
    sv->dangerCooldown = 0;
    sv->lastDanger = 0;
}

void ledInit(void) {
    softPwmCreate(PIN_SMD_RED, 0, 0xff);
    softPwmCreate(PIN_SMD_GRN, 0, 0xff);
    softPwmCreate(PIN_SMD_BLU, 0, 0xff);
    //softPwmCreate(PIN_DIP_RED, 0, 0xff);
    //softPwmCreate(PIN_DIP_GRN, 0, 0xff);
}

void init_sensors(SharedVariable* sv) {
    // initial input output modes of the pins
    pinMode(PIN_BUTTON, INPUT);

    pinMode(PIN_ROTARY_CLK, INPUT);
    pinMode(PIN_ROTARY_DT, INPUT);

    //pinMode(PIN_DIP_RED, OUTPUT);
    //pinMode(PIN_DIP_GRN, OUTPUT);
    pinMode(PIN_SMD_RED, OUTPUT);
    pinMode(PIN_SMD_GRN, OUTPUT);
    pinMode(PIN_SMD_BLU, OUTPUT);

    pinMode(PIN_ALED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    // initialize LED pulses
    ledInit();

    // initialize buzzer pulse
    softPwmCreate(PIN_BUZZER, 0, 100);

    softPwmWrite(PIN_SMD_BLU, 0);

    int fd;
    if ((fd = serialOpen ("/dev/ttyACM0", 115200)) < 0)
    {
        printf("no usb connection to arduino\n");
        sv->bProgramExit = 1;
        return;
    }
    sv->fd = fd;

    serialFlush(fd);

    char line[MAX_LINE_LENGTH];
    readSerialLine(fd, line);

    if (strcmp(line, "1") != 0) {
        printf("No response from arduino\n");
        sv->bProgramExit = 1;
        return;
    }

    serialFlush(fd);
}

void makelcdMsg(SharedVariable* sv, char* beginning, char* middle, double num) {
    //memset(sv->lcdMsg, 0, MAX_LINE_LENGTH);
    strcpy(sv->lcdMsg, beginning);
    strcat(sv->lcdMsg, middle);
    char str[16];
    snprintf(str, sizeof(str), "%.2f", num);
    strcat(sv->lcdMsg, str);
}

void body_button(SharedVariable* sv) {
    if (READ(PIN_BUTTON) == LOW) {
        // the point of lastPress is to prevent more than one switch
        // when the user holds down the button
        if (sv->lastPress == HIGH) {
            if (sv->safety == SAFE || sv->dangerCooldown > 0) {
                // tuning mode!
                if (sv->tuning == FALSE) {
                    sv->tuning = TRUE;
                } else {
                    sv->tuningIndex = (sv->tuningIndex + 1) % (NUMINPUTS * NUMVALUES);
                }
                sv->lastTune = millis();

                if (sv->tuningIndex % 2 == 0) {
                    makelcdMsg(sv, "L:", names[sv->tuningIndex/NUMVALUES], sv->info[sv->tuningIndex/NUMVALUES][LOW]);
                } else {
                    makelcdMsg(sv, "R:", names[sv->tuningIndex/NUMVALUES], sv->info[sv->tuningIndex/NUMVALUES][HIGH]);
                }
            }

            if (sv->safety == DANGER) {
                //sv->safety = SAFE;
                sv->dangerCooldown = millis();
            }
        }
        sv->lastPress = LOW;
    } else {
        sv->lastPress = HIGH;
        if (sv->dangerCooldown > 0 && millis() - sv->dangerCooldown >= DANGERCOOLDOWN_MAX && sv->tuning == FALSE) {
            sv->dangerCooldown = 0;
            sv->safety = SAFE;
            strcpy(sv->lcdMsg, "");
        }
    }
}

void body_encoder(SharedVariable* sv) {
    if (sv->tuning == FALSE) {
        return;
    }

    int currClk = READ(PIN_ROTARY_CLK);
    if (currClk != sv->lastClk && currClk == HIGH) {
        double stepsize;
        int sName = sv->tuningIndex/NUMVALUES;
        if (sv->tuningIndex % 2 == 0) {
            stepsize = (sv->info[sName][HIGH] - sv->info[sName][LOW])/ STEPFACTOR;
        } else {
            stepsize = (sv->info[sName][HIGH] - sv->info[sName][LOW])/ STEPFACTOR;
        }

        if (READ(PIN_ROTARY_DT) != currClk) {
            sv->rotation = COUNTER_CLOCKWISE;
            sv->info[sName][sv->tuningIndex%NUMVALUES] -= stepsize;
        } else {
            sv->rotation = CLOCKWISE;
            sv->info[sName][sv->tuningIndex%NUMVALUES] += stepsize;
        }

        if (sv->tuningIndex % 2 == 0) {
            makelcdMsg(sv, "L:", names[sName], sv->info[sName][sv->tuningIndex%NUMVALUES]);
            sv->stable[sName] = (sv->info[sName][LOW]+sv->info[sName][HIGH]) / 2.0;
        } else {
            makelcdMsg(sv, "R:", names[sName], sv->info[sName][sv->tuningIndex%NUMVALUES]);
            sv->stable[sName] = (sv->info[sName][HIGH]+sv->info[sName][LOW]) / 2.0;
        }
    }
    sv->lastClk = currClk;

    if (millis() - sv->lastTune >= TUNINGDEBOUNCE) {
        sv->tuning = FALSE;
    }
}

int clamp(int num, int min, int max) {
    if (num < min) {
        return min;
    }
    if (num > max) {
        return max;
    }
    return num;
}

// SMD treated as a two-color
void body_twocolor(SharedVariable* sv) {
    int i;
    double maxDanger = sv->relDanger[0];
    for (i = 1; i < NUMINPUTS; i++) {
        if (sv->relDanger[i] > maxDanger) {
            maxDanger = sv->relDanger[i];
        }
    }

    int greenLit = (int) (MAXCOLOR*(1-maxDanger));
    softPwmWrite(PIN_SMD_GRN, clamp(greenLit, 0, MAXCOLOR));
    softPwmWrite(PIN_SMD_RED, clamp(MAXCOLOR - greenLit, 0, MAXCOLOR));
}

void body_aled(SharedVariable* sv) {
    if (sv->dangerCooldown > 0) {
        TURN_OFF(PIN_ALED);
    }
    else if (sv->safety == DANGER && sv->lastDanger > 0 && millis() - sv->lastDanger >= DANGERTOCRITICAL) {
        TURN_ON(PIN_ALED);
    } else {
        TURN_OFF(PIN_ALED);
    }
}

void body_buzzer(SharedVariable* sv) {
    if (sv->dangerCooldown > 0) {
        softPwmWrite(PIN_BUZZER, 0);
    }
    else if (sv->safety == DANGER) {
        if (sv->lastDanger > 0 && millis() - sv->lastDanger >= DANGERTOCRITICAL) {
            softPwmWrite(PIN_BUZZER, 50);
        } 
        else if (millis() - sv->lastBeep >= 3000) {
            softPwmWrite(PIN_BUZZER, 50);
            sv->lastBeep = millis();
        } 
        else if (millis() - sv->lastBeep >= 1500) {
            softPwmWrite(PIN_BUZZER, 0);
        }
    }
    else {
        softPwmWrite(PIN_BUZZER, 0);
    }
}

void body_temphumid(SharedVariable* sv) {
    serialFlush(sv->fd);
    serialPrintf(sv->fd, "T");

    double humid, temp;
    char line[MAX_LINE_LENGTH];

    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("temp+humidity missed a byte\n");
        return;
    }
    humid = strtod(line, NULL);
    if (humid < 0) {
        printf("temp+humidity could not read\n");
        return;
    }

    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("temp+humidity missed a byte\n");
        return;
    }
    temp = strtod(line, NULL);

    //printf("HUMIDITY: %f\n", humid);
    //printf("TEMP: %f\n", temp);
    sv->relDanger[TEMP] = fabs(temp - sv->stable[TEMP]) / sv->stable[TEMP] * 2;
    sv->relDanger[HUMID] = fabs(humid - sv->stable[HUMID]) / sv->stable[HUMID] * 2;

    if (temp < sv->info[TEMP][LOW] || temp > sv->info[TEMP][HIGH]) {
        if (sv->safety == SAFE) {
            sv->lastDanger = millis();
            makelcdMsg(sv, "!!!", names[TEMP], temp);
        }
        sv->safety = DANGER;
    } 
    else if (humid < sv->info[HUMID][LOW] || humid > sv->info[HUMID][HIGH]) {
        if (sv->safety == SAFE) {
            sv->lastDanger = millis();
            makelcdMsg(sv, "!!!", names[HUMID], humid);
        }
        sv->safety = DANGER;
    }
    else if (fabs(temp - sv->temp) / (sv->info[TEMP][HIGH] - sv->info[TEMP][LOW]) >= CHANGETHRES) {
        sv->temp = temp;
        makelcdMsg(sv, "", names[TEMP], temp);
    }
    else if (fabs(humid - sv->humid) / (sv->info[HUMID][HIGH] - sv->info[HUMID][LOW]) >= CHANGETHRES) {
        sv->humid = humid;
        makelcdMsg(sv, "", names[HUMID], humid);
    }
} 

void body_air(SharedVariable* sv) {
    serialFlush(sv->fd);
    serialPrintf(sv->fd, "Q");

    double air;
    char line[MAX_LINE_LENGTH];

    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("air quality missed a byte");
        return;
    }
    air = strtod(line, NULL);

    if (air < 0) {
        printf("air quality sensor unavailable\n");
    } else {
        //printf("AIR QUALITY %f\n", air);
        sv->relDanger[AIR] = fabs(air - sv->stable[AIR]) / sv->stable[AIR] * 2;
        if (air < sv->info[AIR][LOW] || air > sv->info[AIR][HIGH]) {
            if (sv->safety == SAFE) {
                sv->lastDanger = millis();
                makelcdMsg(sv, "!!!", names[AIR], air);
            }
            sv->safety = DANGER;
        } else if (fabs(air - sv->air) / (sv->info[AIR][HIGH] - sv->info[AIR][LOW]) >= CHANGETHRES) {
            sv->air = air;
            makelcdMsg(sv, "", names[AIR], air);
        }
    }
}

void body_accel(SharedVariable* sv) {
    serialFlush(sv->fd);
    serialPrintf(sv->fd, "A");

    double aX, aY, aZ;
    char line[MAX_LINE_LENGTH];

    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("accel missed a byte");
        return;
    }
    aX = strtod(line, NULL);
    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("accel missed a byte");
        return;
    }
    aY = strtod(line, NULL);
    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("accel missed a byte");
        return;
    }
    aZ = strtod(line, NULL);

    //printf("ACCELERATION %f %f %f\n", aX, aY, aZ);
    aX = fabs(aX);
    aY = fabs(aY);
    aZ = fabs(aZ);
    // account for accelerometer noise
    if (aX < 0.01) { aX = 0; }
    if (aY < 0.01) { aY = 0; }
    if (aZ < 0.01) { aZ = 0; }

    sv->accelSum += aX + aY + aZ;

    if (sv->lastAccel == 0) {
        sv->lastAccel = millis();
    } else if (millis() - sv->lastAccel >= ACCELPERIOD) {
        sv->relDanger[ACCEL] = fabs(sv->accelSum - sv->stable[ACCEL]) / sv->stable[ACCEL] * 2;
        if (sv->accelSum < sv->info[ACCEL][LOW] || sv->accelSum > sv->info[ACCEL][HIGH]) {
            if (sv->safety == SAFE) {
                sv->lastDanger = millis();
                makelcdMsg(sv, "!!!", names[ACCEL], sv->accelSum);
            }
            sv->safety = DANGER;
        } else if (fabs(sv->accelSum - sv->accel) / (sv->info[ACCEL][HIGH] - sv->info[ACCEL][LOW]) >= CHANGETHRES) {
            sv->accel = sv->accelSum;
            makelcdMsg(sv, "", names[ACCEL], sv->accelSum);
        }
        sv->accelSum = 0.0;
        sv->lastAccel = 0;
    }
}

void body_lcd(SharedVariable* sv) {
    if (strcmp(sv->prevlcdMsg, sv->lcdMsg) != 0) {
        serialPrintf(sv->fd, "P%s\n", sv->lcdMsg);
        strcpy(sv->prevlcdMsg, sv->lcdMsg);
    }
}

void body_camera(SharedVariable* sv) {
    // printf("Detecting drowsiness...\n");
    double eye_ratio = -1.0;
    if (sv->pyObjects[2] && PyCallable_Check(sv->pyObjects[2])) {
        PyGILState_STATE gstate = PyGILState_Ensure();
	// printf("running the function...");
        PyObject *result = PyObject_CallObject(sv->pyObjects[2], NULL);
	// printf("got result");
        if (result == NULL) {
            PyErr_Print();
            printf("Error calling Python function.\n");
            PyGILState_Release(gstate);
            //sv->bProgramExit = 1;
            return;
        }

        if (PyFloat_Check(result)) {
            eye_ratio = PyFloat_AsDouble(result);
            //printf("Eye Ratio: %f\n", eye_ratio);
            if (eye_ratio >= 0) {
                sv->faceSum -= sv->faces[sv->faceIndex];
                sv->faces[sv->faceIndex] = eye_ratio;
                sv->faceSum += eye_ratio;
                double faceAvg = sv->faceSum / FACENUM;
                sv->relDanger[FACE] = fabs(faceAvg - sv->stable[FACE]) / sv->stable[FACE] * 2;
                if (faceAvg < sv->info[FACE][LOW] || faceAvg > sv->info[FACE][HIGH]) {
                    if (sv->safety == SAFE) {
                        sv->lastDanger = millis();
                        makelcdMsg(sv, "!!!", names[FACE], faceAvg);
                    }
                    sv->safety = DANGER;
                } else if (fabs(faceAvg - sv->face) / (sv->info[FACE][HIGH] - sv->info[FACE][LOW]) >= CHANGETHRES) {
                    sv->face = faceAvg;
                    makelcdMsg(sv, "", names[FACE], faceAvg);
                }
            }
        } else {
            printf("Returned value is not a float.\n");
        }

        Py_DECREF(result);
        PyGILState_Release(gstate);
    }
    sv->eye_ratio = eye_ratio;
}
