#ifndef _READSERIAL_BODY_
#define _READSERIAL_BODY_

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <wiringSerial.h>

#define MAX_LINE_LENGTH 256

char* readSerialData(const int fd, char* buffer);
char* readSerialLine(const int fd, char* line);

#endif