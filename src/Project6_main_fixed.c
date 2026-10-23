/*
* Author: Brian Johnson
* CPEG222 Project 6, 10/20/26
* NucleoF446ZE + CPEG222 Shield - CMSIS bare metal
*
* Ultrasonic range finder. SysTick fires every 1 ms and sets a flag every 0.5 s to start a reading.
* The main loop sends a 10 us TRIG pulse, and EXTI1 interrupts on both ECHO edges use the free running
* TIM7 (1 us per count) to time the pulse. The pulse width and distance are shown on the OLED and sent
* over UART3. Slide switch S1 selects inches (1) or centimeters (0).
*/
#include "stm32f4xx.h"
#include "OLED.h"
#include "SysClock180MHz.h"
#include "HCSR04.h"
#include "UART3.h"
#include <stdint.h>
#include <stdio.h>

#define S1_PORT            GPIOC
#define S1_PIN             8U      // S1 = PC8
#define MEASURE_PERIOD_MS  500U    // 0.5 s between readings, per the rubric
#define ECHO_TIMEOUT_MS    40U     // give up if no echo comes back in 40 ms
#define US_PER_CM          58.3f   // round trip: 2 cm of travel takes about 58.3 us at 343 m/s
#define CM_PER_INCH        2.54f

// Variables shared between the interrupts and the main loop
volatile uint32_t msTicks = 0;      // counts milliseconds, updated by SysTick
volatile uint8_t  measureFlag = 0;  // set by SysTick every 0.5 s to start a reading
volatile uint8_t  echoDone = 0;     // set by the EXTI1 handler when the ECHO pulse has ended
volatile uint16_t echoStart = 0;    // TIM7 count captured at the rising edge
volatile uint16_t pulseWidth = 0;   // ECHO pulse width in microseconds

// Turns on the floating point unit so float math runs in hardware instead of faulting
static void FPU_Enable(void) {
    SCB->CPACR |= (0xFU << 20);
    __DSB();
    __ISB();
}

// Sets slide switch S1 (PC8) up as a plain digital input
static void S1_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    S1_PORT->MODER &= ~(0x3U << (S1_PIN * 2U));
}

// Routes PG1 to EXTI line 1 and interrupts on both the rising and falling edge of ECHO
static void EXTI_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->EXTICR[0] &= ~(0xFU << 4);
    SYSCFG->EXTICR[0] |=  (0x6U << 4);  // 0b0110 selects port G for line 1
    EXTI->IMR  |= (1U << 1);
    EXTI->RTSR |= (1U << 1);
    EXTI->FTSR |= (1U << 1);
    NVIC_SetPriority(EXTI1_IRQn, 0);    // highest priority so edge timestamps are never delayed
    NVIC_EnableIRQ(EXTI1_IRQn);
}

// Configures SysTick to interrupt every 1 ms
static void SysTick_Init(void) {
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    NVIC_SetPriority(SysTick_IRQn, 1);
}

// Every 1 ms: counts time, and every 0.5 s raises the flag that starts a new reading
void SysTick_Handler(void) {
    static uint32_t periodCount = 0;
    msTicks++;
    if (++periodCount >= MEASURE_PERIOD_MS) {
        periodCount = 0;
        measureFlag = 1;
    }
}

// On each ECHO edge: a high pin means the pulse just started, a low pin means it just ended
void EXTI1_IRQHandler(void) {
    if (EXTI->PR & (1U << 1)) {
        EXTI->PR = (1U << 1);  // write 1 to clear the pending flag
        if (HCSR04_ECHO_PORT->IDR & (1U << HCSR04_ECHO_PIN)) {
            echoStart = HCSR04_Timestamp();
        } else {
            pulseWidth = (uint16_t)(HCSR04_Timestamp() - echoStart);
            echoDone = 1;
        }
    }
}

// Prints a line on the OLED padded to 21 characters so leftovers from the previous reading are erased
static void showLine(uint8_t row, const char *text) {
    char padded[24];
    snprintf(padded, sizeof(padded), "%-21s", text);
    OLED_Print(row, 0, padded);
}

// Triggers the sensor, waits for the echo, then shows the result on the OLED and the serial monitor
static void takeMeasurement(void) {
    char line[64];

    echoDone = 0;
    HCSR04_Trigger();

    // Wait for the EXTI1 handler to finish timing the pulse, but never longer than the timeout
    uint32_t startMs = msTicks;
    while (!echoDone && (msTicks - startMs) < ECHO_TIMEOUT_MS) { }

    if (!echoDone) {
        showLine(2, "Pulse: no echo");
        showLine(3, "Dist: ---");
        uart3_send_string("No echo received\r\n");
        return;
    }

    // Convert the pulse width to a distance in the unit chosen by S1 (1 = inches, 0 = cm)
    uint32_t pw = pulseWidth;
    int inches = ((S1_PORT->IDR & (1U << S1_PIN)) != 0U);

    float distance = (float)pw / US_PER_CM;
    if (inches) {
        distance = distance / CM_PER_INCH;
    }

    // Split into whole and hundredths as integers, since printing floats with snprintf is unreliable here
    uint32_t hundredths = (uint32_t)(distance * 100.0f + 0.5f);
    const char *units = inches ? "inch" : "cm";
    unsigned long whole = hundredths / 100U;
    unsigned long frac  = hundredths % 100U;

    // Show the pulse width and distance on the OLED
    snprintf(line, sizeof(line), "Pulse: %lu us", (unsigned long)pw);
    showLine(2, line);
    snprintf(line, sizeof(line), "Dist: %lu.%02lu %s", whole, frac, units);
    showLine(3, line);

    // Send the same values to the serial monitor
    snprintf(line, sizeof(line), "Pulse: %lu us, Distance: %lu.%02lu %s\r\n",
             (unsigned long)pw, whole, frac, units);
    uart3_send_string(line);
}

int main(void) {
    FPU_Enable();

    // Same clock setup as Project 5: 16 MHz at reset, then 180 MHz
    SystemCoreClockUpdate();
    SystemClock_Config();
    SystemCoreClockUpdate();

    // Peripherals are set up after the clock change so the baud rate and timer values are correct
    OLED_Init();
    uart3_init();
    HCSR04_Init();
    S1_Init();
    EXTI_Init();

    OLED_Clear();
    OLED_Print(0, 0, "CPEG222 Project 6");
    OLED_Print(1, 0, "Ultrasonic Range");
    uart3_send_string("CPEG222 Project6 Ultrasonic Range Finder\r\n");

    SysTick_Init(); // start last so the first reading comes 0.5 s after everything is ready

    // Each time SysTick raises the flag, take one reading
    while (1) {
        if (measureFlag) {
            measureFlag = 0;
            takeMeasurement();
        }
    }
    return 0;
}