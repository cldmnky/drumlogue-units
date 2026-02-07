/*
 * LCD stub for drumlogue (no display support)
 * Original LCD class from Nuked-SC55 simplified for embedded use
 */
#pragma once
#include <stdint.h>

struct MCU;

struct LCD {
    MCU *mcu;
    int lcd_width = 600;
    int lcd_height = 100;
    
    LCD(MCU *mcu) : mcu(mcu) {}
    
    // All LCD methods stubbed - drumlogue units have no display access
    void LCD_Init() {}
    void LCD_Enable(uint32_t enable) { (void)enable; }
    void LCD_Write(uint32_t address, uint8_t data) { (void)address; (void)data; }
    void LCD_Update(uint32_t cycles) { (void)cycles; }
    bool LCD_QuitRequested() { return false; }
};
