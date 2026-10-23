/*
* UART3.h - CPEG222 Project 6
* Interface for sending text to the serial monitor over UART3.
*/
#ifndef UART3_H
#define UART3_H

#include "stm32f4xx.h"

// Configures PD8/PD9 for UART3 at 115200 baud, 8N1
void uart3_init(void);

// Sends one character
void uart3_send_char(char c);

// Sends a null terminated string
void uart3_send_string(const char *s);

#endif