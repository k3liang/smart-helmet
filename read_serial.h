#ifndef _READSERIAL_BODY_
#define _READSERIAL_BODY_

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <wiringSerial.h>
#include <wiringPi.h> 

#define MAX_LINE_LENGTH 256

char* readSerialData(const int fd, char* buffer);
char* readSerialLine(const int fd, char* line);
char* readSerialLine2(const int fd, char* line, unsigned int timeout);

#endif