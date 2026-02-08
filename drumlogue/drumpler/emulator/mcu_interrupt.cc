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
#include "mcu.h"
#include "mcu_interrupt.h"

void MCU_Interrupt_Start(MCU* mcu, int32_t mask)
{
    mcu->MCU_PushStack(mcu->mcu.pc);
    mcu->MCU_PushStack(mcu->mcu.cp);
    mcu->MCU_PushStack(mcu->mcu.sr);
    mcu->mcu.sr &= ~STATUS_T;
    if (mask >= 0)
    {
        mcu->mcu.sr &= ~STATUS_INT_MASK;
        mcu->mcu.sr |= mask << 8;
    }
    mcu->mcu.sleep = 0;
}

void MCU_Interrupt_SetRequest(MCU* mcu, uint32_t interrupt, uint32_t value)
{
    mcu->mcu.interrupt_pending[interrupt] = value;
    if (value) {
        mcu->mcu.interrupt_pending_mask |= (1u << interrupt);
        mcu->wakeup_pending = 1;
    } else {
        mcu->mcu.interrupt_pending_mask &= ~(1u << interrupt);
    }
}

void MCU_Interrupt_Exception(MCU* mcu, uint32_t exception)
{
#if 0
    if (interrupt == INTERRUPT_SOURCE_IRQ0 && (mcu->dev_register[DEV_P1CR] & 0x20) == 0)
        return;
    if (interrupt == INTERRUPT_SOURCE_IRQ1 && (mcu->dev_register[DEV_P1CR] & 0x40) == 0)
        return;
#endif
    mcu->mcu.exception_pending = exception;
    mcu->wakeup_pending = 1;
}

void MCU_Interrupt_TRAPA(MCU* mcu, uint32_t vector)
{
    mcu->mcu.trapa_pending[vector] = 1;
    mcu->mcu.trapa_pending_mask |= (1u << vector);
    mcu->wakeup_pending = 1;
}

void MCU_Interrupt_StartVector(MCU* mcu, uint32_t vector, int32_t mask)
{
    uint32_t address = mcu->MCU_GetVectorAddress(vector);
    MCU_Interrupt_Start(mcu, mask);
    mcu->mcu.cp = address >> 16;
    mcu->mcu.pc = address;
}

// Static dispatch table: maps interrupt source index → (vector, dev_reg, shift).
// Entries with vector == 0 are unused/ICI sources (never dispatched).
// IRQ0/IRQ1 have special enable-bit checks handled inline.
struct InterruptDispatchEntry {
    uint16_t vector;
    uint8_t  dev_reg;
    uint8_t  shift;
};

static const InterruptDispatchEntry kInterruptDispatch[INTERRUPT_SOURCE_MAX] = {
    // [0] NMI — handled separately (highest priority, fixed mask=7)
    { VECTOR_NMI,                       0,        0 },
    // [1] IRQ0 — needs P1CR bit5 enable check
    { VECTOR_IRQ0,                      DEV_IPRA, 4 },
    // [2] IRQ1 — needs P1CR bit6 enable check
    { VECTOR_IRQ1,                      DEV_IPRA, 0 },
    // [3] FRT0_ICI — unused (no code sets this)
    { 0,                                0,        0 },
    // [4] FRT0_OCIA
    { VECTOR_INTERNAL_INTERRUPT_94,     DEV_IPRB, 4 },
    // [5] FRT0_OCIB
    { VECTOR_INTERNAL_INTERRUPT_98,     DEV_IPRB, 4 },
    // [6] FRT0_FOVI
    { VECTOR_INTERNAL_INTERRUPT_9C,     DEV_IPRB, 4 },
    // [7] FRT1_ICI — unused
    { 0,                                0,        0 },
    // [8] FRT1_OCIA
    { VECTOR_INTERNAL_INTERRUPT_A4,     DEV_IPRB, 0 },
    // [9] FRT1_OCIB
    { VECTOR_INTERNAL_INTERRUPT_A8,     DEV_IPRB, 0 },
    // [10] FRT1_FOVI
    { VECTOR_INTERNAL_INTERRUPT_AC,     DEV_IPRB, 0 },
    // [11] FRT2_ICI — unused
    { 0,                                0,        0 },
    // [12] FRT2_OCIA
    { VECTOR_INTERNAL_INTERRUPT_B4,     DEV_IPRC, 4 },
    // [13] FRT2_OCIB
    { VECTOR_INTERNAL_INTERRUPT_B8,     DEV_IPRC, 4 },
    // [14] FRT2_FOVI
    { VECTOR_INTERNAL_INTERRUPT_BC,     DEV_IPRC, 4 },
    // [15] TIMER_CMIA
    { VECTOR_INTERNAL_INTERRUPT_C0,     DEV_IPRC, 0 },
    // [16] TIMER_CMIB
    { VECTOR_INTERNAL_INTERRUPT_C4,     DEV_IPRC, 0 },
    // [17] TIMER_OVI
    { VECTOR_INTERNAL_INTERRUPT_C8,     DEV_IPRC, 0 },
    // [18] ANALOG
    { VECTOR_INTERNAL_INTERRUPT_E0,     DEV_IPRD, 0 },
    // [19] UART_RX
    { VECTOR_INTERNAL_INTERRUPT_D4,     DEV_IPRD, 4 },
    // [20] UART_TX
    { VECTOR_INTERNAL_INTERRUPT_D8,     DEV_IPRD, 4 },
};

void MCU_Interrupt_Handle(MCU* mcu)
{
    // ── Fast path: check bitmask mirrors before any scanning ──
    // trapa_pending_mask, exception_pending, and interrupt_pending_mask
    // together cover all possible interrupt sources. When all are clear,
    // we return immediately (~1 branch on ARM vs. scanning 36+ flags).
    if (!mcu->mcu.trapa_pending_mask
        && mcu->mcu.exception_pending < 0
        && !mcu->mcu.interrupt_pending_mask)
    {
        return;
    }

    // ── TRAPA: use CTZ to find first pending ──
    {
        uint32_t tmask = mcu->mcu.trapa_pending_mask;
        if (tmask)
        {
            int i = __builtin_ctz(tmask);
            mcu->mcu.trapa_pending[i] = 0;
            mcu->mcu.trapa_pending_mask &= ~(1u << i);
            MCU_Interrupt_StartVector(mcu, VECTOR_TRAPA_0 + i, -1);
            return;
        }
    }

    // ── Exceptions (address error, invalid instruction, trace) ──
    if (mcu->mcu.exception_pending >= 0)
    {
        switch (mcu->mcu.exception_pending)
        {
            case EXCEPTION_SOURCE_ADDRESS_ERROR:
                MCU_Interrupt_StartVector(mcu, VECTOR_ADDRESS_ERROR, -1);
                break;
            case EXCEPTION_SOURCE_INVALID_INSTRUCTION:
                MCU_Interrupt_StartVector(mcu, VECTOR_INVALID_INSTRUCTION, -1);
                break;
            case EXCEPTION_SOURCE_TRACE:
                MCU_Interrupt_StartVector(mcu, VECTOR_TRACE, -1);
                break;
        }
        mcu->mcu.exception_pending = -1;
        return;
    }

    // ── NMI: highest priority, fixed mask=7 ──
    if (mcu->mcu.interrupt_pending_mask & (1u << INTERRUPT_SOURCE_NMI))
    {
        MCU_Interrupt_StartVector(mcu, VECTOR_NMI, 7);
        return;
    }

    // ── Maskable interrupts: iterate only set bits via CTZ ──
    uint32_t sr_mask_level = (mcu->mcu.sr >> 8) & 7;
    // Skip NMI bit (already handled) and ICI bits (never dispatched)
    uint32_t pending = mcu->mcu.interrupt_pending_mask & ~(1u << INTERRUPT_SOURCE_NMI);
    while (pending)
    {
        int i = __builtin_ctz(pending);
        pending &= pending - 1;  // clear lowest set bit

        const InterruptDispatchEntry& d = kInterruptDispatch[i];
        if (!d.vector)
            continue;  // ICI sources — no dispatch

        // IRQ0/IRQ1 have additional enable-bit checks
        if (i == INTERRUPT_SOURCE_IRQ0) {
            if ((mcu->dev_register[DEV_P1CR] & 0x20) == 0)
                continue;
        } else if (i == INTERRUPT_SOURCE_IRQ1) {
            if ((mcu->dev_register[DEV_P1CR] & 0x40) == 0)
                continue;
        }

        int32_t level = (mcu->dev_register[d.dev_reg] >> d.shift) & 7;
        if ((int32_t)sr_mask_level < level)
        {
            MCU_Interrupt_StartVector(mcu, d.vector, level);
            return;
        }
    }
}
