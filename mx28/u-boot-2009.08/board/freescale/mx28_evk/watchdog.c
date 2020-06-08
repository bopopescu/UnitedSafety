
#include <common.h>
#include <asm/arch/regs-pinctrl.h>
#include <asm/arch/pinctrl.h>
#include <asm/arch/regs-clkctrl.h>
#include <asm/arch/regs-ocotp.h>


#ifdef CONFIG_HW_WATCHDOG
void hw_watchdog_reset(void)
{
	debug("DEBUG: hw_watchdog_reset: kicking watchdog\n");
	REG_WR_ADDR(REGS_RTC_BASE + 0x50, 16000);
}

void hw_watchdog_disable(void)
{
	REG_CLR_ADDR(REGS_RTC_BASE + 0, 1 << 4);
	puts("Disabled HW watchdog until next reset\n");
}
#endif
