#include "sensor_interaction.h"
#include "read_serial.h"
#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <stdlib.h>
#include <string.h>

#include <wiringSerial.h>

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

        sv->bounds[index][i] = value;
        token = strtok(NULL, " \n");
    }

    return 0;
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
    sv->state = RUNNING;
    sv->rotation = CLOCKWISE;
    sv->sound = NO_SOUND;
    sv->motion = NO_MOTION;
    sv->lastClk = LOW;
    sv->lastBeep = 0;
    sv->lastPress = HIGH;
    sv->temp = sv->bounds[TEMP][STABLE];
    sv->humid = sv->bounds[HUMID][STABLE];
    sv->air = sv->bounds[AIR][STABLE];
    sv->accel = sv->bounds[ACCEL][STABLE];
    strncpy(sv->lcdMsg, "Print to DP", 12);
    sv->safety = SAFE;
    sv->lastDanger = 0;
}

void ledInit(void) {
    //initialize SMD and DIP
    //softPwmCreate(PIN_SMD_RED, 0, 0xff);
    //softPwmCreate(PIN_SMD_GRN, 0, 0xff);
    //softPwmCreate(PIN_SMD_BLU, 0, 0xff);
    softPwmCreate(PIN_DIP_RED, 0, 0xff);
    softPwmCreate(PIN_DIP_GRN, 0, 0xff);
}

void init_sensors(SharedVariable* sv) {
    // initial input output modes of the pins
    pinMode(PIN_BUTTON, INPUT);

    pinMode(PIN_ROTARY_CLK, INPUT);
    pinMode(PIN_ROTARY_DT, INPUT);

    pinMode(PIN_DIP_RED, OUTPUT);
    pinMode(PIN_DIP_GRN, OUTPUT);

    pinMode(PIN_ALED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    // initialize LED pulses
    ledInit();

    // initialize buzzer pulses
    softPwmCreate(PIN_BUZZER, 0, 100);

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

// 1. Button
void body_button(SharedVariable* sv) {
    // switch running state if button is pressed
    if (READ(PIN_BUTTON) == LOW) {
        // the point of lastPress is to prevent more than one switch
        // when the user holds down the button
        if (sv->lastPress == HIGH) {
            /*
            if (sv->state == RUNNING) {
                sv->state = PAUSE;
            } else {
                sv->state = RUNNING;
            }
            */
            if (sv->safety == DANGER) {
                sv->safety = SAFE;
            }
        }
        sv->lastPress = LOW;
    } else {
        sv->lastPress = HIGH;
    }
}

// 4. Rotary Encoder
void body_encoder(SharedVariable* sv) {
    int currClk = READ(PIN_ROTARY_CLK);
    if (currClk != sv->lastClk && currClk == HIGH) {
        if (READ(PIN_ROTARY_DT) != currClk) {
            sv->rotation = COUNTER_CLOCKWISE;
        } else {
            sv->rotation = CLOCKWISE;
        }
    }
    sv->lastClk = currClk;
}

// 5. DIP two-color LED
void body_twocolor(SharedVariable* sv) {
    if (sv->state == PAUSE) {
        softPwmWrite(PIN_DIP_RED, 0);
        softPwmWrite(PIN_DIP_GRN, 0);
        return;
    }

    if (sv->motion == NO_MOTION) {
        softPwmWrite(PIN_DIP_RED, 0);
        softPwmWrite(PIN_DIP_GRN, 0xff);
    } else {
        softPwmWrite(PIN_DIP_GRN, 0);
        softPwmWrite(PIN_DIP_RED, 0xff);
    }
}

// 7. Auto-flash LED
void body_aled(SharedVariable* sv) {
    if (sv->safety == SAFE) {
        TURN_OFF(PIN_ALED);
    } else {
        TURN_ON(PIN_ALED);
    }
}

// 8. Buzzer
void body_buzzer(SharedVariable* sv) {
    if (sv->sound == DETECT_SOUND && sv->state == RUNNING) {
        softPwmWrite(PIN_BUZZER, 50);
        sv->lastBeep = millis();
    }

    // im using a zero check for lastBeep in order to 
    // reduce the amount of calls to millis()
    if (sv->lastBeep > 0) {
        if (millis() - sv->lastBeep >= 3000) {
            softPwmWrite(PIN_BUZZER, 0);
            sv->lastBeep = 0;
        }
    }
}

void body_temphumid(SharedVariable* sv) {
    serialPrintf(sv->fd, "T");

    double humid, temp;
    char line[MAX_LINE_LENGTH];

    readSerialLine(sv->fd, line);
    humid = strtod(line, NULL);
    readSerialLine(sv->fd, line);
    temp = strtod(line, NULL);

    printf("HUMIDITY: %f\n", humid);
    printf("TEMP: %f\n", temp);
} 
void body_air(SharedVariable* sv) {
    serialPrintf(sv->fd, "Q");

    double air;
    char line[MAX_LINE_LENGTH];

    readSerialLine(sv->fd, line);
    air = strtod(line, NULL);

    if (air < 0) {
        printf("air quality sensor unavailable\n");
    } else {
        printf("AIR QUALITY %f\n", air);
    }
}
void body_accel(SharedVariable* sv) {
    serialPrintf(sv->fd, "A");

    double aX, aY, aZ;
    char line[MAX_LINE_LENGTH];

    readSerialLine(sv->fd, line);
    aX = strtod(line, NULL);
    readSerialLine(sv->fd, line);
    aY = strtod(line, NULL);
    readSerialLine(sv->fd, line);
    aZ = strtod(line, NULL);

    printf("ACCELERATION %f %f %f\n", aX, aY, aZ);
}

void body_lcd(SharedVariable* sv) {
    serialPrintf(sv->fd, "P%s\n", sv->lcdMsg);
}

void body_camera(SharedVariable* sv) {
    printf("Detecting drowsiness...\n");
    double eye_ratio = -1.0;
    if (sv->pyObjects[2] && PyCallable_Check(sv->pyObjects[2])) {
        PyGILState_STATE gstate = PyGILState_Ensure();
	printf("running the function...");
        PyObject *result = PyObject_CallObject(sv->pyObjects[2], NULL);
	printf("got result");
        if (result == NULL) {
            PyErr_Print();
            printf("Error calling Python function.\n");
            PyGILState_Release(gstate);
            return;
        }

        if (PyFloat_Check(result)) {
            eye_ratio = PyFloat_AsDouble(result);
            printf("Eye Ratio: %f\n", eye_ratio);
        } else {
            printf("Returned value is not a float.\n");
        }

        Py_DECREF(result);
        PyGILState_Release(gstate);
    }
    if(eye_ratio > 0){
	    sv->bProgramExit = 1;
    }
    sv->eye_ratio = eye_ratio;
}
