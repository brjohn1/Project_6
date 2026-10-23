/*
* UART3.c - CPEG222 Project 6
* UART3 serial monitor output, based on the lecture 12 demo but with the baud rate set for 180 MHz.
*/
#include "UART3.h"

#define UART3_TX_PIN    8U
#define UART3_RX_PIN    9U
#define UART3_PORT      GPIOD
#define UART3_PCLK1_HZ  45000000U // APB1 runs at 45 MHz once the 180 MHz clock is set up
#define UART3_BAUD      115200U

// Enables clocks, switches PD8/PD9 to alternate function 7, and sets the baud rate
void uart3_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    UART3_PORT->MODER &= ~((0x3U << (UART3_TX_PIN * 2U)) | (0x3U << (UART3_RX_PIN * 2U)));
    UART3_PORT->MODER |=  ((0x2U << (UART3_TX_PIN * 2U)) | (0x2U << (UART3_RX_PIN * 2U)));
    UART3_PORT->AFR[1] &= ~((0xFU << ((UART3_TX_PIN - 8U) * 4U)) | (0xFU << ((UART3_RX_PIN - 8U) * 4U)));
    UART3_PORT->AFR[1] |=  ((0x7U << ((UART3_TX_PIN - 8U) * 4U)) | (0x7U << ((UART3_RX_PIN - 8U) * 4U)));

    USART3->BRR = (UART3_PCLK1_HZ + (UART3_BAUD / 2U)) / UART3_BAUD; // 45 MHz / 115200 = 391
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

// Waits for the transmit register to be empty, then sends one character
void uart3_send_char(char c) {
    while (!(USART3->SR & USART_SR_TXE)) { }
    USART3->DR = (uint8_t)c;
}

// Sends every character in the string
void uart3_send_string(const char *s) {
    while (*s) {
        uart3_send_char(*s++);
    }
}