#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "myFan.h"
#include "myButton.h"
#include <string.h>

#define LED1 2
#define LED2 3
#define CHOOSE_STATEMENT "Choose an LED [1 or 2]: \n"
#define BUF_SIZE 16
#define BTN_PIN 4
#define MTR_PIN 9

// Shared LED rates (volatile since modified by task_serial and read by task_blink)
volatile unsigned int rate1 = 0;
volatile unsigned int rate2 = 0;

// Serial input state
int targetLED;
char inputBuffer[BUF_SIZE];
uint8_t bufInd = 0;
bool awaitingInterval = false;

// LED state tracking
unsigned long previousMillis1 = 0;
unsigned long previousMillis2 = 0;
int ledState1 = LOW;
int ledState2 = LOW;

// Motor state tracking
const char* speedStr[] = {"OFF", "LOW", "MED", "HIGH", "MED", "LOW"};
int speedInd = 0;

// Mutex to protect Serial access across tasks
SemaphoreHandle_t xSerialMutex;

// FreeRTOS Task Prototypes
void task_blink(void *pvParameters);
void task_serial(void *pvParameters);
void task_btn(void *pvParameters);

void setup() {
  // Note: Omit myInit() if it configures hardware timers that conflict with FreeRTOS
  myFanInit();
  myButtonInit();

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, ledState1);
  digitalWrite(LED2, ledState2);

  Serial.begin(9600);
  while (!Serial) { ; } // Wait for serial port to connect

  // Create Mutex for Serial port
  xSerialMutex = xSemaphoreCreateMutex();

  if (xSerialMutex != NULL) {
    Serial.println("\nSystem Ready");
    Serial.println(CHOOSE_STATEMENT);
    Serial.println("\nFan State: OFF");

    // Create FreeRTOS Tasks (Replaces TaskType Tasks[])
    // Parameters: TaskFunction, Name, StackSize (words), Parameters, Priority, Handle
    xTaskCreate(task_blink,  "Blink",  128, NULL, 2, NULL);
    xTaskCreate(task_btn,    "Button", 128, NULL, 2, NULL);
    xTaskCreate(task_serial, "Serial", 256, NULL, 1, NULL); // Larger stack for string/Serial handling

  }
}

void loop() {
  // Empty: FreeRTOS scheduler takes over execution
}

/*-----------------------------------------------------------*/
/*                         TASKS                             */
/*-----------------------------------------------------------*/

void task_blink(void *pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1; // 1ms interval (matches original Tasks[0])

  for (;;) {
    unsigned long currentMillis = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Check for the first LED
    unsigned int r1 = rate1;
    if (r1 > 0) {
      unsigned long interval1 = r1 / 2;
      if (currentMillis - previousMillis1 >= interval1) {
        previousMillis1 = currentMillis;
        ledState1 = !ledState1;
        digitalWrite(LED1, ledState1);
      }
    } else {
      ledState1 = LOW;
      digitalWrite(LED1, LOW);
    }

    // Check for the second LED
    unsigned int r2 = rate2;
    if (r2 > 0) {
      unsigned long interval2 = r2 / 2;
      if (currentMillis - previousMillis2 >= interval2) {
        previousMillis2 = currentMillis;
        ledState2 = !ledState2;
        digitalWrite(LED2, ledState2);
      }
    } else {
      ledState2 = LOW;
      digitalWrite(LED2, LOW);
    }

    // Precise periodic delay
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void task_btn(void *pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1; // 2ms interval (matches original Tasks[1])

  for (;;) {
    unsigned long currentMillis = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (myButtonPressed(currentMillis)) {
      myFan();

      if (speedInd == 5) {
        speedInd = 0;
      } else {
        speedInd++;
      }

      // Safely print to Serial
      if (xSemaphoreTake(xSerialMutex, portMAX_DELAY) == pdTRUE) {
        Serial.print("Current speed: ");
        Serial.println(speedStr[speedInd]);
        xSemaphoreGive(xSerialMutex);
      }
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void task_serial(void *pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1; // 5ms interval (matches original Tasks[2])

  for (;;) {
    while (Serial.available() > 0) {
      char ch = Serial.read();

      if (ch == '\n' || ch == '\r') {
        if (bufInd > 0) {
          inputBuffer[bufInd] = '\0';

          // Lock Serial before printing prompts/responses
          if (xSemaphoreTake(xSerialMutex, portMAX_DELAY) == pdTRUE) {
            if (!awaitingInterval) {
              targetLED = atoi(inputBuffer);
              if (targetLED == 1 || targetLED == 2) {
                Serial.println(targetLED);
                awaitingInterval = true;
                Serial.println("Choose an interval in ms:\n");
              } else {
                Serial.println("LED Invalid, try again\n");
                Serial.println(CHOOSE_STATEMENT);
              }
            } else {
              int interval = atoi(inputBuffer);
              Serial.println(interval);
              if (targetLED == 1) rate1 = interval;
              if (targetLED == 2) rate2 = interval;

              awaitingInterval = false;
              Serial.println(CHOOSE_STATEMENT);
            }
            xSemaphoreGive(xSerialMutex);
          }

          bufInd = 0;
        }
      } else {
        if (bufInd < BUF_SIZE - 1) {
          inputBuffer[bufInd++] = ch;
        }
      }
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
