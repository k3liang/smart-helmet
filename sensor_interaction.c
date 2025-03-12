#include "sensor_interaction.h"
#include "read_serial.h"
#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wiringSerial.h>
#include "scheduler2.h"

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

void clean_sensors(SharedVariable* sv) {
    serialPrintf(sv->fd, "P\n"); // empty LCD display
    
    // turn off LEDs and buzzer
    softPwmWrite(PIN_SMD_GRN, 0);
    softPwmWrite(PIN_SMD_RED, 0);

    TURN_OFF(PIN_ALED);

    softPwmWrite(PIN_BUZZER, 0);
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
    
    sv->dataS[TEMP] = sv->temp;
    sv->dataS[HUMID] = sv->humid;
    sv->dataS[AIR] = sv->air;
    sv->dataS[ACCEL] = sv->accel;
    sv->dataS[FACE] = sv->face;
    
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
        sv->prevRelDanger[i] = 0.0;
    }
    sv->sumDanger = 0.0;

    strcpy(sv->prevlcdMsg, "");
    strcpy(sv->lcdMsg, "");
    sv->printState = FREE;

    sv->lastWarn = 0;

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
    if ((fd = serialOpen ("/dev/ttyACM0", 9600)) < 0)
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

void makelcdMsg(SharedVariable* sv, char* beginning, char* middle, double num, double num2) {
    //memset(sv->lcdMsg, 0, MAX_LINE_LENGTH);
    if ((beginning[0]=='R' || beginning[0]=='L') && beginning[1] == ':') {
        sv->printState = PTUNING;
    } else if (beginning[0] == '!') {
        if (sv->printState == PTUNING && sv->tuning == TRUE) {
            return;
        }
        sv->printState = PDANGER;
    } else if (beginning[0]=='W'&&beginning[1]=='A'&&beginning[2]=='R'&&beginning[3]=='N') {
        if ((sv->printState == PTUNING && sv->tuning == TRUE) || (sv->printState == PDANGER && sv->safety == DANGER)) {
            return;
        }
        sv->printState = PWARN;
        sv->lastWarn = millis();
    } else {
        if ((sv->printState == PTUNING && sv->tuning == TRUE) || (sv->printState == PDANGER && sv->safety == DANGER) || (sv->printState == PWARN && millis() - sv->lastWarn <= WARNWAIT)) {
            return;
        }
        sv->printState = FREE;
    }
    if (sv->printState == PTUNING || sv->printState == PWARN) {
        strcpy(sv->lcdMsg, "-");
        strcat(sv->lcdMsg, beginning);
    } else {
        strcpy(sv->lcdMsg, beginning);
    }
    strcat(sv->lcdMsg, middle);
    strcat(sv->lcdMsg, " ");
    char str[17];
    snprintf(str, sizeof(str), "%.2f", num);
    strcat(sv->lcdMsg, str);
    if (sv->printState == PTUNING || sv->printState == PWARN) {
        char str2[17];
        snprintf(str2, sizeof(str2), "%.2f", num2);
        strcat(sv->lcdMsg, "_");
        strcat(sv->lcdMsg, str2);
    }
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
                    tuneOn(sv);
                } else {
                    sv->tuningIndex = (sv->tuningIndex + 1) % (NUMINPUTS * NUMVALUES);
                }
                sv->lastTune = millis();

                if (sv->tuningIndex % 2 == 0) {
                    makelcdMsg(sv, "L:", names[sv->tuningIndex/NUMVALUES], sv->info[sv->tuningIndex/NUMVALUES][LOW], sv->dataS[sv->tuningIndex/NUMVALUES]);
                } else {
                    makelcdMsg(sv, "R:", names[sv->tuningIndex/NUMVALUES], sv->info[sv->tuningIndex/NUMVALUES][HIGH], sv->dataS[sv->tuningIndex/NUMVALUES]);
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
            alarmOff(sv);
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
        sv->lastTune = millis();
        if (sv->tuningIndex % 2 == 0) {
            stepsize = (sv->info[sName][HIGH] - sv->info[sName][LOW])/ STEPFACTOR;
        } else {
            stepsize = (sv->info[sName][HIGH] - sv->info[sName][LOW])/ STEPFACTOR;
        }

        int colValue = sv->tuningIndex % NUMVALUES;

        if (READ(PIN_ROTARY_DT) != currClk) {
            sv->rotation = COUNTER_CLOCKWISE;
            sv->info[sName][colValue] -= stepsize;

            if (sv->info[sName][LOW] > sv->info[sName][HIGH]) {
                sv->info[sName][colValue] += stepsize;
            }
        } else {
            sv->rotation = CLOCKWISE;
            sv->info[sName][colValue] += stepsize;

            if (sv->info[sName][LOW] > sv->info[sName][HIGH]) {
                sv->info[sName][colValue] -= stepsize;
            }
        }

        if (sv->tuningIndex % 2 == 0) {
            makelcdMsg(sv, "L:", names[sName], sv->info[sName][colValue], sv->dataS[sName]);
            sv->stable[sName] = (sv->info[sName][LOW]+sv->info[sName][HIGH]) / 2.0;
        } else {
            makelcdMsg(sv, "R:", names[sName], sv->info[sName][colValue], sv->dataS[sName]);
            sv->stable[sName] = (sv->info[sName][HIGH]+sv->info[sName][LOW]) / 2.0;
        }
    }
    sv->lastClk = currClk;

    if (millis() - sv->lastTune >= TUNINGDEBOUNCE) {
        sv->tuning = FALSE;
        tuneOff(sv);
        strcpy(sv->lcdMsg, "");
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
    int j = 0;
    double maxDanger = sv->relDanger[0];
    for (i = 1; i < NUMINPUTS; i++) {
        if (sv->relDanger[i] > maxDanger) {
            maxDanger = sv->relDanger[i];
            j = i;
        }
    }

    if (sv->tuning == FALSE) {
        if (maxDanger >= RELDANGERTHRES && sv->prevRelDanger[j] < RELDANGERTHRES) {
            makelcdMsg(sv, "WARN ", names[j], maxDanger, sv->dataS[j]);
        }
        sv->prevRelDanger[j] = sv->relDanger[j];
    }

    int greenLit = (int) (MAXCOLOR*(1-maxDanger));
    softPwmWrite(PIN_SMD_GRN, clamp(greenLit, 0, MAXCOLOR));
    softPwmWrite(PIN_SMD_RED, clamp(MAXCOLOR - greenLit, 0, MAXCOLOR));
}

void body_aled(SharedVariable* sv) {
    if (sv->tuning == TRUE) {
        TURN_OFF(PIN_ALED);
    }
    else if (sv->dangerCooldown > 0) {
        TURN_OFF(PIN_ALED);
    }
    else if (sv->safety == DANGER && sv->lastDanger > 0 && millis() - sv->lastDanger >= DANGERTOCRITICAL) {
        TURN_ON(PIN_ALED);
    } else {
        TURN_OFF(PIN_ALED);
    }
}

void body_buzzer(SharedVariable* sv) {
    if (sv->tuning == TRUE) {
        softPwmWrite(PIN_BUZZER, 0);
    }
    else if (sv->dangerCooldown > 0) {
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
    //serialFlush(sv->fd);
    serialPrintf(sv->fd, "T");

    double humid, temp;
    char line[MAX_LINE_LENGTH];

    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("temp+humidity missed a byte\n");
        return;
    }
    humid = strtod(line, NULL);
    if (humid < 0) {
        //printf("temp+humidity could not read\n");
        return;
    }

    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("temp+humidity missed a byte\n");
        return;
    }
    temp = strtod(line, NULL);
    
    sv->dataS[TEMP] = temp;
    sv->dataS[HUMID] = humid;

    //printf("HUMIDITY: %f\n", humid);
    //printf("TEMP: %f\n", temp);
    if (sv->relDanger[TEMP] > sv->relDanger[HUMID]) {
        sv->sumDanger -= sv->relDanger[TEMP]*sv->relDanger[TEMP]*sv->relDanger[TEMP];
    } else {
        sv->sumDanger -= sv->relDanger[HUMID]*sv->relDanger[HUMID]*sv->relDanger[HUMID];
    }
    sv->relDanger[TEMP] = fabs(temp - sv->stable[TEMP]) / (sv->info[TEMP][HIGH] - sv->info[TEMP][LOW]) * 2;
    sv->relDanger[HUMID] = fabs(humid - sv->stable[HUMID]) / (sv->info[HUMID][HIGH] - sv->info[HUMID][LOW]) * 2;
    if (sv->relDanger[TEMP] > sv->relDanger[HUMID]) {
        sv->sumDanger += sv->relDanger[TEMP]*sv->relDanger[TEMP]*sv->relDanger[TEMP];
        if (DEBUG == 1 && DEBUG2 == 1) {
            makelcdMsg(sv, "", names[TEMP], temp, 0);
        }
    } else {
        sv->sumDanger += sv->relDanger[HUMID]*sv->relDanger[HUMID]*sv->relDanger[HUMID];
        if (DEBUG == 1 && DEBUG2 == 1) {
            makelcdMsg(sv, "", names[HUMID], humid, 0);
        }
    }

    if (sv->tuning == TRUE) {
        return;
    }
    if (temp < sv->info[TEMP][LOW] || temp > sv->info[TEMP][HIGH]) {
        if (sv->safety == SAFE) {
            sv->lastDanger = millis();
            alarmOn(sv);
            makelcdMsg(sv, "!!!", names[TEMP], temp, 0);
        }
        sv->safety = DANGER;
    } 
    else if (humid < sv->info[HUMID][LOW] || humid > sv->info[HUMID][HIGH]) {
        if (sv->safety == SAFE) {
            sv->lastDanger = millis();
            alarmOn(sv);
            makelcdMsg(sv, "!!!", names[HUMID], humid, 0);
        }
        sv->safety = DANGER;
    }
    else if (fabs(temp - sv->temp) / (sv->info[TEMP][HIGH] - sv->info[TEMP][LOW]) >= CHANGETHRES) {
        sv->temp = temp;
        makelcdMsg(sv, "", names[TEMP], temp, 0);
    }
    else if (fabs(humid - sv->humid) / (sv->info[HUMID][HIGH] - sv->info[HUMID][LOW]) >= CHANGETHRES) {
        sv->humid = humid;
        makelcdMsg(sv, "", names[HUMID], humid, 0);
    }
} 

void body_air(SharedVariable* sv) {
    //serialFlush(sv->fd);
    serialPrintf(sv->fd, "Q");

    double air;
    char line[MAX_LINE_LENGTH];

    if (readSerialLine2(sv->fd, line, 500) == NULL) {
        printf("air quality missed a byte");
        return;
    }
    air = strtod(line, NULL);

    if (air < 0) {
        //printf("air quality sensor unavailable\n");
    } else {
        //printf("AIR QUALITY %f\n", air);
        sv->dataS[AIR] = air;
        sv->sumDanger -= sv->relDanger[AIR]*sv->relDanger[AIR]*sv->relDanger[AIR];
        sv->relDanger[AIR] = fabs(air - sv->stable[AIR]) / (sv->info[AIR][HIGH] - sv->info[AIR][LOW]) * 2;
        sv->sumDanger += sv->relDanger[AIR]*sv->relDanger[AIR]*sv->relDanger[AIR];
        if (DEBUG == 1 && DEBUG2 == 1) {
            makelcdMsg(sv, "", names[AIR], air, 0);
        }
        if (sv->tuning == TRUE) {
            return;
        }
        if (air < sv->info[AIR][LOW] || air > sv->info[AIR][HIGH]) {
            if (sv->safety == SAFE) {
                sv->lastDanger = millis();
                alarmOn(sv);
                makelcdMsg(sv, "!!!", names[AIR], air, 0);
            }
            sv->safety = DANGER;
        } else if (fabs(air - sv->air) / (sv->info[AIR][HIGH] - sv->info[AIR][LOW]) >= CHANGETHRES) {
            sv->air = air;
            makelcdMsg(sv, "", names[AIR], air, 0);
        }
    }
}

void body_accel(SharedVariable* sv) {
    //serialFlush(sv->fd);
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
    if (aX < ACCELNOISE) { aX = 0; }
    if (aY < ACCELNOISE) { aY = 0; }
    if (aZ < ACCELNOISE) { aZ = 0; }

    if (sv->tuning == TRUE) {
        sv->accelSum = 0.0;
        sv->lastAccel = 0;
        return;
    }

    sv->accelSum += aX + aY + aZ;
    if (DEBUG == 1 && DEBUG2 == 1) {
        makelcdMsg(sv, "", names[ACCEL], sv->accelSum, 0);
    }

    if (sv->lastAccel == 0) {
        sv->lastAccel = millis();
    } else if (millis() - sv->lastAccel >= ACCELPERIOD) {
        sv->dataS[ACCEL] = sv->accelSum;
        sv->sumDanger -= sv->relDanger[ACCEL]*sv->relDanger[ACCEL]*sv->relDanger[ACCEL];
        sv->relDanger[ACCEL] = fabs(sv->accelSum - sv->stable[ACCEL]) / (sv->info[ACCEL][HIGH] - sv->info[ACCEL][LOW]) * 2;
        sv->sumDanger += sv->relDanger[ACCEL]*sv->relDanger[ACCEL]*sv->relDanger[ACCEL];
        if (sv->accelSum < sv->info[ACCEL][LOW] || sv->accelSum > sv->info[ACCEL][HIGH]) {
            if (sv->safety == SAFE) {
                sv->lastDanger = millis();
                alarmOn(sv);
                makelcdMsg(sv, "!!!", names[ACCEL], sv->accelSum, 0);
            }
            sv->safety = DANGER;
        } else if (fabs(sv->accelSum - sv->accel) / (sv->info[ACCEL][HIGH] - sv->info[ACCEL][LOW]) >= CHANGETHRES) {
            sv->accel = sv->accelSum;
            makelcdMsg(sv, "", names[ACCEL], sv->accelSum, 0);
        }
        sv->accelSum = 0.0;
        sv->lastAccel = 0;
    }
}

void body_lcd(SharedVariable* sv) {
    if (strcmp(sv->prevlcdMsg, sv->lcdMsg) != 0) {
        if (sv->lcdMsg[0] == '-') {
            serialPrintf(sv->fd, "p%s\n", sv->lcdMsg+1);
        } else {
            serialPrintf(sv->fd, "P%s\n", sv->lcdMsg);
        }
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
                sv->faceIndex = (sv->faceIndex + 1) % FACENUM;
                double faceAvg = sv->faceSum / FACENUM;
                sv->sumDanger -= sv->relDanger[FACE]*sv->relDanger[FACE]*sv->relDanger[FACE];
                sv->relDanger[FACE] = fabs(faceAvg - sv->stable[FACE]) / (sv->info[FACE][HIGH] - sv->info[FACE][LOW]) * 2;
                sv->sumDanger += sv->relDanger[FACE]*sv->relDanger[FACE]*sv->relDanger[FACE];
                sv->dataS[FACE] = faceAvg;
                if (DEBUG == 1 && DEBUG2 == 1) {
                    makelcdMsg(sv, "", names[FACE], faceAvg, 0);
                }
                if (sv->tuning == TRUE) {
                    //return;
                }
                else if (faceAvg < sv->info[FACE][LOW] || faceAvg > sv->info[FACE][HIGH]) {
                    if (sv->safety == SAFE) {
                        sv->lastDanger = millis();
                        alarmOn(sv);
                        makelcdMsg(sv, "!!!", names[FACE], faceAvg, 0);
                    }
                    sv->safety = DANGER;
                } else if (fabs(faceAvg - sv->face) / (sv->info[FACE][HIGH] - sv->info[FACE][LOW]) >= CHANGETHRES) {
                    sv->face = faceAvg;
                    makelcdMsg(sv, "", names[FACE], faceAvg, 0);
                }
            } else {
                double faceAvg = sv->faceSum / FACENUM;
                sv->dataS[FACE] = faceAvg;

                if (DEBUG == 1 && DEBUG2 == 1) {
                    makelcdMsg(sv, "", names[FACE], faceAvg, 0);
                }
                if (sv->tuning == TRUE) {
                    //return;
                }
                else if (faceAvg < sv->info[FACE][LOW] || faceAvg > sv->info[FACE][HIGH]) {
                    if (sv->safety == SAFE) {
                        sv->lastDanger = millis();
                        alarmOn(sv);
                        makelcdMsg(sv, "!!!", names[FACE], faceAvg, 0);
                    }
                    sv->safety = DANGER;
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
