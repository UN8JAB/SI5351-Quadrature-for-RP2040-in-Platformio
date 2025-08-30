#ifndef _SI5351_H_
#define _SI5351_H_
/*
 * si5351.h
 *
 * Created: August 2025
 * Author: Anton W. Zimakos (aka UN8JAB)
 *
 * Driver for Si5351A chip.
 * VFO 0 quadrature for CLK0 and CLK1,
 * VFO 1 simple for CLK2.
 *
 * VFO 0 set frequency and phase 0-90-180-270 deg
 * VFO 1 set frequency, phase is ignored
 *
 * To get smooth tuning, a suggested interval is 100ms
 *
 */

#include <Wire.h>

// Phase settings for quadrature output (CLK1 relative to CLK0)
#define PH000 0 // 0° phase shift
#define PH090 1 // 90° phase shift
#define PH180 2 // 180° phase shift
#define PH270 3 // 270° phase shift

// SI5351 register addresses
#define SI5351_ADDR     0x60 // I2C address of the SI5351 chip
#define SI_CLK_OE       3    // Output enable control register
#define SI_CLK0_CTL     16   // CLK0 control register
#define SI_CLK1_CTL     17   // CLK1 control register
#define SI_CLK2_CTL     18   // CLK2 control register
#define SI_SYNTH_PLLA   26   // PLLA synthesizer base register
#define SI_SYNTH_PLLB   34   // PLLB synthesizer base register
#define SI_SYNTH_MS0    42   // MultiSynth 0 base Piregister base address (CLK0)
#define SI_SYNTH_MS1    50   // MultiSynth 1 register base address (CLK1)
#define SI_SYNTH_MS2    58   // MultiSynth 2 register base address (CLK2)
#define SI_SS_EN        149  // Spread spectrum enable register
#define SI_CLK0_PHOFF   165  // CLK0 phase offset register
#define SI_CLK1_PHOFF   166  // CLK1 phase offset register
#define SI_CLK2_PHOFF   167  // CLK2 phase offset register
#define SI_PLL_RESET    177  // PLL reset register
#define SI_XTAL_LOAD    183  // Crystal load capacitance register

// Bit fields for CLKi_CTL registers
#define SI_CLK_INT      0b01000000 // Enable integer mode (required for integer MultiSynth divider)
#define SI_CLK_PLLB     0b00100000 // Select PLLB as clock source (0 = PLLA)
#define SI_CLK_INV      0b00010000 // Invert the clock output
#define SI_CLK_SRC_MS   0b00001100 // Select MultiSynth as clock source (otherwise XTAL)
#define SI_CLK_IDRV_4mA 0b00000001 // Set output drive strength to 4mA

// VCO/PLL frequency limits and fractional denominator
#define SI_VCO_LO       400000000UL // Minimum VCO frequency (400 MHz, relaxed from 600 MHz datasheet spec)
#define SI_VCO_HI       900000000UL // Maximum VCO frequency (900 MHz)
#define SI_PLL_C        1000000UL   // Denominator for PLL fractional multiplier (b/c)

// Structure to store VFO configuration
typedef struct {
    uint32_t freq;  // Target frequency in Hz
    uint8_t  phase; // Quadrature phase (0°, 90°, 180°, or 270°)
    uint8_t  ri;    // R divider value (1, 2, 4, 8, 16, 32, 64, 128)
    uint8_t  msi;   // MultiSynth integer divider (even, 4 to 126)
    double   msn;   // PLL multiplier (a + b/c)
} vfo_t;

class Si5351 {
public:
    // Constructor: Initialize with crystal frequency (default 25 MHz, can be customized)
    explicit Si5351(uint32_t xtalFreq = 25000000UL)
      : _xtal(xtalFreq) {}

    // Initialize I2C and configure the SI5351 chip
    void begin();

    // Reset both PLLA and PLLB
    void resetPLL();

    // Enable or disable a VFO (0 = CLK0+CLK1, 1 = CLK2)
    void enable(uint8_t vfoIdx, bool en);

    // Set phase for VFO0 (CLK1 relative to CLK0)
    void setPhase(uint8_t vfoIdx, uint8_t phase);

    // Set the desired frequency in Hz (registers updated by update())
    void setFreq(uint8_t vfoIdx, uint32_t freqHz);

    // Calculate and write all necessary registers for a VFO
    void update(uint8_t vfoIdx);

private:
    uint32_t _xtal; // Crystal frequency in Hz
    vfo_t _vfo[2];  // VFO configurations: 0 for CLK0/CLK1 (quadrature), 1 for CLK2

    // Low-level I2C communication functions
    void _wr(uint8_t reg, uint8_t val); // Write a single byte to a register
    void _wrBulk(uint8_t reg, const uint8_t* data, uint8_t len); // Write multiple bytes to consecutive registers
    uint8_t _rd(uint8_t reg); // Read a single byte from a register

    // PLL and MultiSynth configuration functions
    void _setMSN(uint8_t pllIdx, double msn); // Configure PLL multiplier
    void _setMSI(uint8_t clkIdx, uint8_t msiEven, uint8_t rDivLog2); // Configure MultiSynth divider

    // Calculate parameters for a target frequency
    void _evaluate(uint8_t vfoIdx, uint32_t freqHz);

    // Convert R divider value to its code (1, 2, 4, ..., 128 -> 0..7)
    static uint8_t _rDivToCode(uint8_t r);
};

#endif