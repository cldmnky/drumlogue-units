/*
 * Copyright (C) 2021, 2024 nukeykt
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <cmath>
#include "mcu.h"
#include "mcu_opcodes.h"
#include "mcu_interrupt.h"
#include "mcu_timer.h"
#include "pcm.h"
#include "lcd_stub.h"
#include "submcu.h"
#ifdef USE_LIBRESAMPLE
#include "resample/libresample.h"
#endif

#ifdef USE_NEON
#include "../../common/simd_utils.h"
#endif

// ============================================================================
// Audio Debug Utilities
// Define AUDIO_DEBUG to enable periodic audio pipeline diagnostics
// ============================================================================
#ifdef AUDIO_DEBUG
#include <float.h>

struct AudioStats {
    float min_val;
    float max_val;
    double sum;
    double sum_sq;
    int zero_count;
    int nan_count;
    int inf_count;
    int clip_count;   // |val| > 1.0
    int total;

    void reset() {
        min_val = FLT_MAX;
        max_val = -FLT_MAX;
        sum = 0.0;
        sum_sq = 0.0;
        zero_count = 0;
        nan_count = 0;
        inf_count = 0;
        clip_count = 0;
        total = 0;
    }

    void accumulate(float v) {
        total++;
        if (std::isnan(v)) { nan_count++; return; }
        if (std::isinf(v)) { inf_count++; return; }
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
        sum += v;
        sum_sq += (double)v * v;
        if (v == 0.0f) zero_count++;
        if (v > 1.0f || v < -1.0f) clip_count++;
    }

    void print(const char* label) const {
        if (total == 0) { fprintf(stderr, "  [%s] no samples\n", label); return; }
        double mean = sum / total;
        double rms = std::sqrt(sum_sq / total);
        double dc_offset = mean;
        double crest = (rms > 0.0) ? (double)(std::max(std::fabs((double)min_val), std::fabs((double)max_val))) / rms : 0.0;
        fprintf(stderr, "  [%s] n=%d min=%.6f max=%.6f mean=%.6f rms=%.6f dc=%.6f crest=%.1f\n",
                label, total, min_val, max_val, mean, rms, dc_offset, crest);
        if (zero_count > 0)
            fprintf(stderr, "         zeros=%d (%.1f%%)\n", zero_count, 100.0 * zero_count / total);
        if (nan_count > 0 || inf_count > 0 || clip_count > 0)
            fprintf(stderr, "         NaN=%d Inf=%d clip=%d\n", nan_count, inf_count, clip_count);
    }
};

static int g_audio_debug_counter = 0;
static constexpr int AUDIO_DEBUG_INTERVAL = 100; // Print every N calls
#endif // AUDIO_DEBUG

#if __linux__
#include <unistd.h>
#include <limits.h>
#endif

const char* rs_name[ROM_SET_COUNT] = {
    "SC-55mk2",
    "SC-55st",
    "SC-55mk1",
    "CM-300/SCC-1",
    "JV-880",
    "SCB-55",
    "RLP-3237",
    "SC-155",
    "SC-155mk2"
};

int romset = ROM_SET_MK2;

void MCU::MCU_ErrorTrap(void)
{
#ifdef DEBUG
    printf("trap %.2x %.4x\n", mcu.cp, mcu.pc);
#endif
}

uint8_t MCU::RCU_Read(void)
{
    return 0;
}

enum {
    ANALOG_LEVEL_RCU_LOW = 0,
    ANALOG_LEVEL_RCU_HIGH = 0,
    ANALOG_LEVEL_SW_0 = 0,
    ANALOG_LEVEL_SW_1 = 0x155,
    ANALOG_LEVEL_SW_2 = 0x2aa,
    ANALOG_LEVEL_SW_3 = 0x3ff,
    ANALOG_LEVEL_BATTERY = 0x2a0,
};

uint16_t MCU_SC155Sliders(uint32_t /* index*/)
{
    // 0 - 1/9
    // 1 - 2/10
    // 2 - 3/11
    // 3 - 4/12
    // 4 - 5/13
    // 5 - 6/14
    // 6 - 7/15
    // 7 - 8/16
    // 8 - ALL
    return 0x0;
}

uint32_t MCU::MCU_GetAddress(uint8_t page, uint16_t address) {
    return (page << 16) + address;
}

uint8_t MCU::MCU_ReadCode(void) {
    return MCU_Read(MCU_GetAddress(mcu.cp, mcu.pc));
}

uint8_t MCU::MCU_ReadCodeAdvance(void) {
    uint8_t ret = MCU_ReadCode();
    mcu.pc++;
    return ret;
}

void MCU::MCU_SetRegisterByte(uint8_t reg, uint8_t val)
{
    mcu.r[reg] = val;
}

uint32_t MCU::MCU_GetVectorAddress(uint32_t vector)
{
    return MCU_Read32(vector * 4);
}

uint32_t MCU::MCU_GetPageForRegister(uint32_t reg)
{
    if (reg >= 6)
        return mcu.tp;
    else if (reg >= 4)
        return mcu.ep;
    return mcu.dp;
}

void MCU::MCU_ControlRegisterWrite(uint32_t reg, uint32_t siz, uint32_t data)
{
    if (siz)
    {
        if (reg == 0)
        {
            mcu.sr = data;
            mcu.sr &= sr_mask;
        }
        else if (reg == 5) // FIXME: undocumented
        {
            mcu.dp = data & 0xff;
        }
        else if (reg == 4) // FIXME: undocumented
        {
            mcu.ep = data & 0xff;
        }
        else if (reg == 3) // FIXME: undocumented
        {
            mcu.br = data & 0xff;
        }
        else
        {
            MCU_ErrorTrap();
        }
    }
    else
    {
        if (reg == 1)
        {
            mcu.sr &= ~0xff;
            mcu.sr |= data & 0xff;
            mcu.sr &= sr_mask;
        }
        else if (reg == 3)
        {
            mcu.br = data;
        }
        else if (reg == 4)
        {
            mcu.ep = data;
        }
        else if (reg == 5)
        {
            mcu.dp = data;
        }
        else if (reg == 7)
        {
            mcu.tp = data;
        }
        else
        {
            MCU_ErrorTrap();
        }
    }
}

uint32_t MCU::MCU_ControlRegisterRead(uint32_t reg, uint32_t siz)
{
    uint32_t ret = 0;
    if (siz)
    {
        if (reg == 0)
        {
            ret = mcu.sr & sr_mask;
        }
        else if (reg == 5) // FIXME: undocumented
        {
            ret = mcu.dp | (mcu.dp << 8);
        }
        else if (reg == 4) // FIXME: undocumented
        {
            ret = mcu.ep | (mcu.ep << 8);
        }
        else if (reg == 3) // FIXME: undocumented
        {
            ret = mcu.br | (mcu.br << 8);;
        }
        else
        {
            MCU_ErrorTrap();
        }
        ret &= 0xffff;
    }
    else
    {
        if (reg == 1)
        {
            ret = mcu.sr & sr_mask;
        }
        else if (reg == 3)
        {
            ret = mcu.br;
        }
        else if (reg == 4)
        {
            ret = mcu.ep;
        }
        else if (reg == 5)
        {
            ret = mcu.dp;
        }
        else if (reg == 7)
        {
            ret = mcu.tp;
        }
        else
        {
            MCU_ErrorTrap();
        }
        ret &= 0xff;
    }
    return ret;
}

void MCU::MCU_SetStatus(uint32_t condition, uint32_t mask)
{
    if (condition)
        mcu.sr |= mask;
    else
        mcu.sr &= ~mask;
}

void MCU::MCU_PushStack(uint16_t data)
{
    if (mcu.r[7] & 1)
        MCU_Interrupt_Exception(this, EXCEPTION_SOURCE_ADDRESS_ERROR);
    mcu.r[7] -= 2;
    MCU_Write16(mcu.r[7], data);
}

uint16_t MCU::MCU_PopStack(void)
{
    uint16_t ret;
    if (mcu.r[7] & 1)
        MCU_Interrupt_Exception(this, EXCEPTION_SOURCE_ADDRESS_ERROR);
    ret = MCU_Read16(mcu.r[7]);
    mcu.r[7] += 2;
    return ret;
}

uint16_t MCU::MCU_AnalogReadPin(uint32_t pin)
{
    if (mcu_cm300)
        return 0;
    if (mcu_jv880)
    {
        if (pin == 1)
            return ANALOG_LEVEL_BATTERY;
        return 0x3ff;
    }
    if (0)
    {
READ_RCU:
        uint8_t rcu = RCU_Read();
        if (rcu & (1 << pin))
            return ANALOG_LEVEL_RCU_HIGH;
        else
            return ANALOG_LEVEL_RCU_LOW;
    }
    if (mcu_mk1)
    {
        if (mcu_sc155 && (dev_register[DEV_P9DR] & 1) != 0)
        {
            return MCU_SC155Sliders(pin);
        }
        if (pin == 7)
        {
            if (mcu_sc155 && (dev_register[DEV_P9DR] & 2) != 0)
                return MCU_SC155Sliders(8);
            else
                return ANALOG_LEVEL_BATTERY;
        }
        else
            goto READ_RCU;
    }
    else
    {
        if (mcu_sc155 && (io_sd & 16) != 0)
        {
            return MCU_SC155Sliders(pin);
        }
        if (pin == 7)
        {
            if (mcu_mk1)
                return ANALOG_LEVEL_BATTERY;
            switch ((io_sd >> 2) & 3)
            {
            case 0: // Battery voltage
                return ANALOG_LEVEL_BATTERY;
            case 1: // NC
                if (mcu_sc155)
                    return MCU_SC155Sliders(8);
                return 0;
            case 2: // SW
                switch (sw_pos)
                {
                case 0:
                default:
                    return ANALOG_LEVEL_SW_0;
                case 1:
                    return ANALOG_LEVEL_SW_1;
                case 2:
                    return ANALOG_LEVEL_SW_2;
                case 3:
                    return ANALOG_LEVEL_SW_3;
                }
            case 3: // RCU
                goto READ_RCU;
            }
        }
        else
            goto READ_RCU;
    }

    return 0;
}

void MCU::MCU_AnalogSample(int channel)
{
    int value = MCU_AnalogReadPin(channel);
    int dest = (channel << 1) & 6;
    dev_register[DEV_ADDRAH + dest] = value >> 2;
    dev_register[DEV_ADDRAL + dest] = (value << 6) & 0xc0;
}

void MCU::MCU_DeviceWrite(uint32_t address, uint8_t data)
{
    address &= 0x7f;
    if (address >= 0x10 && address < 0x40)
    {
        mcu_timer.TIMER_Write(address, data);
        return;
    }
    if (address >= 0x50 && address < 0x55)
    {
        mcu_timer.TIMER2_Write(address, data);
        return;
    }
    switch (address)
    {
    case DEV_P1DDR: // P1DDR
        break;
    case DEV_P5DDR:
        break;
    case DEV_P6DDR:
        break;
    case DEV_P7DDR:
        break;
    case DEV_SCR:
        break;
    case DEV_WCR:
        break;
    case DEV_P9DDR:
        break;
    case DEV_RAME: // RAME
        break;
    case DEV_P1CR: // P1CR
        break;
    case DEV_DTEA:
        break;
    case DEV_DTEB:
        break;
    case DEV_DTEC:
        break;
    case DEV_DTED:
        break;
    case DEV_SMR:
        break;
    case DEV_BRR:
        break;
    case DEV_IPRA:
        break;
    case DEV_IPRB:
        break;
    case DEV_IPRC:
        break;
    case DEV_IPRD:
        break;
    case DEV_PWM1_DTR:
        break;
    case DEV_PWM1_TCR:
        break;
    case DEV_PWM2_DTR:
        break;
    case DEV_PWM2_TCR:
        break;
    case DEV_PWM3_DTR:
        break;
    case DEV_PWM3_TCR:
        break;
    case DEV_P7DR:
        break;
    case DEV_TMR_TCNT:
        break;
    case DEV_TMR_TCR:
        break;
    case DEV_TMR_TCSR:
        break;
    case DEV_TMR_TCORA:
        break;
    case DEV_TDR:
        break;
    case DEV_ADCSR:
    {
        dev_register[address] &= ~0x7f;
        dev_register[address] |= data & 0x7f;
        if ((data & 0x80) == 0 && adf_rd)
        {
            dev_register[address] &= ~0x80;
            MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_ANALOG, 0);
        }
        if ((data & 0x40) == 0)
            MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_ANALOG, 0);
        return;
    }
    case DEV_SSR:
    {
        if ((data & 0x80) == 0 && (ssr_rd & 0x80) != 0)
        {
            dev_register[address] &= ~0x80;
            uart_tx_delay = mcu.cycles + 3000;
            MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_UART_TX, 0);
        }
        if ((data & 0x40) == 0 && (ssr_rd & 0x40) != 0)
        {
            uart_rx_delay = mcu.cycles + 3000;
            dev_register[address] &= ~0x40;
            MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_UART_RX, 0);
        }
        if ((data & 0x20) == 0 && (ssr_rd & 0x20) != 0)
        {
            dev_register[address] &= ~0x20;
        }
        if ((data & 0x10) == 0 && (ssr_rd & 0x10) != 0)
        {
            dev_register[address] &= ~0x10;
        }
        break;
    }
    default:
        address += 0;
        break;
    }
    dev_register[address] = data;
}

uint8_t MCU::MCU_DeviceRead(uint32_t address)
{
    address &= 0x7f;
    if (address >= 0x10 && address < 0x40)
    {
        return mcu_timer.TIMER_Read(address);
    }
    if (address >= 0x50 && address < 0x55)
    {
        return mcu_timer.TIMER_Read2(address);
    }
    switch (address)
    {
    case DEV_ADDRAH:
    case DEV_ADDRAL:
    case DEV_ADDRBH:
    case DEV_ADDRBL:
    case DEV_ADDRCH:
    case DEV_ADDRCL:
    case DEV_ADDRDH:
    case DEV_ADDRDL:
        return dev_register[address];
    case DEV_ADCSR:
        adf_rd = (dev_register[address] & 0x80) != 0;
        return dev_register[address];
    case DEV_SSR:
        ssr_rd = dev_register[address];
        return dev_register[address];
    case DEV_RDR:
        return uart_rx_byte;
    case 0x00:
        return 0xff;
    case DEV_P7DR:
    {
        if (!mcu_jv880) return 0xff;

        uint8_t data = 0xff;
        uint32_t button_pressed = mcu_button_pressed;

        if (io_sd == 0b11111011)
            data &= ((button_pressed >> 0) & 0b11111) ^ 0xFF;
        if (io_sd == 0b11110111)
            data &= ((button_pressed >> 5) & 0b11111) ^ 0xFF;
        if (io_sd == 0b11101111)
            data &= ((button_pressed >> 10) & 0b1111) ^ 0xFF;

        data |= 0b10000000;
        return data;
    }
    case DEV_P9DR:
    {
        int cfg = 0;
        if (!mcu_mk1)
            cfg = mcu_sc155 ? 0 : 2; // bit 1: 0 - SC-155mk2 (???), 1 - SC-55mk2

        int dir = dev_register[DEV_P9DDR];

        int val = cfg & (dir ^ 0xff);
        val |= dev_register[DEV_P9DR] & dir;
        return val;
    }
    case DEV_SCR:
        if (dev_register[address] == 0x30)
            midi_ready = true; // FIXME
        return dev_register[address];
    case DEV_TDR:
        return dev_register[address];
    case DEV_SMR:
        return dev_register[address];
    case DEV_IPRC:
    case DEV_IPRD:
    case DEV_DTEC:
    case DEV_DTED:
    case DEV_FRT2_TCSR:
    case DEV_FRT1_TCSR:
    case DEV_FRT1_TCR:
    case DEV_FRT1_FRCH:
    case DEV_FRT1_FRCL:
    case DEV_FRT3_TCSR:
    case DEV_FRT3_OCRAH:
    case DEV_FRT3_OCRAL:
        return dev_register[address];
    }
    return dev_register[address];
}

void MCU::MCU_DeviceReset(void)
{
    // dev_register[0x00] = 0x03;
    // dev_register[0x7c] = 0x87;
    dev_register[DEV_RAME] = 0x80;
    dev_register[DEV_SSR] = 0x80;
}

void MCU::MCU_UpdateAnalog(uint64_t cycles)
{
    int ctrl = dev_register[DEV_ADCSR];
    int isscan = (ctrl & 16) != 0;

    if (ctrl & 0x20)
    {
        if (analog_end_time == 0)
            analog_end_time = cycles + 200;
        else if (analog_end_time < cycles)
        {
            if (isscan)
            {
                int base = ctrl & 4;
                for (int i = 0; i <= (ctrl & 3); i++)
                    MCU_AnalogSample(base + i);
                analog_end_time = cycles + 200;
            }
            else
            {
                MCU_AnalogSample(ctrl & 7);
                dev_register[DEV_ADCSR] &= ~0x20;
                analog_end_time = 0;
            }
            dev_register[DEV_ADCSR] |= 0x80;
            if (ctrl & 0x40)
                MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_ANALOG, 1);
        }
    }
    else
        analog_end_time = 0;
}

uint8_t MCU::MCU_Read(uint32_t address)
{
    uint32_t address_rom = address & 0x3ffff;
    if (address & 0x80000 && !mcu_jv880)
        address_rom |= 0x40000;
    uint8_t page = (address >> 16) & 0xf;
    address &= 0xffff;
    uint8_t ret = 0xff;
    switch (page)
    {
    case 0:
        if (!(address & 0x8000))
            ret = rom1[address & 0x7fff];
        else
        {
            if (!mcu_mk1)
            {
                uint16_t base = mcu_jv880 ? 0xf000 : 0xe000;
                if (address >= base && address < (base | 0x400))
                {
                    ret = pcm.PCM_Read(address & 0x3f);
                }
                else if (!mcu_scb55 && address >= 0xec00 && address < 0xf000)
                {
                    ret = sub_mcu.SM_SysRead(address & 0xff);
                }
                else if (address >= 0xff80)
                {
                    ret = MCU_DeviceRead(address & 0x7f);
                }
                else if (address >= 0xfb80 && address < 0xff80
                    && (dev_register[DEV_RAME] & 0x80) != 0)
                    ret = ram[(address - 0xfb80) & 0x3ff];
                else if (address >= 0x8000 && address < 0xe000)
                {
                    ret = sram[address & 0x7fff];
                }
                else if (address == (base | 0x402))
                {
                    ret = ga_int_trigger;
                    ga_int_trigger = 0;
                    MCU_Interrupt_SetRequest(this, mcu_jv880 ? INTERRUPT_SOURCE_IRQ0 : INTERRUPT_SOURCE_IRQ1, 0);
                }
                else
                {
#ifdef DEBUG
                    printf("Unknown read %x\n", address);
#endif
                    ret = 0xff;
                }
                //
                // e402:2-0 irq source
                //
            }
            else
            {
                if (address >= 0xe000 && address < 0xe040)
                {
                    ret = pcm.PCM_Read(address & 0x3f);
                }
                else if (address >= 0xff80)
                {
                    ret = MCU_DeviceRead(address & 0x7f);
                }
                else if (address >= 0xfb80 && address < 0xff80
                    && (dev_register[DEV_RAME] & 0x80) != 0)
                {
                    ret = ram[(address - 0xfb80) & 0x3ff];
                }
                else if (address >= 0x8000 && address < 0xe000)
                {
                    ret = sram[address & 0x7fff];
                }
                else if (address >= 0xf000 && address < 0xf100)
                {
                    io_sd = address & 0xff;

                    if (mcu_cm300)
                        return 0xff;

                    lcd.LCD_Enable((io_sd & 8) != 0);

                    uint8_t data = 0xff;
                    uint32_t button_pressed = mcu_button_pressed;

                    if ((io_sd & 1) == 0)
                        data &= ((button_pressed >> 0) & 255) ^ 255;
                    if ((io_sd & 2) == 0)
                        data &= ((button_pressed >> 8) & 255) ^ 255;
                    if ((io_sd & 4) == 0)
                        data &= ((button_pressed >> 16) & 255) ^ 255;
                    if ((io_sd & 8) == 0)
                        data &= ((button_pressed >> 24) & 255) ^ 255;
                    return data;
                }
                else if (address == 0xf106)
                {
                    ret = ga_int_trigger;
                    ga_int_trigger = 0;
                    MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_IRQ1, 0);
                }
                else
                {
#ifdef DEBUG
                    printf("Unknown read %x\n", address);
#endif
                    ret = 0xff;
                }
                //
                // f106:2-0 irq source
                //
            }
        }
        break;
#if 0
    case 3:
        ret = rom2[address | 0x30000];
        break;
    case 4:
        ret = rom2[address];
        break;
    case 10:
        ret = rom2[address | 0x60000]; // FIXME
        break;
    case 1:
        ret = rom2[address | 0x10000];
        break;
#endif
    case 1:
        ret = rom2[address_rom & rom2_mask];
        break;
    case 2:
        ret = rom2[address_rom & rom2_mask];
        break;
    case 3:
        ret = rom2[address_rom & rom2_mask];
        break;
    case 4:
        ret = rom2[address_rom & rom2_mask];
        break;
    case 8:
        if (!mcu_jv880)
            ret = rom2[address_rom & rom2_mask];
        else
            ret = 0xff;
        break;
    case 9:
        if (!mcu_jv880)
            ret = rom2[address_rom & rom2_mask];
        else
            ret = 0xff;
        break;
    case 14:
    case 15:
        if (!mcu_jv880)
            ret = rom2[address_rom & rom2_mask];
        else
            ret = cardram[address & 0x7fff]; // FIXME
        break;
    case 10:
    case 11:
        if (!mcu_mk1)
            ret = sram[address & 0x7fff]; // FIXME
        else
            ret = 0xff;
        break;
    case 12:
    case 13:
        if (mcu_jv880)
            ret = nvram[address & 0x7fff]; // FIXME
        else
            ret = 0xff;
        break;
    case 5:
        if (mcu_mk1)
            ret = sram[address & 0x7fff]; // FIXME
        else
            ret = 0xff;
        break;
    default:
        ret = 0x00;
        break;
    }
    return ret;
}

uint16_t MCU::MCU_Read16(uint32_t address)
{
    address &= ~1;
    uint8_t b0, b1;
    b0 = MCU_Read(address);
    b1 = MCU_Read(address+1);
    return (b0 << 8) + b1;
}

uint32_t MCU::MCU_Read32(uint32_t address)
{
    address &= ~3;
    uint8_t b0, b1, b2, b3;
    b0 = MCU_Read(address);
    b1 = MCU_Read(address+1);
    b2 = MCU_Read(address+2);
    b3 = MCU_Read(address+3);
    return (b0 << 24) + (b1 << 16) + (b2 << 8) + b3;
}

void MCU::MCU_Write(uint32_t address, uint8_t value)
{
    uint8_t page = (address >> 16) & 0xf;
    address &= 0xffff;
    if (page == 0)
    {
        if (address & 0x8000)
        {
            if (!mcu_mk1)
            {
                uint16_t base = mcu_jv880 ? 0xf000 : 0xe000;
                if (address >= (base | 0x400) && address < (base | 0x800))
                {
                    if (address == (base | 0x404) || address == (base | 0x405))
                        lcd.LCD_Write(address & 1, value);
                    else if (address == (base | 0x401))
                    {
                        io_sd = value;
                        lcd.LCD_Enable((value & 1) == 0);
                    }
                    else if (address == (base | 0x402))
                        ga_int_enable = (value << 1);
                    else
                    {
#ifdef DEBUG
                        printf("Unknown write %x %x\n", address, value);
#endif
                    }
                    //
                    // e400: always 4?
                    // e401: SC0-6?
                    // e402: enable/disable IRQ?
                    // e403: always 1?
                    // e404: LCD
                    // e405: LCD
                    // e406: 0 or 40
                    // e407: 0, e406 continuation?
                    //
                }
                else if (address >= (base | 0x000) && address < (base | 0x400))
                {
                    pcm.PCM_Write(address & 0x3f, value);
                }
                else if (!mcu_scb55 && address >= 0xec00 && address < 0xf000)
                {
                    sub_mcu.SM_SysWrite(address & 0xff, value);
                }
                else if (address >= 0xff80)
                {
                    MCU_DeviceWrite(address & 0x7f, value);
                }
                else if (address >= 0xfb80 && address < 0xff80
                    && (dev_register[DEV_RAME] & 0x80) != 0)
                {
                    ram[(address - 0xfb80) & 0x3ff] = value;
                }
                else if (address >= 0x8000 && address < 0xe000)
                {
                    sram[address & 0x7fff] = value;
                }
                else
                {
#ifdef DEBUG
                    printf("Unknown write %x %x\n", address, value);
#endif
                }
            }
            else
            {
                if (address >= 0xe000 && address < 0xe040)
                {
                    pcm.PCM_Write(address & 0x3f, value);
                }
                else if (address >= 0xff80)
                {
                    MCU_DeviceWrite(address & 0x7f, value);
                }
                else if (address >= 0xfb80 && address < 0xff80
                    && (dev_register[DEV_RAME] & 0x80) != 0)
                {
                    ram[(address - 0xfb80) & 0x3ff] = value;
                }
                else if (address >= 0x8000 && address < 0xe000)
                {
                    sram[address & 0x7fff] = value;
                }
                else if (address >= 0xf000 && address < 0xf100)
                {
                    io_sd = address & 0xff;
                    lcd.LCD_Enable((io_sd & 8) != 0);
                }
                else if (address == 0xf105)
                {
                    lcd.LCD_Write(0, value);
                    ga_lcd_counter = 500;
                }
                else if (address == 0xf104)
                {
                    lcd.LCD_Write(1, value);
                    ga_lcd_counter = 500;
                }
                else if (address == 0xf107)
                {
                    io_sd = value;
                }
                else
                {
#ifdef DEBUG
                    printf("Unknown write %x %x\n", address, value);
#endif
                }
            }
        }
        else if (mcu_jv880 && address >= 0x6196 && address <= 0x6199)
        {
            // nop: the jv880 rom writes into the rom at 002E77-002E7D
        }
        else
        {
            // printf("Unknown write %x %x\n", address, value);
        }
    }
    else if (page == 5 && mcu_mk1)
    {
        sram[address & 0x7fff] = value; // FIXME
    }
    else if (page == 10 && !mcu_mk1)
    {
        sram[address & 0x7fff] = value; // FIXME
    }
    else if (page == 12 && mcu_jv880)
    {
        nvram[address & 0x7fff] = value; // FIXME
    }
    else if (page == 14 && mcu_jv880)
    {
        cardram[address & 0x7fff] = value; // FIXME
    }
    else
    {
#ifdef DEBUG
        printf("Unknown write %x %x\n", (page << 16) | address, value);
#endif
    }
}

void MCU::MCU_Write16(uint32_t address, uint16_t value)
{
    address &= ~1;
    MCU_Write(address, value >> 8);
    MCU_Write(address + 1, value & 0xff);
}

void MCU::MCU_ReadInstruction(void)
{
    // Tighten the hot instruction loop by avoiding extra helper calls.
    mcu_t& state = mcu;
    uint8_t operand = 0;
    if (__builtin_expect(state.cp == 0 && state.pc < 0x8000, 1))
    {
        // Fast path: ROM1 fetch for the common opcode stream.
        operand = rom1[state.pc];
        state.pc++;
    }
    else
    {
        const uint32_t address = (static_cast<uint32_t>(state.cp) << 16) | state.pc;
        operand = MCU_Read(address);
        state.pc++;
    }

    MCU_Operand_Table[operand](this, operand);

    if (state.sr & STATUS_T)
        MCU_Interrupt_Exception(this, EXCEPTION_SOURCE_TRACE);
}

void MCU::MCU_Init(void)
{
    memset(&mcu, 0, sizeof(mcu_t));
}

void MCU::MCU_Reset(void)
{
    mcu.r[0] = 0;
    mcu.r[1] = 0;
    mcu.r[2] = 0;
    mcu.r[3] = 0;
    mcu.r[4] = 0;
    mcu.r[5] = 0;
    mcu.r[6] = 0;
    mcu.r[7] = 0;

    mcu.pc = 0;

    mcu.sr = 0x700;

    mcu.cp = 0;
    mcu.dp = 0;
    mcu.ep = 0;
    mcu.tp = 0;
    mcu.br = 0;

    uint32_t reset_address = MCU_GetVectorAddress(VECTOR_RESET);
    mcu.cp = (reset_address >> 16) & 0xff;
    mcu.pc = reset_address & 0xffff;

    mcu.exception_pending = -1;

    MCU_DeviceReset();

    if (mcu_mk1)
    {
        ga_int_enable = 255;
    }
}

void MCU::MCU_PostUART(uint8_t data)
{
    if (!midi_ready) return;
    uart_buffer[uart_write_ptr] = data;
    uart_write_ptr = (uart_write_ptr + 1) % uart_buffer_size;
}

void MCU::MCU_UpdateUART_RX(void)
{
    if ((dev_register[DEV_SCR] & 16) == 0) // RX disabled
        return;
    if (uart_write_ptr == uart_read_ptr) // no byte
        return;

     if (dev_register[DEV_SSR] & 0x40)
         return;

    if (mcu.cycles < uart_rx_delay)
        return;

    uart_rx_byte = uart_buffer[uart_read_ptr];
    uart_read_ptr = (uart_read_ptr + 1) % uart_buffer_size;
    dev_register[DEV_SSR] |= 0x40;
    MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_UART_RX, (dev_register[DEV_SCR] & 0x40) != 0);
}

// dummy TX
void MCU::MCU_UpdateUART_TX(void)
{
    if ((dev_register[DEV_SCR] & 32) == 0) // TX disabled
        return;

    if (dev_register[DEV_SSR] & 0x80)
        return;

    if (mcu.cycles < uart_tx_delay)
        return;

    dev_register[DEV_SSR] |= 0x80;
    MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_UART_TX, (dev_register[DEV_SCR] & 0x80) != 0);

    // printf("tx:%x\n", dev_register[DEV_TDR]);
}

void MCU::MCU_PatchROM(void)
{
    //rom2[0x1333] = 0x11;
    //rom2[0x1334] = 0x19;
    //rom1[0x622d] = 0x19;

    rom2[0x318f7] = 0x19;
}

uint8_t MCU::MCU_ReadP0(void)
{
    return 0xff;
}

uint8_t MCU::MCU_ReadP1(void)
{
    uint8_t data = 0xff;
    uint32_t button_pressed = mcu_button_pressed;

    if ((mcu_p0_data & 1) == 0)
        data &= ((button_pressed >> 0) & 255) ^ 255;
    if ((mcu_p0_data & 2) == 0)
        data &= ((button_pressed >> 8) & 255) ^ 255;
    if ((mcu_p0_data & 4) == 0)
        data &= ((button_pressed >> 16) & 255) ^ 255;
    if ((mcu_p0_data & 8) == 0)
        data &= ((button_pressed >> 24) & 255) ^ 255;

    return data;
}

void MCU::MCU_WriteP0(uint8_t data)
{
    mcu_p0_data = data;
}

void MCU::MCU_WriteP1(uint8_t data)
{
    mcu_p1_data = data;
}

void unscramble(uint8_t *src, uint8_t *dst, int len)
{
    for (int i = 0; i < len; i++)
    {
        int address = i & ~0xfffff;
        static const int aa[] = {
            2, 0, 3, 4, 1, 9, 13, 10, 18, 17, 6, 15, 11, 16, 8, 5, 12, 7, 14, 19
        };
        for (int j = 0; j < 20; j++)
        {
            if (i & (1 << j))
                address |= 1<<aa[j];
        }
        uint8_t srcdata = src[address];
        uint8_t data = 0;
        static const int dd[] = {
            2, 0, 4, 5, 7, 6, 3, 1
        };
        for (int j = 0; j < 8; j++)
        {
            if (srcdata & (1 << dd[j]))
                data |= 1<<j;
        }
        dst[i] = data;
    }
}

void MCU::MCU_PostSample(int *sample)
{
    // JUCE ref: sample[0] / 2147483648.0 (double division)
    // We use float multiply by reciprocal (identical result for power-of-2)
    static constexpr float kInt32ToFloat = 1.0f / 2147483648.0f;
#ifdef AUDIO_DEBUG
    static int post_sample_count = 0;
    static int post_nonzero_count = 0;
    static int32_t post_raw_min = INT32_MAX;
    static int32_t post_raw_max = INT32_MIN;
    if (sample[0] != 0 || sample[1] != 0) post_nonzero_count++;
    if (sample[0] < post_raw_min) post_raw_min = sample[0];
    if (sample[0] > post_raw_max) post_raw_max = sample[0];
    if (sample[1] < post_raw_min) post_raw_min = sample[1];
    if (sample[1] > post_raw_max) post_raw_max = sample[1];
    post_sample_count++;
    if (post_sample_count % 64000 == 0) {
        float fmin = post_raw_min * kInt32ToFloat;
        float fmax = post_raw_max * kInt32ToFloat;
        fprintf(stderr, "[AUDIO_DEBUG] MCU_PostSample: %d samples, %d non-zero (%.1f%%), raw=[%d..%d] float=[%.6f..%.6f]\n",
                post_sample_count, post_nonzero_count,
                100.0 * post_nonzero_count / post_sample_count,
                post_raw_min, post_raw_max, fmin, fmax);
        fflush(stderr);
        // Reset periodic stats
        post_nonzero_count = 0;
        post_raw_min = INT32_MAX;
        post_raw_max = INT32_MIN;
    }
#endif
    sample_buffer_l[sample_write_ptr] = sample[0] * kInt32ToFloat;
    sample_buffer_r[sample_write_ptr] = sample[1] * kInt32ToFloat;
    // Optimization: Use bitmask instead of modulo (audio_buffer_size is power of 2)
    // Buffer resets to 0 each render, so this is safe and avoids division
    sample_write_ptr = (sample_write_ptr + 1) & (audio_buffer_size - 1);
}

void MCU::MCU_GA_SetGAInt(int line, int value)
{
    // guesswork
    if (value && !ga_int[line] && (ga_int_enable & (1 << line)) != 0)
        ga_int_trigger = line;
    ga_int[line] = value;

    if (mcu_jv880)
        MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_IRQ0, ga_int_trigger != 0);
    else
        MCU_Interrupt_SetRequest(this, INTERRUPT_SOURCE_IRQ1, ga_int_trigger != 0);
}

void MCU::MCU_EncoderTrigger(int dir)
{
    if (!mcu_jv880) return;
    MCU_GA_SetGAInt(dir == 0 ? 3 : 4, 0);
    MCU_GA_SetGAInt(dir == 0 ? 3 : 4, 1);
}

MCU::MCU() : pcm(this), lcd(this), mcu_timer(this), sub_mcu(this) {
    midiQueueCount = 0;
    wakeup_pending = 0;
}

MCU::~MCU() {
#ifdef USE_LIBRESAMPLE
    // Clean up libresample handles
    if (resampleL) {
        resample_close(resampleL);
        resampleL = 0;
    }
    if (resampleR) {
        resample_close(resampleR);
        resampleR = 0;
    }
#endif
}

int MCU::startSC55(const uint8_t* s_rom1, const uint8_t* s_rom2, const uint8_t* s_waverom1, const uint8_t* s_waverom2, const uint8_t* s_nvram)
{
    romset = ROM_SET_JV880;

    mcu_mk1 = false;
    mcu_cm300 = false;
    mcu_st = false;
    mcu_jv880 = false;
    mcu_scb55 = false;
    mcu_sc155 = false;
    switch (romset)
    {
        case ROM_SET_MK2:
        case ROM_SET_SC155MK2:
            if (romset == ROM_SET_SC155MK2)
                mcu_sc155 = true;
            break;
        case ROM_SET_ST:
            mcu_st = true;
            break;
        case ROM_SET_MK1:
        case ROM_SET_SC155:
            mcu_mk1 = true;
            mcu_st = false;
            if (romset == ROM_SET_SC155)
                mcu_sc155 = true;
            break;
        case ROM_SET_CM300:
            mcu_mk1 = true;
            mcu_cm300 = true;
            break;
        case ROM_SET_JV880:
            mcu_jv880 = true;
            rom2_mask /= 2; // rom is half the size
            lcd.lcd_width = 820;
            lcd.lcd_height = 100;
            break;
        case ROM_SET_SCB55:
        case ROM_SET_RLP3237:
            mcu_scb55 = true;
            break;
    }

    memset(&mcu, 0, sizeof(mcu_t));

    memcpy(rom1, s_rom1, ROM1_SIZE);
    memcpy(rom2, s_rom2, ROM2_SIZE_JV880);
    rom2_mask = ROM2_SIZE_JV880 - 1;
    memcpy(nvram, s_nvram, NVRAM_SIZE);

    // Assign waverom pointers (pre-unscrambled at build time, no const_cast needed)
    if (mcu_mk1)
    {
        pcm.waverom1 = s_waverom1;
        pcm.waverom2 = s_waverom2;
        // Note: waverom3 not used for mk1
    }
    else if (mcu_jv880)
    {
        pcm.waverom1 = s_waverom1;
        pcm.waverom2 = s_waverom2;
        // waverom_exp loaded separately in Init() after startSC55()
    }
    else
    {
        pcm.waverom1 = s_waverom1;
        pcm.waverom2 = s_waverom2;
    }

    SC55_Reset();

    // std::string nvramFilePath = *basePath + "/nvram.bin";
    // FILE *nvramFile = fopen(nvramFilePath.c_str(), "rb");
    // if (nvramFile) {
    //     fread(nvram, 1, 0x8000, nvramFile);
    //     fclose(nvramFile);
    // }

    sample_write_ptr = 0;

    return 0;
}

namespace {

static inline void MCU_Step(MCU& self, mcu_t& state) {
    if (!state.ex_ignore)
        MCU_Interrupt_Handle(&self);
    else
        state.ex_ignore = 0;

    if (!state.sleep)
        self.MCU_ReadInstruction();

    state.cycles += 12; // FIXME: assume 12 cycles per instruction

    self.pcm.PCM_Update(state.cycles);

    self.mcu_timer.TIMER_Clock(state.cycles);

    if (!self.mcu_mk1 && !self.mcu_jv880)
        self.sub_mcu.SM_Update(state.cycles);
    else {
        self.MCU_UpdateUART_RX();
        self.MCU_UpdateUART_TX();
    }

    self.MCU_UpdateAnalog(state.cycles);
}

// Sleep-optimized MCU step: same order as MCU_Step but skips
// MCU_Interrupt_Handle (~36-flag scan) and MCU_ReadInstruction.
// The wakeup_pending flag (set by TIMER_Clock/UART) causes the render
// loop to route through MCU_Step on the next iteration instead.
static inline void MCU_Step_Sleep(MCU& self, mcu_t& state) {
    state.cycles += 12;
    self.pcm.PCM_Update(state.cycles);
    self.mcu_timer.TIMER_Clock(state.cycles);
    self.MCU_UpdateUART_RX();
    self.MCU_UpdateUART_TX();
    self.MCU_UpdateAnalog(state.cycles);
}

static inline void MCU_UpdateSC55WithSampleRate(MCU& self, float *dataL, float *dataR,
                                               unsigned int nFrames, int destSampleRate) {
    // Resampling architecture:
    // 1. MCU runs internally at 64kHz, generating samples into sample_buffer_l/r
    // 2. We resample 64kHz -> destSampleRate (e.g., 48kHz) using interpolation
    //
    // Two paths:
    // - USE_LIBRESAMPLE: streaming sinc resampler with error accumulator (JUCE-compatible)
    // - else: persistent-phase linear interpolation (no malloc, embedded-safe)

    mcu_t& state = self.mcu;
    const double step = 64000.0 / static_cast<double>(destSampleRate);  // ~1.333 for 48kHz

#ifdef USE_LIBRESAMPLE
    // --- libresample path: use JUCE-style error accumulator ---
    double renderBufferFramesFloat = static_cast<double>(nFrames) / destSampleRate * 64000;
    int renderBufferFrames = static_cast<int>(std::ceil(renderBufferFramesFloat));
    double currentError = renderBufferFrames - renderBufferFramesFloat;

    int limit = static_cast<int>(nFrames) / 2;
    if (self.samplesError > limit) {
        renderBufferFrames -= limit;
        currentError -= limit;
    } else if (-self.samplesError > limit) {
        renderBufferFrames += limit;
        currentError += limit;
    }
#else
    // --- Persistent-phase path: compute exact needed input samples ---
    // resample_phase is the fractional offset into this call's generated buffer
    // We need enough input samples so that phase doesn't exceed the buffer
    double maxPhase = self.resample_phase + static_cast<double>(nFrames - 1) * step;
    int renderBufferFrames = static_cast<int>(std::floor(maxPhase)) + 2;  // +2 for interp margin
#endif

    if (renderBufferFrames > static_cast<int>(audio_buffer_size)) {
#ifdef DEBUG
        printf("Audio buffer size is too small. (%d requested)\n", renderBufferFrames);
        fflush(stdout);
#endif
        return;
    }

    // Reset write pointer to 0 each call - linear buffer
    self.sample_write_ptr = 0;

    int maxCycles = static_cast<int>(nFrames) * 256;

    for (int i = 0; self.sample_write_ptr < renderBufferFrames; i++) {
        if (i > maxCycles) {
#ifdef DEBUG
            printf("Not enough samples!\n");
            fflush(stdout);
#endif
            break;
        }

        for (int j = 0; j < self.midiQueueCount; j++) {
            if (!self.midiQueue[j].processed && self.midiQueue[j].samplePos <= self.sample_write_ptr) {
                self.postMidiSC55(self.midiQueue[j].data, self.midiQueue[j].length);
                self.midiQueue[j].processed = true;
            }
        }

        // Sleep fast-path: skip MCU_Interrupt_Handle (36-flag scan) and
        // MCU_ReadInstruction when the MCU is sleeping with no pending wakeup.
        // Saves ~10% of render budget during sleep-heavy periods.
        if (state.sleep && !self.wakeup_pending) {
            MCU_Step_Sleep(self, state);
        } else {
            if (state.sleep)
                self.wakeup_pending = 0;  // Will be re-set if interrupt fires
            MCU_Step(self, state);
        }
    }

#ifdef AUDIO_DEBUG
    g_audio_debug_counter++;
    if (g_audio_debug_counter % AUDIO_DEBUG_INTERVAL == 1) {
        AudioStats pre_l, pre_r;
        pre_l.reset(); pre_r.reset();
        for (int i = 0; i < self.sample_write_ptr; i++) {
            pre_l.accumulate(self.sample_buffer_l[i]);
            pre_r.accumulate(self.sample_buffer_r[i]);
        }
#ifdef USE_LIBRESAMPLE
        fprintf(stderr, "\n[AUDIO_DEBUG] === Call #%d === nFrames=%u destRate=%d renderBufFrames=%d written=%d error=%.4f\n",
                g_audio_debug_counter, nFrames, destSampleRate, renderBufferFrames, self.sample_write_ptr, self.samplesError);
#else
        fprintf(stderr, "\n[AUDIO_DEBUG] === Call #%d === nFrames=%u destRate=%d renderBufFrames=%d written=%d phase=%.6f\n",
                g_audio_debug_counter, nFrames, destSampleRate, renderBufferFrames, self.sample_write_ptr, self.resample_phase);
#endif
        pre_l.print("64kHz_L");
        pre_r.print("64kHz_R");

        fprintf(stderr, "  [64kHz_L first8]");
        for (int i = 0; i < 8 && i < self.sample_write_ptr; i++)
            fprintf(stderr, " %.6f", self.sample_buffer_l[i]);
        fprintf(stderr, "\n");
    }
#endif

    // ===== Resample 64kHz -> destSampleRate =====

#ifdef USE_LIBRESAMPLE
    // Streaming sinc resampler (matches JUCE plugin)
    const double ratio = static_cast<double>(destSampleRate) / 64000.0;
    if (self.savedDestSampleRate != destSampleRate) {
        self.savedDestSampleRate = destSampleRate;
        if (self.resampleL) resample_close(self.resampleL);
        if (self.resampleR) resample_close(self.resampleR);
        self.resampleL = resample_open(1, ratio, ratio);
        self.resampleR = resample_open(1, ratio, ratio);
        self.samplesError = 0.0;
    }

    int inUsedL = 0, inUsedR = 0;
    int outL = resample_process(self.resampleL, ratio, self.sample_buffer_l, renderBufferFrames,
                                0, &inUsedL, dataL, static_cast<int>(nFrames));
    int outR = resample_process(self.resampleR, ratio, self.sample_buffer_r, renderBufferFrames,
                                0, &inUsedR, dataR, static_cast<int>(nFrames));

    self.samplesError += currentError;
    if (inUsedL == 0 || inUsedR == 0) {
        self.samplesError = 0.0;
    }

    for (int i = outL; i < static_cast<int>(nFrames); ++i) dataL[i] = 0.0f;
    for (int i = outR; i < static_cast<int>(nFrames); ++i) dataR[i] = 0.0f;

#else
    // Persistent-phase linear interpolation resampler
    // - No malloc, no stack allocation (embedded-safe)
    // - resample_phase carries fractional position between calls
    // - No error accumulator needed (phase handles drift naturally)
    // - No periodic silence dropouts
    {
        double phase = self.resample_phase;
        const int inputLen = self.sample_write_ptr;  // actual samples generated
        const int outputLen = static_cast<int>(nFrames);
        int outCount = 0;

        for (int i = 0; i < outputLen; ++i) {
            const int idx = static_cast<int>(phase);
            if (idx + 1 >= inputLen) {
                // Should not happen with correct renderBufferFrames calculation
                break;
            }
            const float frac = static_cast<float>(phase - idx);
            dataL[i] = self.sample_buffer_l[idx] + frac * (self.sample_buffer_l[idx + 1] - self.sample_buffer_l[idx]);
            dataR[i] = self.sample_buffer_r[idx] + frac * (self.sample_buffer_r[idx + 1] - self.sample_buffer_r[idx]);
            phase += step;
            outCount++;
        }

        // Fill any remaining (shouldn't happen normally)
        for (int i = outCount; i < outputLen; ++i) {
            dataL[i] = 0.0f;
            dataR[i] = 0.0f;
        }

        // Update persistent phase: subtract consumed integer samples
        // Next call starts from the fractional remainder
        int consumed = static_cast<int>(std::floor(phase));
        self.resample_phase = phase - static_cast<double>(consumed);

#ifdef AUDIO_DEBUG
        if (g_audio_debug_counter % AUDIO_DEBUG_INTERVAL == 1) {
            fprintf(stderr, "  [Resample] step=%.6f outCount=%d/%d phase_in=%.6f phase_out=%.6f consumed=%d\n",
                    step, outCount, outputLen, self.resample_phase + consumed - phase + self.resample_phase, self.resample_phase, consumed);
            AudioStats out_l, out_r;
            out_l.reset(); out_r.reset();
            for (int j = 0; j < outputLen; j++) {
                out_l.accumulate(dataL[j]);
                out_r.accumulate(dataR[j]);
            }
            out_l.print("Output_L");
            out_r.print("Output_R");

            fprintf(stderr, "  [Output_L first8]");
            for (int j = 0; j < 8 && j < outputLen; j++)
                fprintf(stderr, " %.6f", dataL[j]);
            fprintf(stderr, "\n");
            fflush(stderr);
        }
#endif
    }
#endif

    // Flush the midi buffer
    for (int i = 0; i < self.midiQueueCount; i++) {
        if (!self.midiQueue[i].processed) {
            self.postMidiSC55(self.midiQueue[i].data, self.midiQueue[i].length);
            self.midiQueue[i].processed = true;
        }
    }
    self.midiQueueCount = 0;
}

}  // namespace

void MCU::updateSC55WithSampleRate(float *dataL, float *dataR, unsigned int nFrames, int destSampleRate) {
    MCU_UpdateSC55WithSampleRate(*this, dataL, dataR, nFrames, destSampleRate);
}

void MCU::SC55_Reset() {
    mcu_button_pressed = 0x00;
    mcu_p0_data = 0x00;
    mcu_p1_data = 0x00;
    memset(ga_int, 0x00, sizeof(ga_int));
    ga_int_enable = 0;
    ga_int_trigger = 0;
    ga_lcd_counter = 0;
    memset(ad_val, 0x00, sizeof(ga_int));
    ad_nibble = 0x00;
    sw_pos = 3;
    io_sd = 0x00;
    adf_rd = 0;
    analog_end_time = 0;
    ssr_rd = 0;
    midi_ready = false;
    uart_write_ptr = 0x00;
    uart_read_ptr = 0x00;
    memset(uart_buffer, 0x00, uart_buffer_size);
    uart_rx_byte = 0x00;
    uart_rx_delay = 0x00;
    uart_tx_delay = 0x00;
    memset(dev_register, 0, sizeof(dev_register));
    wakeup_pending = 0;

    lcd.LCD_Init();
    MCU_Init();
    MCU_PatchROM();
    MCU_Reset();
    sub_mcu.SM_Reset();
    pcm.PCM_Reset();
    mcu_timer.TIMER_Reset();

    sample_write_ptr = 0;
#ifndef USE_LIBRESAMPLE
    resample_phase = 0.0;
#endif
}

void MCU::postMidiSC55(const uint8_t* message, int length) {
    for (int i = 0; i < length; i++) {
        MCU_PostUART(message[i]);
    }
}

void MCU::enqueueMidiSC55(const uint8_t* message, int length, int samplePos) {
    if (length >= MIDI_EVENT_DATA_SIZE)
    {
        return;
    }

    MidiEvent event{};
    event.length = length;
    event.samplePos = samplePos;
    event.processed = false;
    memcpy(&event.data, message, length);
    
    if (midiQueueCount < MAX_MIDI_QUEUE) {
        midiQueue[midiQueueCount++] = event;
    }
}
