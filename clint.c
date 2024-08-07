#include <stdint.h>
#include "device.h"
#include "riscv.h"
#include "riscv_private.h"

void clint_update_interrupts(hart_t *hart, clint_state_t *clint)
{
    uint64_t time_delta =
        clint->mtimecmp[hart->mhartid] - semu_timer_get(&hart->time);
    if ((int64_t) time_delta <= 0)
        hart->sip |= RV_INT_STI_BIT;
    else
        hart->sip &= ~RV_INT_STI_BIT;

    if (clint->msip[hart->mhartid]) {
        hart->sip |= RV_INT_SSI_BIT;
    } else
        hart->sip &= ~RV_INT_SSI_BIT;
}

static bool clint_reg_read(clint_state_t *clint, uint32_t addr, uint32_t *value)
{
    if (addr < 0x4000) {
        *value = clint->msip[addr >> 2];
        return true;
    }

    if (addr < 0xBFF8) {
        addr -= 0x4000;
        *value =
            (uint32_t) (clint->mtimecmp[addr >> 3] >> (32 & -!!(addr & 0b100)));
        return true;
    }

    if (addr < 0xBFFF) {
        *value = (uint32_t) (semu_timer_get(&clint->mtime) >>
                             (32 & -!!(addr & 0b100)));
        return true;
    }
    return false;
}

static bool clint_reg_write(clint_state_t *clint, uint32_t addr, uint32_t value)
{
    if (addr < 0x4000) {
        clint->msip[addr >> 2] = value;
        return true;
    }

    if (addr < 0xBFF8) {
        addr -= 0x4000;
        int32_t upper = clint->mtimecmp[addr >> 3] >> 32;
        int32_t lowwer = clint->mtimecmp[addr >> 3];
        if (addr & 0b100)
            upper = value;
        else
            lowwer = value;

        clint->mtimecmp[addr >> 3] = (uint64_t) upper << 32 | lowwer;
        return true;
    }

    if (addr < 0xBFFF) {
        int32_t upper = clint->mtime.begin >> 32;
        int32_t lower = clint->mtime.begin;
        if (addr & 0b100)
            upper = value;
        else
            lower = value;

        semu_timer_rebase(&clint->mtime, (uint64_t) upper << 32 | lower);
        return true;
    }
    return false;
}

void clint_read(hart_t *vm,
                clint_state_t *clint,
                uint32_t addr,
                uint8_t width,
                uint32_t *value)
{
    if (!clint_reg_read(clint, addr, value))
        vm_set_exception(vm, RV_EXC_LOAD_FAULT, vm->exc_val);
    *value = (*value) >> (RV_MEM_SW - width);
}

void clint_write(hart_t *vm,
                 clint_state_t *clint,
                 uint32_t addr,
                 uint8_t width,
                 uint32_t value)
{
    if (!clint_reg_write(clint, addr, value >> (RV_MEM_SW - width)))
        vm_set_exception(vm, RV_EXC_STORE_FAULT, vm->exc_val);
}
