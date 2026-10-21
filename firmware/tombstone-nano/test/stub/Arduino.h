#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t val);
unsigned long millis(void);

#endif
