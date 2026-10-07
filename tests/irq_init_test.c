/* SPDX-License-Identifier: GPL-3.0-only */
/* Execute the real IRQ initialiser with RAM-backed MMIO on Linux.
 * This checks register writes, not JieLi instruction execution or hardware behaviour. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include "../firmware/hal/fm1_irq.h"
const char fm1_fatal_stubs[768] = {0};
static void fm1_fault(const fm1_crash_t *c) { (void)c; assert(!"unexpected fault handler"); }
static void map_page(uintptr_t address)
{
    void *result=mmap((void *)address,4096,PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(result==(void *)address);
}
int main(void)
{
    uint32_t i,initial[] = {0,1u<<2,0xffffffffu,0xa5a5a5a5u};
    map_page(0x01C7F000u); map_page(0x01EEF000u); map_page(0x01EEE000u);
    for(i=0;i<sizeof(initial)/sizeof(initial[0]);i++) {
        FM1_EMU_CON=initial[i]; FM1_ETM_CON=0x40u;
        fm1_irq_init();
        assert(FM1_EMU_CON==(initial[i]&~(1u<<2)));
        assert(FM1_ETM_CON==0x41u);
        assert((FM1_ICFG(1)&0xf0u)==0xf0u);
        assert(FM1_VEC[127]==(uint32_t)(uintptr_t)(fm1_fatal_stubs+6u*127u));
        fm1_irq_init(); assert(!(FM1_EMU_CON&(1u<<2)));
    }
    puts("irq init: div0 bit cleared from cold/warm states, unrelated bits preserved, ETM and vectors retained: PASS");
    return 0;
}
