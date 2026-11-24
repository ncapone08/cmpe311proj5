#include "myFan.h"
#include <stdint.h>      // standard integer types only; no avr/io.h or avr/interrupt.h needed

// ===== Own register constants (ATmega328P data-memory addresses) =====
#define myPORTB (*(volatile uint8_t *)0x25)  
#define myDDRB (*(volatile uint8_t *)0x24)  
#define myTCCR1A (*(volatile uint8_t *)0x80)   // Timer/Counter1 Control Register A (COM1A/COM1B output mode, WGM11:10)
#define myTCCR1B (*(volatile uint8_t *)0x81)   // Timer/Counter1 Control Register B (WGM13:12, clock select CS12:0)
#define myOCR1A  (*(volatile uint16_t *)0x88)   // Timer/Counter1 Output Compare Register A (16-bit, duty compare for OC1A)
#define myICR1  (*(volatile uint16_t *)0x86)   // Timer/Counter1 Input Capture Register (16-bit, used as TOP in mode 14)
#define clksel   0                             // bit position of the clock select field (CS12:0) in myTCCR1B

// pwm values
typedef enum{
  OFF = 0,
  LOW = 100,
  MED = 166,
  HIGH = 249,
} fanSpeed;

fanSpeed speeds[] = {OFF, LOW, MED, HIGH, MED, LOW};
uint8_t ind = 0;
fanSpeed currSpeed = speeds[ind];

// Sets up Timer/Counter1 for Fast PWM (mode 14) on OC1A (Arduino pin 9, PB1).
// 16 MHz / 8 = 2 MHz (0.5 us per count); counting 0 to 249 is 250 counts = 125 us = 8 kHz PWM.
void myFanInit() {
  myTCCR1A = 0;   // clear
  myTCCR1B = 0;   // clear
  myDDRB |= (0b1 << 1);
  myTCCR1A |= (0b10 << 0) | (0b10 << 6);   // WGM11:10 = 0b10 (low half of mode 14); COM1A1:0 = 0b10 (non-inverting: clear OC1A on compare match, set at BOTTOM)
  myICR1 = 0b0000000011111001;        // TOP = 249 (0x00F9): 250 counts per period, 2 MHz / 250 = 8 kHz PWM
  myOCR1A = OFF;
  myTCCR1B |= (0b11 << 3) | (0b010 << clksel);     // WGM13:12 = 0b11 (high half of mode 14, Fast PWM with TOP = ICR1); CS12:0 = 0b010 = divide by 8 (2 MHz timer clock); written last so the timer starts only after setup
}

static void applySpeed(fanSpeed speed) {
    if (speed == OFF) {
        myTCCR1A &= ~(0b11 << 6); // Disconnect OC1A for true 0% duty cycle
        myPORTB &= ~(0b1 << 1);   // Drive PB1 LOW
        myOCR1A = 0;
    } else {
        myOCR1A = speed;
        myTCCR1A |= (0b10 << 6);  // Enable non-inverting PWM on OC1A
    }
}

// this needs to cycle through the duty cycles
void myFan(void) {
    ind = (ind + 1) % 6;
    currSpeed = speeds[ind];
    applySpeed(currSpeed);
}












