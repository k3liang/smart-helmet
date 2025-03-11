#include "sensor_interaction.h"
#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <stdlib.h>
#include <string.h>

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

int init_python(PyObject* pyObjects[4]) {
    Py_Initialize();

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
        PyObject_CallObject(pInit, NULL);
    }

    sv->pyObjects[0] = pModule;
    sv->pyObjects[1] = pInit;
    sv->pyObjects[2] = pDetect;
    sv->pyObjects[3] = pCleanup;

    return 0;
}

int clean_python(SharedVariable* sv) {
    Py_XDECREF(sv->pyObjects[1]);
    Py_XDECREF(sv->pyObjects[2]);
    Py_XDECREF(sv->pyObjects[3]);
    Py_DECREF(sv->pyObjects[0]);

    Py_Finalize();
    return 0;
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
    sv->heart = sv->bounds[HEART][STABLE];
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
    //pinMode(PIN_MOTION, INPUT);
    pinMode(PIN_ROTARY_CLK, INPUT);
    pinMode(PIN_ROTARY_DT, INPUT);
    //pinMode(PIN_SOUND, INPUT);

    pinMode(PIN_DIP_RED, OUTPUT);
    pinMode(PIN_DIP_GRN, OUTPUT);
    //pinMode(PIN_SMD_RED, OUTPUT);
    //pinMode(PIN_SMD_GRN, OUTPUT);
    //pinMode(PIN_SMD_BLU, OUTPUT);
    pinMode(PIN_ALED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    // initialize LED pulses
    ledInit();

    // initialize buzzer pulses
    softPwmCreate(PIN_BUZZER, 0, 100);

    pinMode(PIN_TEMPHUMID, INPUT);
    pinMode(PIN_AIR, INPUT);
    pinMode(PIN_HEART, INPUT);
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

// 2. Infrared Motion Sensor
/*
void body_motion(SharedVariable* sv) {
    if (READ(PIN_MOTION) == LOW) {
        sv->motion = DETECT_MOTION;
    } else {
        sv->motion = NO_MOTION;
    }
}
*/

// 3. Microphone Sound Sensor
/*
void body_sound(SharedVariable* sv) {
    if (READ(PIN_SOUND) == HIGH) {
        sv->sound = DETECT_SOUND;
    } else {
        sv->sound = NO_SOUND;
    }
}
*/

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

// 6. SMD RGB LED
/*
void body_rgbcolor(SharedVariable* sv) {
    if (sv->state == PAUSE) {
        softPwmWrite(PIN_SMD_RED, 0);
        softPwmWrite(PIN_SMD_GRN, 0);
        softPwmWrite(PIN_SMD_BLU, 0);
        return;
    }

    if (sv->motion == NO_MOTION) {
        if (sv->rotation == CLOCKWISE) {
            softPwmWrite(PIN_SMD_RED, 0xff);
            softPwmWrite(PIN_SMD_GRN, 0);
            softPwmWrite(PIN_SMD_BLU, 0);
        } else {
            softPwmWrite(PIN_SMD_RED, 0xee);
            softPwmWrite(PIN_SMD_GRN, 0);
            softPwmWrite(PIN_SMD_BLU, 0xc8);
        }
    } else {
        if (sv->rotation == CLOCKWISE) {
            softPwmWrite(PIN_SMD_RED, 0x80);
            softPwmWrite(PIN_SMD_GRN, 0xff);
            softPwmWrite(PIN_SMD_BLU, 0);
        } else {
            softPwmWrite(PIN_SMD_RED, 0);
            softPwmWrite(PIN_SMD_GRN, 0xff);
            softPwmWrite(PIN_SMD_BLU, 0xff);
        }
    }
}
*/

// 7. Auto-flash LED
void body_aled(SharedVariable* sv) {
    if (sv->state == PAUSE) {
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
    
} 
void body_air(SharedVariable* sv) {

}
void body_heart(SharedVariable* sv) {

}
void body_camera(SharedVariable* sv) {
    double eye_ratio = -1.0;
    if (sv->pyObjects[2] && PyCallable_Check(sv->pyObjects[2])) {
        PyObject *result = PyObject_CallObject(sv->pyObjects[2], NULL);

        if (result == NULL) {
            PyErr_Print();
            printf("Error calling Python function.\n");
            return;
        }

        if (PyFloat_Check(result)) {
            eye_ratio = PyFloat_AsDouble(result);
            printf("Eye Ratio: %f\n", eye_ratio);
        } else {
            printf("Returned value is not a float.\n");
        }

        Py_DECREF(result);
    }
    sv->eye_ratio = eye_ratio;
}