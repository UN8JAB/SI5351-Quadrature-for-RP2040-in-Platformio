#include <Arduino.h>
#include "si5351.h"

Si5351 vfo(25000000UL); // Crystal is 25 MHz

void setup() {
    Serial.begin(57600);
    Serial.println("Serial started");

    // Initialization Si5351
    vfo.begin(); // Start VFO
    Serial.println("Si5351 begin");
    vfo.resetPLL();
    Serial.println("PLL Reset done");

    // VFO0 (quadrature for CLK0 and CLK1)
    vfo.setFreq(0, 7074000);
    vfo.setPhase(0, PH090);  // 90 deg between CLK0 CLK1
    vfo.enable(0, true);
    vfo.update(0);

    // VFO1 simple for CLK2
    vfo.setFreq(1, 10000000);
    vfo.enable(1, true);
    vfo.update(1);

    Serial.println("Si5351 initialized");
}

void loop() {
    // yours code here
}