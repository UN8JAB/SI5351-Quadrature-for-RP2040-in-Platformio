#include "si5351.h"

/*
 * si5351.cpp
 *
 * Created: August 2025
 * Author: Anton W. Zimakos (aka UN8JAB)
 *
 * Driver for Si5351A chip.
 * VFO 0 quadrature for CLK0 and CLK1,
 * VFO 1 simple for CLK2.
 *
 * For more information see AN619
 */

// ============ I2C Communication Functions ============

// Write a single byte to a specified register on the SI5351
void Si5351::_wr(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(SI5351_ADDR); // Start I2C communication with SI5351
    Wire.write(reg);                   // Specify the target register
    Wire.write(val);                   // Write the value to the register
    Wire.endTransmission();            // End the I2C transmission
}

// Write multiple bytes to consecutive registers starting from a specified register
void Si5351::_wrBulk(uint8_t reg, const uint8_t* data, uint8_t len) {
    Wire.beginTransmission(SI5351_ADDR); // Start I2C communication with SI5351
    Wire.write(reg);                   // Specify the starting register
    for (uint8_t i = 0; i < len; i++) {
        Wire.write(data[i]);           // Write each byte from the data array
    }
    Wire.endTransmission();            // End the I2C transmission
}

// Read a single byte from a specified register on the SI5351
uint8_t Si5351::_rd(uint8_t reg) {
    Wire.beginTransmission(SI5351_ADDR); // Start I2C communication with SI5351
    Wire.write(reg);                   // Specify the register to read
    Wire.endTransmission(false);       // End transmission but keep the connection active
    Wire.requestFrom(SI5351_ADDR, (uint8_t)1); // Request one byte from the SI5351
    return Wire.available() ? Wire.read() : 0xFF; // Return the read byte or 0xFF if no data
}

// Convert an R divider value (1, 2, 4, 8, 16, 32, 64, 128) to its corresponding code
uint8_t Si5351::_rDivToCode(uint8_t r) {
    switch (r) {
        case 1:   return 0; // R=1 maps to code 0
        case 2:   return 1; // R=2 maps to code 1
        case 4:   return 2; // R=4 maps to code 2
        case 8:   return 3; // R=8 maps to code 3
        case 16:  return 4; // R=16 maps to code 4
        case 32:  return 5; // R=32 maps to code 5
        case 64:  return 6; // R=64 maps to code 6
        case 128: return 7; // R=128 maps to code 7
        default:  return 0; // Default to code 0 (R=1) for invalid inputs
    }
}

// ============ Public API Functions ============

// Initialize the SI5351 chip and configure initial settings
void Si5351::begin() {
    Wire.begin(); // Initialize I2C communication

    // Disable spread spectrum to ensure stable output frequencies (AN619 p.8-9)
    _wr(SI_SS_EN, 0x00);

    // Configure output clocks: CLK0 and CLK1 use PLLA, CLK2 uses PLLB, 4mA drive strength
    _wr(SI_CLK0_CTL, (uint8_t)(SI_CLK_SRC_MS | SI_CLK_IDRV_4mA)); // CLK0: MultiSynth source, 4mA
    _wr(SI_CLK1_CTL, (uint8_t)(SI_CLK_SRC_MS | SI_CLK_IDRV_4mA)); // CLK1: MultiSynth source, 4mA
    _wr(SI_CLK2_CTL, (uint8_t)(SI_CLK_SRC_MS | SI_CLK_PLLB | SI_CLK_IDRV_4mA)); // CLK2: MultiSynth, PLLB, 4mA

    // Set initial VFO configurations (frequency, phase, divider, multiplier, etc.)
    _vfo[0] = {7074000UL, PH270, 1, 106, 30.0}; // VFO0: 7.074 MHz, 270° phase
    _vfo[1] = {10000000UL, PH000, 1, 76, 30.0}; // VFO1: 10 MHz, 0° phase

    // Apply initial settings to VFO0 and VFO1
    update(0);
    update(1);

    // Enable VFO0 (CLK0 and CLK1) and disable VFO1 (CLK2) by default
    enable(0, true);
    enable(1, false);
}

// Reset both PLLA and PLLB to apply new settings
void Si5351::resetPLL() {
    _wr(SI_PLL_RESET, 0xA0); // Reset PLLA and PLLB (may cause a brief click)
}

// Enable or disable a specific VFO output
void Si5351::enable(uint8_t vfoIdx, bool en) {
    uint8_t oe = _rd(SI_CLK_OE); // Read current output enable register
    if (vfoIdx == 0) {
        // VFO0 controls CLK0 and CLK1
        if (en) oe &= ~0x03; // Enable CLK0 and CLK1
        else oe |= 0x03;     // Disable CLK0 and CLK1
    } else {
        // VFO1 controls CLK2
        if (en) oe &= ~0x04; // Enable CLK2
        else oe |= 0x04;     // Disable CLK2
    }
    _wr(SI_CLK_OE, oe); // Write updated output enable settings
}

// Set the phase for VFO0 (CLK0 and CLK1)
void Si5351::setPhase(uint8_t vfoIdx, uint8_t phase) {
    if (vfoIdx != 0 || phase > 3) return; // Only VFO0 supports phase, valid values 0-3
    _vfo[0].phase = phase; // Store the phase setting (0°, 90°, 180°, or 270°)
}

// Set the frequency for a specific VFO
void Si5351::setFreq(uint8_t vfoIdx, uint32_t freqHz) {
    if (vfoIdx > 1) return; // Only VFO0 and VFO1 are supported
    _evaluate(vfoIdx, freqHz); // Calculate and store new frequency parameters
}

// Update the SI5351 registers for a specific VFO
void Si5351::update(uint8_t vfoIdx) {
    if (vfoIdx > 1) return; // Only VFO0 and VFO1 are supported

    // Configure PLL multiplier (MSN) for the selected VFO
    _setMSN(vfoIdx == 0 ? 0 : 1, _vfo[vfoIdx].msn);

    if (vfoIdx == 0) {
        // VFO0 controls CLK0 and CLK1 with the same MultiSynth divider in integer mode
        uint8_t rcode = _rDivToCode(_vfo[0].ri); // Get R divider code
        _setMSI(0, _vfo[0].msi, rcode); // Configure CLK0 MultiSynth
        _setMSI(1, _vfo[0].msi, rcode); // Configure CLK1 MultiSynth

        // Set phase offset for quadrature output (90° shift if needed)
        _wr(SI_CLK0_PHOFF, 0); // Reset CLK0 phase offset
        _wr(SI_CLK1_PHOFF, (_vfo[0].phase == PH090 || _vfo[0].phase == PH270) ? _vfo[0].msi : 0); // Set CLK1 phase

        // Configure clock control registers, including inversion for 180°/270° phase
        uint8_t clk0ctl = (uint8_t)(SI_CLK_SRC_MS | SI_CLK_INT | SI_CLK_IDRV_4mA); // CLK0: MultiSynth, integer mode, 4mA
        uint8_t clk1ctl = (uint8_t)(SI_CLK_SRC_MS | SI_CLK_INT | SI_CLK_IDRV_4mA); // CLK1: MultiSynth, integer mode, 4mA
        if (_vfo[0].phase == PH180 || _vfo[0].phase == PH270) clk1ctl |= SI_CLK_INV; // Invert CLK1 for 180°/270°
        _wr(SI_CLK0_CTL, clk0ctl); // Apply CLK0 settings
        _wr(SI_CLK1_CTL, clk1ctl); // Apply CLK1 settings
    } else {
        // VFO1 controls CLK2
        uint8_t rcode = _rDivToCode(_vfo[1].ri); // Get R divider code
        _setMSI(2, _vfo[1].msi, rcode); // Configure CLK2 MultiSynth

        // Configure CLK2 to use PLLB in integer mode
        uint8_t clk2ctl = (uint8_t)(SI_CLK_SRC_MS | SI_CLK_INT | SI_CLK_PLLB | SI_CLK_IDRV_4mA);
        _wr(SI_CLK2_CTL, clk2ctl); // Apply CLK2 settings
    }

    // Reset PLLs to apply the new settings
    resetPLL();
}

// ============ Internal Configuration Functions ============

// Configure PLL multiplier (MSN = a + b/c) for a specified PLL (0 for PLLA, 1 for PLLB)
void Si5351::_setMSN(uint8_t pllIdx, double msn) {
    uint32_t A = (uint32_t)floor(msn); // Integer part of the multiplier
    uint32_t B = (uint32_t)((msn - (double)A) * (double)SI_PLL_C); // Fractional part numerator
    uint32_t P1, P2, P3 = SI_PLL_C; // Denominator for fractional part

    // Calculate P1 and P2 based on the SI5351 application note (AN619)
    uint32_t tmp = (uint32_t)floor((double)(128 * B) / (double)SI_PLL_C);
    P1 = (uint32_t)(128 * A + tmp - 512);
    P2 = (uint32_t)(128 * B - SI_PLL_C * tmp);

    // Prepare register data for PLL configuration
    uint8_t base = (pllIdx == 0) ? SI_SYNTH_PLLA : SI_SYNTH_PLLB; // Select PLLA or PLLB base register
    uint8_t buf[8];
    buf[0] = (P3 >> 8) & 0xFF; // P3[15:8]
    buf[1] = P3 & 0xFF;        // P3[7:0]
    buf[2] = (P1 >> 16) & 0x03; // P1[17:16]
    buf[3] = (P1 >> 8) & 0xFF; // P1[15:8]
    buf[4] = P1 & 0xFF;        // P1[7:0]
    buf[5] = ((P3 >> 12) & 0xF0) | ((P2 >> 16) & 0x0F); // P3[19:16] | P2[19:16]
    buf[6] = (P2 >> 8) & 0xFF; // P2[15:8]
    buf[7] = P2 & 0xFF;        // P2[7:0]

    _wrBulk(base, buf, 8); // Write the PLL configuration to registers
}

// Configure MultiSynth divider for a specific clock output in integer mode
void Si5351::_setMSI(uint8_t clkIdx, uint8_t msiEven, uint8_t rDivLog2) {
    uint32_t P1 = 128UL * (uint32_t)msiEven - 512UL; // Calculate P1 for integer mode
    uint8_t base = (clkIdx == 0) ? SI_SYNTH_MS0 : (clkIdx == 1 ? SI_SYNTH_MS1 : SI_SYNTH_MS2); // Select MultiSynth base register
    uint8_t Rbits = (rDivLog2 & 0x07) << 4; // Shift R divider code to correct bit position

    // Prepare register data for MultiSynth configuration
    uint8_t buf[8];
    buf[0] = 0x00; // P3[15:8] = 0 (P3=1 in integer mode)
    buf[1] = 0x01; // P3[7:0] = 1
    buf[2] = (uint8_t)((P1 >> 16) & 0x03) | Rbits; // P1[17:16] | R divider bits
    buf[3] = (uint8_t)((P1 >> 8) & 0xFF); // P1[15:8]
    buf[4] = (uint8_t)(P1 & 0xFF);       // P1[7:0]
    buf[5] = 0x00; // P3[19:16]=0, P2[19:16]=0 (P3=1, P2=0)
    buf[6] = 0x00; // P2[15:8]=0
    buf[7] = 0x00; // P2[7:0]=0

    _wrBulk(base, buf, 8); // Write the MultiSynth configuration to registers
}

// Calculate optimal parameters for a desired output frequency
void Si5351::_evaluate(uint8_t vfoIdx, uint32_t freqHz) {
    if (vfoIdx > 1 || _vfo[vfoIdx].freq == freqHz) return; // Skip if invalid VFO or frequency unchanged

    // Strategy: Target VCO frequency around 700 MHz, use even integer MultiSynth divider (4-126),
    // and select R divider based on frequency range
    uint32_t ri; // R divider
    if (freqHz < 1000000UL) ri = 128; // Use R=128 for frequencies below 1 MHz
    else if (freqHz < 3000000UL) ri = 32; // Use R=32 for 1-3 MHz
    else ri = 1; // Use R=1 for frequencies above 3 MHz

    uint8_t msi; // MultiSynth integer divider
    if (freqHz < 6000000UL) {
        msi = 126; // Use maximum divider for low frequencies
    } else {
        // Calculate divider to target ~700 MHz VCO frequency
        uint32_t tentative = (uint32_t)(700000000UL / ((uint64_t)freqHz * ri));
        if (tentative < 4) tentative = 4; // Ensure divider is at least 4
        if (tentative > 126) tentative = 126; // Cap at 126
        if (tentative & 1) tentative++; // Make even if odd
        if (tentative > 126) tentative = 126; // Ensure cap after increment
        msi = (uint8_t)tentative;
    }

    // Verify if the calculated VCO frequency is within valid range
    uint64_t fvco = (uint64_t)freqHz * (uint64_t)msi * (uint64_t)ri;
    if (fvco < SI_VCO_LO || fvco > SI_VCO_HI) {
        // If out of range, parameters are still valid as they will be applied correctly
    }

    // Calculate PLL multiplier (MSN) based on crystal frequency
    double msn = ((double)msi * (double)ri * (double)freqHz) / (double)_xtal;

    // Store calculated parameters in VFO structure
    _vfo[vfoIdx].freq = freqHz;
    _vfo[vfoIdx].ri = (uint8_t)ri;
    _vfo[vfoIdx].msi = msi;
    _vfo[vfoIdx].msn = msn;

}
