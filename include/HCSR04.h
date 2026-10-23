/*
* HCSR04.h - CPEG222 Project 6
* Interface for the HC-SR04 ultrasonic sensor helper.
*/
#ifndef HCSR04_H
#define HCSR04_H

#include "stm32f4xx.h"
#include <stdint.h>

#define HCSR04_ECHO_PORT GPIOG // ECHO is connected to PG1
#define HCSR04_ECHO_PIN  1U

// Sets up the TRIG pin, the ECHO pin as an input, and TIM7 as a free running 1 us counter
void HCSR04_Init(void);

// Sends the roughly 10 us HIGH pulse on TRIG that starts one measurement
void HCSR04_Trigger(void);

// Waits the given number of microseconds using TIM7 as the time base
void delay_us(uint16_t us);

// Returns the current TIM7 count in microseconds, which wraps every 65.536 ms
uint16_t HCSR04_Timestamp(void);

#endif