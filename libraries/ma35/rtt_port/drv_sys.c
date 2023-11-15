/**************************************************************************//**
*
* @copyright (C) 2020 Nuvoton Technology Corp. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*
* Change Logs:
* Date            Author           Notes
* 2020-11-11      Wayne            First version
*
******************************************************************************/

#include <rthw.h>
#include <rtthread.h>
#include "drv_sys.h"
#include <stdio.h>

#define LOG_TAG    "drv.sys"
#undef  DBG_ENABLE
#define DBG_SECTION_NAME   LOG_TAG
#define DBG_LEVEL      LOG_LVL_DBG
#define DBG_COLOR
#include <rtdbg.h>

#define DEF_RAISING_CPU_FREQUENCY

#if defined(BSP_USING_PMIC)
    //#define DEF_RAISING_CPU_FREQUENCY_1GHZ
    #define DEF_RAISING_CPU_VOLTAGE
#endif

void machine_shutdown(void)
{
    rt_kprintf("machine_shutdown...\n");
    rt_hw_interrupt_disable();

    /* Unlock */
    SYS_UnlockReg();

    while (1);
}

void machine_reset(void)
{
    rt_kprintf("machine_reset...\n");
    rt_hw_interrupt_disable();

    /* Unlock */
    SYS_UnlockReg();

    SYS->IPRST0 = SYS_IPRST0_CHIPRST_Msk;
    SYS->IPRST0 = 0;
    while (1);
}

int reboot(int argc, char **argv)
{
    machine_reset();
    return 0;
}
MSH_CMD_EXPORT(reboot, Reboot System);


void nu_sys_ip_reset(uint32_t u32ModuleIndex)
{
    SYS_ResetModule(u32ModuleIndex);
}

E_SYS_USB0_ID nu_sys_usb0_role(void)
{
#if 0
    /* Check Role on USB0 dual-role port. */
    /*
        [17] USB0_IDS
           USB0_ID Status
           0 = USB port 0 used as a USB device port.
           1 = USB port 0 used as a USB host port.
      */
    return ((inpw(REG_SYS_MISCISR) & (1 << 17)) > 0) ? USB0_ID_HOST : USB0_ID_DEVICE;
#else
    return USB0_ID_DEVICE;
#endif
}

void nu_sys_check_register(S_NU_REG *psNuReg)
{
    if (psNuReg == RT_NULL)
        return;

    while (psNuReg->vu32RegAddr != 0)
    {
        vu32 vc32RegValue = *((vu32 *)psNuReg->vu32RegAddr);
        vu32 vc32BMValue = vc32RegValue & psNuReg->vu32BitMask;
        LOG_I("[%3s] %32s(0x%08x) %24s(0x%08x): 0x%08x(AndBitMask:0x%08x)\n",
              (psNuReg->vu32Value == vc32BMValue) ? "Ok" : "!OK",
              psNuReg->szVName,
              psNuReg->vu32Value,
              psNuReg->szRegName,
              psNuReg->vu32RegAddr,
              vc32RegValue,
              vc32BMValue);
        psNuReg++;
    }
}

int nu_tempsen_get_value(double *pfTemperture)
{
    rt_tick_t begin_tick = rt_tick_get();

    SYS->TSENSRFCR &= ~SYS_TSENSRFCR_PD_Msk; // Disable power down, don't wait, takes double conv time (350ms * 2)

    // Wait valid bit set
    while ((SYS->TSENSRFCR & SYS_TSENSRFCR_DATAVALID_Msk) == 0)
    {
        // 700 ms after clear pd bit. other conversion takes 350 ms
        if (rt_tick_get() > (500 + begin_tick))
        {
            return -1;
        }
    }

    *pfTemperture = (double)((SYS->TSENSRFCR & 0x0FFF0000) >> 16) * 274.3531 / 4096.0 - 93.3332;

    // Clear Valid bit
    SYS->TSENSRFCR = SYS_TSENSRFCR_DATAVALID_Msk;

    return 0;
}

void nu_tempsen_hook(void)
{
    static rt_tick_t last_tick = 0;

    if ((last_tick + 5000) < rt_tick_get())
    {
        double temp;
        if (nu_tempsen_get_value(&temp) == 0)
        {
            char sztmp[32];
            snprintf(sztmp, sizeof(sztmp), "Temperature: %.1f\n", temp);
            LOG_I("%s", sztmp);
            last_tick = rt_tick_get();
        }
    }
}

static int nu_tempsen_go(void)
{
    rt_err_t err = rt_thread_idle_sethook(nu_tempsen_hook);

    if (err != RT_EOK)
    {
        LOG_E("set %s idle hook failed!\n", __func__);
        return -1;
    }

    return 0;
}
//INIT_APP_EXPORT(nu_tempsen_go);
MSH_CMD_EXPORT(nu_tempsen_go, go tempsen);

#define REG_SYS_CHIPCFG      (SYS_BASE + 0x1F4)

uint32_t nu_chipcfg_ddrsize(void)
{
    uint32_t u32ChipCfg = *((vu32 *)REG_SYS_CHIPCFG);

    return ((u32ChipCfg & 0xF0000) != 0) ? (1 << ((u32ChipCfg & 0xF0000) >> 16)) << 20 : 0;
}

static const char *szBootFromTypeName [] =
{
    "QSPI0_NOR",
    "QSPI0_NAND",
    "SD_eMMC0",
    "SD_eMMC1",
    "RAW_NAND",
    "USBD",
    "USBH0",
    "USBH1",
    "Invalid"
};

#define BOOTFROMTYPE_SIZE    (sizeof(szBootFromTypeName)/sizeof(char*))
void nu_sys_dump(void)
{
    LOG_I("Boot from: %s", szBootFromTypeName[nu_get_bootfrom_source()]);
}

void nu_chipcfg_dump(void)
{
    uint32_t u32ChipCfg = *((vu32 *)REG_SYS_CHIPCFG);
    uint32_t u32ChipCfg_DDRSize = ((u32ChipCfg & 0xF0000) != 0) ? 1 << ((u32ChipCfg & 0xF0000) >> 16) : 0;
    uint32_t u32ChipCfg_DDRType = ((u32ChipCfg & 0x8000) >> 15);

    LOG_I("CHIPCFG: 0x%08x ", u32ChipCfg);
    LOG_I("DDR SDRAM Size: %d MB", u32ChipCfg_DDRSize);
    LOG_I("MCP DDR TYPE: %s", u32ChipCfg_DDRSize ? (u32ChipCfg_DDRType ? "DDR2" : "DDR3/3L") : "Unknown");

    nu_sys_dump();
}

void nu_clock_dump(void)
{
    LOG_I("HXT: %d Hz", CLK_GetHXTFreq());
    LOG_I("LXT: %d Hz", CLK_GetLXTFreq());
    LOG_I("CAPLL: %d Hz(OpMode=%d)", CLK_GetPLLClockFreq(CAPLL),   CLK_GetPLLOpMode(CAPLL));
    LOG_I("DDRPLL: %d Hz(OpMode=%d)", CLK_GetPLLClockFreq(DDRPLL), CLK_GetPLLOpMode(DDRPLL));
    LOG_I("APLL: %d Hz(OpMode=%d)", CLK_GetPLLClockFreq(APLL),     CLK_GetPLLOpMode(APLL));
    LOG_I("EPLL: %d Hz(OpMode=%d)", CLK_GetPLLClockFreq(EPLL),     CLK_GetPLLOpMode(EPLL));
    LOG_I("VPLL: %d Hz(OpMode=%d)", CLK_GetPLLClockFreq(VPLL),     CLK_GetPLLOpMode(VPLL));

    LOG_I("M4-CPU: %d Hz", CLK_GetCPUFreq());
    LOG_I("SYSCLK0: %d Hz", CLK_GetSYSCLK0Freq());
    LOG_I("SYSCLK1: %d Hz", CLK_GetSYSCLK1Freq());
    LOG_I("HCLK0: %d Hz", CLK_GetHCLK0Freq());
    LOG_I("HCLK1: %d Hz", CLK_GetHCLK1Freq());
    LOG_I("HCLK2: %d Hz", CLK_GetHCLK2Freq());
    LOG_I("HCLK3: %d Hz", CLK_GetHCLK3Freq());
    LOG_I("PCLK0: %d Hz", CLK_GetPCLK0Freq());
    LOG_I("PCLK1: %d Hz", CLK_GetPCLK1Freq());
    LOG_I("PCLK2: %d Hz", CLK_GetPCLK2Freq());
    LOG_I("PCLK3: %d Hz", CLK_GetPCLK3Freq());
    LOG_I("PCLK4: %d Hz", CLK_GetPCLK4Freq());
}


static const char *szClockName [] =
{
    "HXT",
    "LXT",
    "N/A",
    "LIRC",
    "HIRC",
    "N/A",
    "CAPLL",
    "N/A",
    "DDRPLL",
    "EPLL",
    "APLL",
    "VPLL"
};
#define CLOCKNAME_SIZE    (sizeof(szClockName)/sizeof(char*))

void nu_clock_isready(void)
{
    uint32_t u32IsReady, i;
    for (i = 0; i < CLOCKNAME_SIZE; i++)
    {
        if (i == 5 || i == 7 || i == 2) continue;
        u32IsReady = CLK_WaitClockReady(1 << i);
        LOG_I("%s: %s\n", szClockName[i], (u32IsReady == 1) ? "[Stable]" : "[Unstable]");
    }
}

extern uint32_t ma35d1_set_cpu_voltage(uint32_t sys_clk, uint32_t u32Vol);
void nu_clock_raise(void)
{
    uint32_t u32PllRefClk;

    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable HXT, LXT */
    CLK->PWRCTL |= (CLK_PWRCTL_HXTEN_Msk | CLK_PWRCTL_HIRCEN_Msk);
    if (CLK->STATUS & CLK_STATUS_HXTSTB_Msk)     // Check Ready
    {
        u32PllRefClk = __HXT;
    }
    else if (CLK->STATUS & CLK_STATUS_HIRCSTB_Msk)      // Check Ready
    {
        u32PllRefClk = __HIRC;  // HXT_CHECK_FAIL
    }
    else
    {
        return;
    }

    CLK_SetPLLFreq(APLL, PLL_OPMODE_INTEGER, u32PllRefClk, 144000000ul);
    CLK_SetPLLFreq(EPLL, PLL_OPMODE_INTEGER, u32PllRefClk, 500000000ul);

    /* Waiting clock ready */
    CLK_WaitClockReady(CLK_STATUS_APLLSTB_Msk | CLK_STATUS_EPLLSTB_Msk);

#if defined(DEF_RAISING_CPU_FREQUENCY)
    /* Switch clock source of CA35 to DDRPLL before raising CA-PLL */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_CA35CKSEL_Msk)) | CLK_CLKSEL0_CA35CKSEL_DDRPLL;
#if defined(DEF_RAISING_CPU_VOLTAGE) && defined(DEF_RAISING_CPU_FREQUENCY_1GHZ)
    if (ma35d1_set_cpu_voltage(CLK_GetPLLClockFreq(SYSPLL), 0x64))  //1.30v
    {
        CLK_SetPLLFreq(CAPLL, PLL_OPMODE_INTEGER, u32PllRefClk, 1000000000ul);
    }
    else
#endif
    {
#if defined(DEF_RAISING_CPU_VOLTAGE)
        ma35d1_set_cpu_voltage(CLK_GetPLLClockFreq(SYSPLL), 0x60);  //1.26v
#endif
        CLK_SetPLLFreq(CAPLL, PLL_OPMODE_INTEGER, u32PllRefClk, 800000000ul);
    }

    /* Waiting clock ready */
    CLK_WaitClockReady(CLK_STATUS_CAPLLSTB_Msk);

    /* Switch clock source of CA35 to CA-PLL after raising */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_CA35CKSEL_Msk)) | CLK_CLKSEL0_CA35CKSEL_CAPLL;
#endif

}

#ifdef FINSH_USING_MSH
    MSH_CMD_EXPORT(nu_clock_dump, Dump all clocks);
    MSH_CMD_EXPORT(nu_clock_raise, Raise clock);
    MSH_CMD_EXPORT(nu_clock_isready, Check PLL clocks);
#endif

E_POR_BOOTSRC nu_get_bootfrom_source(void)
{
    /* check power-on-setting */
    //  SYS_PWRONOTP.BTSRCSEL[11:10] or SYS_PWRONPIN.BTSRCSEL[3:2]
    //    00 = Boot from SPI Flash (Default).
    //    01 = Boot from SD/eMMC.
    //    10 = Boot from NAND Flash.
    //    11 = Boot from USB.

    // If BTSRCSEL = 00, the Boot from SPI Flash.
    //  SYS_PWRONOTP.MISCCFG[15:14] or SYS_PWRONPIN.MISCCFG[7:6]
    //    00 = SPI-NAND Flash with 1-bit mode booting (Default).
    //    10 = SPI-NOR Flash with 1-bit mode booting.

    uint32_t u32PWRON = (uint32_t)SYS->PWRONOTP;
    uint32_t u32BTSRCSEL, u32MISCCFG, u32NPAGESEL;
    E_POR_BOOTSRC ret;

    if ((u32PWRON & SYS_PWRONOTP_PWRONSRC_Msk) == SYS_PWRONOTP_PWRONSRC_Msk) // Using SYS_PWRONOTP
    {
        u32BTSRCSEL = (u32PWRON & SYS_PWRONOTP_BTSRCSEL_Msk) >> SYS_PWRONOTP_BTSRCSEL_Pos ;
        u32MISCCFG = (u32PWRON & SYS_PWRONOTP_MISCCFG_Msk) >> SYS_PWRONOTP_MISCCFG_Pos;
        u32NPAGESEL = (u32PWRON & SYS_PWRONOTP_NPAGESEL_Msk) >> SYS_PWRONOTP_NPAGESEL_Pos;
    }
    else
    {
        // Using SYS_PWRONPIN
        u32BTSRCSEL = (SYS->PWRONPIN & SYS_PWRONPIN_BTSRCSEL_Msk) >> SYS_PWRONPIN_BTSRCSEL_Pos ;
        u32MISCCFG = (SYS->PWRONPIN & SYS_PWRONPIN_MISCCFG_Msk) >> SYS_PWRONPIN_MISCCFG_Pos;
        u32NPAGESEL = (u32PWRON & SYS_PWRONPIN_NPAGESEL_Msk) >> SYS_PWRONPIN_NPAGESEL_Pos;
    }

    switch (u32BTSRCSEL)
    {
    case 0: // Boot from SPI Flash (Default).
        if (u32MISCCFG & 0x2)
        {
            ret = evBootFrom_QSPI0_NOR;
        }
        else
        {
            ret = evBootFrom_QSPI0_NAND;
        }
        break;

    case 1: // Boot from SD/eMMC.
        if (u32MISCCFG & 0x1)
        {
            ret = evBootFrom_SD_eMMC1;
        }
        else
        {
            ret = evBootFrom_SD_eMMC0;
        }

        break;

    case 2: // Boot from NAND Flash.
        ret = evBootFrom_RAW_NAND;
        break;

    case 3: // Boot from USB.
        if (u32NPAGESEL & 0x1)
        {
            if (u32NPAGESEL & 0x2)
            {
                ret = evBootFrom_USBH1;
            }
            else
            {
                ret = evBootFrom_USBH0;
            }
        }
        else
        {
            ret = evBootFrom_USBD;
        }
        break;

    default:
        break;
    }

    return ret;
}

void devmem(int argc, char *argv[])
{
    volatile unsigned int u32Addr;
    unsigned int value = 0, mode = 0;

    if (argc < 2 || argc > 3)
    {
        goto exit_devmem;
    }

    if (argc == 3)
    {
        if (sscanf(argv[2], "0x%x", &value) != 1)
            goto exit_devmem;
        mode = 1; //Write
    }

    if (sscanf(argv[1], "0x%x", &u32Addr) != 1)
        goto exit_devmem;
    else if (u32Addr & (4 - 1))
        goto exit_devmem;

    if (mode)
    {
        *((volatile uint32_t *)u32Addr) = value;
    }
    LOG_I("0x%08x\n", *((volatile uint32_t *)u32Addr));

    return;
exit_devmem:
    rt_kprintf("Read: devmem <physical address in hex>\n");
    rt_kprintf("Write: devmem <physical address in hex> <value in hex format>\n");
    return;
}
MSH_CMD_EXPORT(devmem, dump device registers);

void devmem2(int argc, char *argv[])
{
    volatile unsigned int u32Addr;
    unsigned int value = 0, word_count = 1;

    if (argc < 2 || argc > 3)
    {
        goto exit_devmem;
    }

    if (argc == 3)
    {
        if (sscanf(argv[2], "%d", &value) != 1)
            goto exit_devmem;
        word_count = value;
    }

    if (sscanf(argv[1], "0x%x", &u32Addr) != 1)
        goto exit_devmem;
    else if (u32Addr & (4 - 1))
        goto exit_devmem;

    if (word_count > 0)
    {
        LOG_HEX("devmem", 16, (void *)u32Addr, word_count * sizeof(rt_base_t));
    }

    return;

exit_devmem:
    rt_kprintf("devmem2: <physical address in hex> <count in dec>\n");
    return;
}
MSH_CMD_EXPORT(devmem2, dump device registers);

