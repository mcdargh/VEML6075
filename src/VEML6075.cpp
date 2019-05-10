/*
 * VEML6075.cpp
 *
 * Arduino library for the Vishay VEML6075 UVA/UVB i2c sensor.
 *
 * Author: Sean Caulfield <sean@yak.net>
 * 
 * License: GPLv2.0
 *
 */

#include "VEML6075.h"

VEML6075::VEML6075()
{

    // Despite the datasheet saying this isn't the default on startup, it appears
    // like it is. So tell the thing to actually start gathering data.
    this->config = VEML6075_CONF_DEFAULT;

    // fix sensitivity for this integration time
    // given values were for 50ms, so halve it for 100ms
    // as it increases for integration time
    this->sensA = VEML6075_UVA_SENSITIVITY / 2.0;
    this->sensB = VEML6075_UVB_SENSITIVITY / 2.0;
}

bool VEML6075::begin(TwoWire *_i2c)
{

    this->i2c = _i2c;

    this->i2c->begin();
    if (this->getDevID() != VEML6075_DEVID)
    {
        return false;
    }

    // Write config to make sure device is enabled
    this->write16(VEML6075_REG_CONF, this->config);

    return true;
}

// Poll sensor for latest values and cache them
void VEML6075::poll()
{
    this->raw_uva = this->read16(VEML6075_REG_UVA);
    this->raw_uvb = this->read16(VEML6075_REG_UVB);
    this->raw_dark = this->read16(VEML6075_REG_DUMMY);
    this->raw_vis = this->read16(VEML6075_REG_UVCOMP1);
    this->raw_ir = this->read16(VEML6075_REG_UVCOMP2);
}

uint16_t VEML6075::getRawUVA()
{
    return this->raw_uva;
}

uint16_t VEML6075::getRawUVB()
{
    return this->raw_uvb;
}

uint16_t VEML6075::getRawDark()
{
    return this->raw_dark;
}

uint16_t VEML6075::getRawVisComp()
{
    return this->raw_vis;
}

uint16_t VEML6075::getRawIRComp()
{
    return this->raw_ir;
}

float VEML6075::getUVAIntensity()
{
    if (this->raw_uva == 0)
    {
        return 0.0;
    }
    return (float)this->raw_uva / this->sensA;
}

float VEML6075::getUVBIntensity()
{
    if (this->raw_uvb == 0)
    {
        return 0.0;
    }
    return (float)this->raw_uvb / this->sensB;
}

uint16_t VEML6075::getDevID()
{
    return this->read16(VEML6075_REG_DEVID);
}

float VEML6075::getUVA()
{
    //float comp_vis = this->raw_vis - this->raw_dark;
    //float comp_ir = this->raw_ir - this->raw_dark;
    //float comp_uva = this->raw_uva - this->raw_dark;
    float comp_vis;
    float comp_ir;
    float comp_uva;

    // Constrain compensation channels to positive values
    comp_ir = _max(this->raw_ir - this->raw_dark, 0);
    comp_vis = _max(this->raw_vis - this->raw_dark, 0);
    comp_uva = _max(this->raw_uva - this->raw_dark, 0);

    // Scale by coeffs from datasheet
    comp_vis *= VEML6075_UVI_UVA_VIS_COEFF;
    comp_ir *= VEML6075_UVI_UVA_IR_COEFF;

    // Subtract out visible and IR components
    comp_uva = _max(comp_uva - comp_ir, 0);
    comp_uva = _max(comp_uva - comp_vis, 0);

    return comp_uva;
}

float VEML6075::getUVB()
{
    //float comp_vis = this->raw_vis - this->raw_dark;
    //float comp_ir = this->raw_ir - this->raw_dark;
    //float comp_uvb = this->raw_uvb - this->raw_dark;
    float comp_vis;
    float comp_ir;
    float comp_uvb;

    // Constrain compensation channels to positive values
    comp_ir = _max(this->raw_ir - this->raw_dark, 0);
    comp_vis = _max(this->raw_vis - this->raw_dark, 0);
    comp_uvb = _max(this->raw_uvb - this->raw_dark, 0);

    // Scale by coeffs from datasheet
    comp_vis *= VEML6075_UVI_UVB_VIS_COEFF;
    comp_ir *= VEML6075_UVI_UVB_IR_COEFF;

    // Subtract out visible and IR components
    comp_uvb = _max(comp_uvb - comp_ir, 0);
    comp_uvb = _max(comp_uvb - comp_vis, 0);

    return comp_uvb;
}

float VEML6075::getUVIndex()
{
    float uva_weighted = this->getUVA() * VEML6075_UVI_UVA_RESPONSE;
    float uvb_weighted = this->getUVB() * VEML6075_UVI_UVB_RESPONSE;
    return (uva_weighted + uvb_weighted) / 2.0;
}

uint16_t VEML6075::read16(uint8_t reg)
{
    uint8_t msb = 0;
    uint8_t lsb = 0;

    this->i2c->beginTransmission(VEML6075_ADDR);
    this->i2c->write(reg);
    this->i2c->endTransmission(false);

    this->i2c->requestFrom(VEML6075_ADDR, 2, 1);
    lsb = this->i2c->read();
    msb = this->i2c->read();

    return (msb << 8) | lsb;
}

void VEML6075::write16(uint8_t reg, uint16_t data)
{
    this->i2c->beginTransmission(VEML6075_ADDR);
    this->i2c->write(reg);
    this->i2c->write((uint8_t)(0xFF & (data >> 0))); // LSB
    this->i2c->write((uint8_t)(0xFF & (data >> 8))); // MSB
    this->i2c->endTransmission();
}