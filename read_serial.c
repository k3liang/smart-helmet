/*
 * serial.c:
 *	Example program to read bytes from the Serial line
 *
 * Copyright (c) 2012-2013 Gordon Henderson.
 ***********************************************************************
 * This file is part of wiringPi:
 *      https://github.com/WiringPi/WiringPi
 *
 *    wiringPi is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    wiringPi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with wiringPi.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <wiringSerial.h>

#define MAX_LINE_LENGTH 256

char* readSerialData(const int fd);
char* readSerialLine(const int fd);

int main ()
{
  int fd ;

  if ((fd = serialOpen ("/dev/ttyACM0", 115200)) < 0)
  {
    fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno)) ;
    return 1 ;
  }

  serialFlush(fd);
// Loop, getting and printing characters

  int count = 0;
  while (1 == 1)
  {
    if (serialDataAvail(fd) > 0) {
      char* str = readSerialLine(fd);
      printf(str);
      // fflush(stdout);
      //int i;
      //for (i = 0; i < 21; i++) {
      //  printf("%d\n", str[i]);
      //}
      printf("%ld\n", strlen(str));
      // printf("%d\n", len);
      serialPrintf(fd, "WE SO BACK %d\n", count);
      //serialPrintf(fd, "WE SO BACK\n");
      count++;
    }
    // fflush (stdout) ;
  }
}

char* readSerialData(const int fd) {
    static char buffer[MAX_LINE_LENGTH];  // Static buffer to hold the data
    int index = 0;
    int availableData;
    int ch;

    // Clear the buffer before reading the new data
    memset(buffer, 0, MAX_LINE_LENGTH);

    // Get the number of available bytes in the serial buffer
    availableData = serialDataAvail(fd);

    // Read available data from the serial port
    while (index < availableData && index < MAX_LINE_LENGTH - 1) {
        ch = serialGetchar(fd);  // Read next character from the serial port

        if (ch == -1) {
            // Handle error (e.g., no data available)
            break;
        }

        buffer[index++] = (char)ch;  // Store the character in the buffer
    }

    // Null-terminate the buffer to make it a valid C string
    buffer[index] = '\0';

    return buffer;  // Return the buffer that was read
}

char* readSerialLine(const int fd) {
    static char line[MAX_LINE_LENGTH];  // Static buffer to hold the line
    int index = 0;
    int ch;

    // Clear the buffer before reading the new line
    memset(line, 0, MAX_LINE_LENGTH);

    // Read characters until a newline is encountered or we reach the max length
    while (index < MAX_LINE_LENGTH - 1) {
        ch = serialGetchar(fd);  // Read next character from the serial port

        if (ch == -1) {
            // Handle error (e.g., no data available)
            continue;
        }

        if (ch == '\n') {
            // End of line, terminate the string
            if (index > 0 && line[index-1] == 13) {
                line[index-1] = '\0';
                break;
            }
            line[index] = '\0';
            //line[index]='\n';
            //line[index+1] = '\0';
            break;
        }

        // Store the character in the buffer and move to the next index
        line[index++] = (char)ch;
    }
    return line;  // Return the line that was read
}
