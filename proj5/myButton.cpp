#include "myButton.h"
#include <stdint.h>

// ===== ATmega328P Port D register addresses (Pin 2 = PD2) =====
#define myPIND  (*(volatile uint8_t *)0x29)
#define myDDRD  (*(volatile uint8_t *)0x2A)
#define myPORTD (*(volatile uint8_t *)0x2B)

#define BTN_BIT     4
#define DEBOUNCE_MS 50UL

static uint8_t lastRawReading = 1;
static uint8_t stableState = 1;
static unsigned long lastDebounceTime = 0;

void myButtonInit(void) {
    myDDRD &= ~(1 << BTN_BIT);  // Configure PD2 (Arduino Pin 2) as input
    myPORTD |= (1 << BTN_BIT);  // Enable internal pull-up resistor
}

// Returns 1 once per debounced button press (falling edge), otherwise 0
unsigned long myButtonPressed(unsigned long currentMillis) {
    uint8_t rawReading = (myPIND & (1 << BTN_BIT)) ? 1 : 0; // use ternary to determin 1 or 0
    uint8_t pressedEvent = 0;

    if (rawReading != lastRawReading) {
        lastDebounceTime = currentMillis;
    }

    if ((currentMillis - lastDebounceTime) >= DEBOUNCE_MS) {
        if (rawReading != stableState) {
            stableState = rawReading;
            if (stableState == 0) {
                pressedEvent = 1;
            }
        }
    }
    lastRawReading = rawReading;
    return pressedEvent;
}