#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#ifdef CONFIG_OF
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include "musb.h"
#include "musbfsh_core.h"
#include "musbfsh_mt65xx.h"
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif

#define FRA (48)
#define PARA (25)
bool musbfsh_power = false;

struct mt_usb11_glue {
	struct device *dev;
	struct platform_device *musbfsh;
};
static const struct of_device_id apusb_of_ids[] = {
	{.compatible = "mediatek,mt8163-usb11",},
	{},
};


void usb11_hs_slew_rate_cal(void)
{
	unsigned long data;
	unsigned long x;
	unsigned char value;
	unsigned long start_time, timeout;
	unsigned int timeout_flag = 0;
	/*4 s1:enable usb ring oscillator.*/
	USB11PHY_WRITE8(0x15, 0x80);

	/*4 s2:wait 1us.*/
	udelay(1);

	/*4 s3:enable free run clock*/
	USB11PHY_WRITE8(0xf00 - 0x900 + 0x11, 0x01);
	/*4 s4:setting cyclecnt*/
	USB11PHY_WRITE8(0xf00 - 0x900 + 0x01, 0x04);
	/*4 s5:enable frequency meter*/
	USB11PHY_SET8(0xf00 - 0x900 + 0x03, 0x05);

	/*4 s6:wait for frequency valid.*/
	start_time = jiffies;
	timeout = jiffies + 3 * HZ;
	while (!(USB11PHY_READ8(0xf00 - 0x900 + 0x10) & 0x1)) {
		if (time_after(jiffies, timeout)) {
			timeout_flag = 1;
			break;
	    }
	}

	/*4 s7: read result.*/
	if (timeout_flag) {
		INFO("[USBPHY] Slew Rate Calibration: Timeout\n");
		value = 0x4;
	} else {
	    data = USB11PHY_READ32(0xf00 - 0x900 + 0x0c);
	    x = ((1024 * FRA * PARA) / data);
	    value = (unsigned char)(x / 1000);
		if (((x - value * 1000) / 100) >= 5)
			value += 1;
	    /* INFO("[USB11PHY]slew calibration:FM_OUT =%d, x=%d,value=%d\n",data,x,value);*/
	}

	/*4 s8: disable Frequency and run clock.*/
	USB11PHY_CLR8(0xf00 - 0x900 + 0x03, 0x05);	/*disable frequency meter*/
	USB11PHY_CLR8(0xf00 - 0x900 + 0x11, 0x01);	/*disable free run clock*/

	/*4 s9: */
	USB11PHY_WRITE8(0x15, value << 4);

	/*4 s10:disable usb ring oscillator.*/
	USB11PHY_CLR8(0x15, 0x80);
}

void mt65xx_usb11_phy_poweron(void)
{
	INFO("mt65xx_usb11_phy_poweron++\r\n");
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
	/* enable_pll(UNIVPLL, "USB11"); */
	/*udelay(100); */
#if 0
	/* reverse preloader's sin @mt6575_usbphy.c */
	USB11PHY_CLR8(U1PHTCR2 + 3,
		force_usb11_avalid | force_usb11_bvalid | force_usb11_sessend |
		force_usb11_vbusvalid);
	USB11PHY_CLR8(U1PHTCR2 + 2,
		RG_USB11_AVALID | RG_USB11_BVALID | RG_USB11_SESSEND | RG_USB11_VBUSVALID);
	USB11PHY_CLR8(U1PHYCR1 + 2, force_usb11_en_fs_ls_rcv | force_usb11_en_fs_ls_tx);
	/**************************************/

	USB11PHY_SET8(U1PHYCR0 + 1, RG_USB11_FSLS_ENBGRI);

	USB11PHY_SET8(U1PHTCR2 + 3,
		force_usb11_avalid | force_usb11_sessend | force_usb11_vbusvalid);
	USB11PHY_SET8(U1PHTCR2 + 2, RG_USB11_AVALID | RG_USB11_VBUSVALID);
	USB11PHY_CLR8(U1PHTCR2 + 2, RG_USB11_SESSEND);
#endif


#if 0
	/*HQA LOOPBACK TEST. <FS>*/
	USB11PHY_CLR8(0x1a, 0x80);
	USB11PHY_CLR8(0x68, 0x08);

	USB11PHY_CLR8(0x68, 0x03);
	USB11PHY_SET8(0x68, 0x10);

	USB11PHY_SET8(0x68, 0x04);
	USB11PHY_CLR8(0x69, 0x03);
	USB11PHY_CLR8(0x69, 0x3C);

	USB11PHY_SET8(0x68, 0x80);

	USB11PHY_SET8(0x6a, 0x04);
	USB11PHY_SET8(0x6a, 0x01);
	USB11PHY_SET8(0x6a, 0x08);
	USB11PHY_SET8(0x6a, 0x02);

	USB11PHY_SET8(0x6a, 0x40);
	USB11PHY_SET8(0x6a, 0x80);
	USB11PHY_SET8(0x6a, 0x30);

	USB11PHY_SET8(0x68, 0x08);
	udelay(50);

	USB11PHY_SET8(0x63, 0x02);
	udelay(1);

	USB11PHY_SET8(0x63, 0x02);
	USB11PHY_SET8(0x63, 0x04);
	USB11PHY_CLR8(0x63, 0x08);

#endif



#if 0
	/*HQA LOOPBACK TEST. <HS>*/
	USB11PHY_CLR8(0x1a, 0x80);
	USB11PHY_CLR8(0x68, 0x08);

	USB11PHY_CLR8(0x68, 0x03);
	USB11PHY_CLR8(0x68, 0x30);

	USB11PHY_CLR8(0x68, 0x04);
	USB11PHY_CLR8(0x69, 0x03);
	USB11PHY_CLR8(0x69, 0x3C);

	USB11PHY_CLR8(0x68, 0xC0);

	USB11PHY_SET8(0x6a, 0x04);
	USB11PHY_SET8(0x6a, 0x01);
	USB11PHY_SET8(0x6a, 0x08);
	USB11PHY_SET8(0x6a, 0x02);

	USB11PHY_SET8(0x6a, 0x40);
	USB11PHY_SET8(0x6a, 0x80);
	USB11PHY_SET8(0x6a, 0x30);

	USB11PHY_SET8(0x68, 0x08);
	udelay(50);

	USB11PHY_SET8(0x63, 0x02);
	udelay(1);

	USB11PHY_SET8(0x63, 0x02);
	USB11PHY_SET8(0x63, 0x04);
	USB11PHY_CLR8(0x63, 0x08);

	usb11_hs_slew_rate_cal();

#endif

	udelay(50);

	USB11PHY_CLR8(0x6b, 0x04);
	USB11PHY_CLR8(0x6e, 0x01);

	USB11PHY_CLR8(0x1a, 0x80);

	/* remove in MT6588 ?????
	 USBPHY_CLR8(0x02, 0x7f);
	 USBPHY_SET8(0x02, 0x09);
	 USBPHY_CLR8(0x22, 0x03);
	*/

	USB11PHY_CLR8(0x6a, 0x04);
	/*USBPHY_SET8(0x1b, 0x08);*/

	/*force VBUS Valid          */
	USB11PHY_SET8(0x6C, 0x2C);
	USB11PHY_SET8(0x6D, 0x3C);
#if 1				/*joson*/
	/* VBUSVALID=0, AVALID=0, BVALID=0, SESSEND=1, IDDIG=X */
	USB11PHY_SET8(0x6c, 0x10);
	USB11PHY_CLR8(0x6c, 0x2e);
	USB11PHY_SET8(0x6d, 0x3e);

	/* wait */
	msleep(20);
	/* restart session */

	/* USB MAC ONand Host Mode */
	/* VBUSVALID=1, AVALID=1, BVALID=1, SESSEND=0, IDDIG=0 */
	USB11PHY_CLR8(0x6c, 0x10);
	USB11PHY_SET8(0x6c, 0x2c);
	USB11PHY_SET8(0x6d, 0x3e);
#endif
	udelay(800);

}


void mt65xx_usb11_phy_savecurrent(void)
{
	INFO("mt65xx_usb11_phy_savecurrent++\r\n");
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
#if 0
	USB11PHY_SET8(U1PHTCR2 + 3,
		force_usb11_avalid | force_usb11_sessend | force_usb11_vbusvalid);
	USB11PHY_CLR8(U1PHTCR2 + 2, RG_USB11_AVALID | RG_USB11_VBUSVALID);
	USB11PHY_SET8(U1PHTCR2 + 2, RG_USB11_SESSEND);

	USB11PHY_CLR8(U1PHYCR0 + 1, RG_USB11_FSLS_ENBGRI);

	USB11PHY_SET8(U1PHYCR1 + 2, force_usb11_en_fs_ls_rcv | force_usb11_en_fs_ls_tx);
	USB11PHY_CLR8(U1PHYCR1 + 3, RG_USB11_EN_FS_LS_RCV | RG_USB11_EN_FS_LS_TX);
#endif

	/*4 1. swtich to USB function. (system register, force ip into usb mode.*/
	USB11PHY_CLR8(0x6b, 0x04);
	USB11PHY_CLR8(0x6e, 0x01);

	/*4 2. release force suspendm.*/
	USB11PHY_CLR8(0x6a, 0x04);
	/*4 3. RG_DPPULLDOWN./RG_DMPULLDOWN.*/
	USB11PHY_SET8(0x68, 0xc0);
	/*4 4. RG_XCVRSEL[1:0] =2'b01.*/
	USB11PHY_CLR8(0x68, 0x30);
	USB11PHY_SET8(0x68, 0x10);
	/*4 5. RG_TERMSEL = 1'b1*/
	USB11PHY_SET8(0x68, 0x04);
	/*4 6. RG_DATAIN[3:0]=4'b0000*/
	USB11PHY_CLR8(0x69, 0x3c);
	/*4 7.force_dp_pulldown, force_dm_pulldown, force_xcversel,force_termsel.*/
	USB11PHY_SET8(0x6a, 0xba);

	/*4 8.RG_USB20_BC11_SW_EN 1'b0*/
	USB11PHY_CLR8(0x1a, 0x80);
	/*4 9.RG_USB20_OTG_VBUSSCMP_EN 1'b0*/
	USB11PHY_CLR8(0x1a, 0x10);
	/*4 10. delay 800us.*/
	udelay(800);
	/*4 11. rg_usb20_pll_stable = 1*/
	USB11PHY_SET8(0x63, 0x02);

	udelay(1);
	/*4 12.  force suspendm = 1.*/
	USB11PHY_SET8(0x6a, 0x04);

	USB11PHY_CLR8(0x6C, 0x2C);
	USB11PHY_SET8(0x6C, 0x10);
	USB11PHY_CLR8(0x6D, 0x3C);

	/*4 13.  wait 1us*/
	udelay(1);
}

void mt81xx_usb11_phy_recover(void)
{
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
	INFO("mt65xx_usb11_phy_recover++\r\n");
	/*4 1. turn on USB reference clock.     */
	/*enable_pll(UNIVPLL, "USB11"); */
	/*
	* swtich to USB function.
	* (system register, force ip into usb mode).
	*/
	USB11PHY_CLR8(0x6b, 0x04);
	USB11PHY_CLR8(0x6e, 0x01);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	USB11PHY_CLR8(0x1a, 0x80);

	/* RG_USB20_DP_100K_EN = 1'b0 */
	/* RG_USB20_DM_100K_EN = 1'b0 */
	USB11PHY_CLR8(0x22, 0x03);

	/* release force suspendm */
	USB11PHY_CLR8(0x6a, 0x04);

	udelay(800);

	/* force enter device mode */
	USB11PHY_CLR8(0x6c, 0x10);
	USB11PHY_SET8(0x6c, 0x2E);
	USB11PHY_SET8(0x6d, 0x3E);

	/* clean PUPD_BIST_EN */
	/* PUPD_BIST_EN = 1'b0 */
	/* PMIC will use it to detect charger type */
	/*USB11PHY_CLR8(0x1d, 0x10);*/

	/* force_uart_en = 1'b0 */
	USB11PHY_CLR8(0x6b, 0x04);
	/* RG_UART_EN = 1'b0 */
	USB11PHY_CLR8(0x6e, 0x01);
	/* force_uart_en = 1'b0 */
	USB11PHY_CLR8(0x6a, 0x04);

	USB11PHY_CLR8(0x68, 0xf4);
	USB11PHY_SET8(0x68, 0x08);

	/* RG_DATAIN[3:0] = 4'b0000 */
	USB11PHY_CLR8(0x69, 0x3c);

	USB11PHY_CLR8(0x6a, 0xba);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	USB11PHY_SET8(0x1a, 0x10);
	USB11PHY_CLR8(0x1a, 0x80);
	/* RG_USB20_OTG_VBUSSCMP_EN = 1'b1 */
	USB11PHY_SET8(0x1a, 0x10);

	udelay(800);

	/* force enter device mode */
	USB11PHY_CLR8(0x6c, 0x10);
	USB11PHY_SET8(0x6c, 0x2E);
	USB11PHY_SET8(0x6d, 0x3E);

	/* force enter host mode */
#if 1
	udelay(100);

	USB11PHY_SET8(0x6d, 0x3e);
	USB11PHY_SET8(0x6c, 0x10);
	USB11PHY_CLR8(0x6c, 0x2e);

	udelay(5);

	USB11PHY_CLR8(0x6c, 0x10);
	USB11PHY_SET8(0x6c, 0x2c);
#endif
	return;
	}

	void mt65xx_usb11_phy_recover(void)
	{
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
	INFO("mt65xx_usb11_phy_recover++\r\n");

#if 0
	USB11PHY_SET8(U1PHTCR2 + 3,
		force_usb11_avalid | force_usb11_sessend | force_usb11_vbusvalid);
	USB11PHY_SET8(U1PHTCR2 + 2, RG_USB11_AVALID | RG_USB11_VBUSVALID);
	USB11PHY_CLR8(U1PHTCR2 + 2, RG_USB11_SESSEND);

	USB11PHY_CLR8(U1PHYCR1 + 2, force_usb11_en_fs_ls_rcv | force_usb11_en_fs_ls_tx);
	USB11PHY_CLR8(U1PHYCR1 + 3, RG_USB11_EN_FS_LS_RCV | RG_USB11_EN_FS_LS_TX);

	USB11PHY_SET8(U1PHYCR0 + 1, RG_USB11_FSLS_ENBGRI);

	udelay(100);
#endif
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
#if 1
	USB11PHY_CLR8(0x6b, 0x04);
	USB11PHY_CLR8(0x6e, 0x01);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	USB11PHY_CLR8(0x1a, 0x80);

	/* RG_USB20_DP_100K_EN = 1'b0 */
	/* RG_USB20_DM_100K_EN = 1'b0 */
	USB11PHY_CLR8(0x22, 0x03);

	/* release force suspendm */
	USB11PHY_CLR8(0x6a, 0x04);

	udelay(800);

	/* force enter device mode */
	USB11PHY_CLR8(0x6c, 0x10);
	USB11PHY_SET8(0x6c, 0x2E);
	USB11PHY_SET8(0x6d, 0x3E);

	/* clean PUPD_BIST_EN */
	/* PUPD_BIST_EN = 1'b0 */
	/* PMIC will use it to detect charger type */
	USB11PHY_CLR8(0x1d, 0x10);

	/* force_uart_en = 1'b0 */
	USB11PHY_CLR8(0x6b, 0x04);
	/* RG_UART_EN = 1'b0 */
	USB11PHY_CLR8(0x6e, 0x01);
	/* force_uart_en = 1'b0 */
	USB11PHY_CLR8(0x6a, 0x04);

	USB11PHY_CLR8(0x68, 0xf4);

	/* RG_DATAIN[3:0] = 4'b0000 */
	USB11PHY_CLR8(0x69, 0x3c);

	USB11PHY_CLR8(0x6a, 0xba);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	USB11PHY_CLR8(0x1a, 0x80);
	/* RG_USB20_OTG_VBUSSCMP_EN = 1'b1 */
	USB11PHY_SET8(0x1a, 0x10);

	udelay(800);

	/* force enter device mode */
	USB11PHY_CLR8(0x6c, 0x10);
	USB11PHY_SET8(0x6c, 0x2E);
	USB11PHY_SET8(0x6d, 0x3E);
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
	/* force enter host mode */
#if 1
	udelay(100);

	USB11PHY_SET8(0x6d, 0x3e);
	USB11PHY_READ8(0x6d);
	USB11PHY_SET8(0x6c, 0x10);
	USB11PHY_CLR8(0x6c, 0x2e);

	udelay(5);

	USB11PHY_CLR8(0x6c, 0x10);
	USB11PHY_SET8(0x6c, 0x2c);
	USB11PHY_READ8(0x6c);
#endif
#endif
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
#if 0
	/*4 2. wait 50 usec.*/
	udelay(50);
	/*4 3. force_uart_en = 1'b0*/
	USB11PHY_CLR8(0x6b, 0x04);
	/*4 4. RG_UART_EN = 1'b0*/
	USB11PHY_CLR8(0x6e, 0x1);
	/*4 5. force_uart_en = 1'b0*/
	USB11PHY_CLR8(0x6a, 0x04);

	/*4 6. RG_DPPULLDOWN = 1'b0*/
	USB11PHY_CLR8(0x68, 0x40);
	/*4 7. RG_DMPULLDOWN = 1'b0*/
	USB11PHY_CLR8(0x68, 0x80);
	/*4 8. RG_XCVRSEL = 2'b00*/
	USB11PHY_CLR8(0x68, 0x30);
	/*4 9. RG_TERMSEL = 1'b0*/
	USB11PHY_CLR8(0x68, 0x04);
	/*4 10. RG_DATAIN[3:0] = 4'b0000    */
	USB11PHY_CLR8(0x69, 0x3c);

	/*4 11. force_dp_pulldown = 1b'0*/
	USB11PHY_CLR8(0x6a, 0x10);
	/*4 12. force_dm_pulldown = 1b'0*/
	USB11PHY_CLR8(0x6a, 0x20);
	/*4 13. force_xcversel = 1b'0*/
	USB11PHY_CLR8(0x6a, 0x08);
	/*4 14. force_termsel = 1b'0   */
	USB11PHY_CLR8(0x6a, 0x02);
	/*4 15. force_datain = 1b'0*/
	USB11PHY_CLR8(0x6a, 0x80);

	/*4 16. RG_USB20_BC11_SW_EN 1'b0*/
	USB11PHY_CLR8(0x1a, 0x80);
	/*4 17. RG_USB20_OTG_VBUSSCMP_EN 1'b1   */
	USB11PHY_SET8(0x1a, 0x10);

	USB11PHY_SET8(0x6C, 0x2C);
	USB11PHY_SET8(0x6D, 0x3C);
	/*<4> 18. wait 800 usec.*/
	udelay(800);

	usb11_hs_slew_rate_cal();
#endif
}


static bool clock_enabled;

void mt65xx_usb11_clock_enable(bool enable)
{
	if (enable) {
		if (clock_enabled) {	/*already enable*/
			INFO("[Flow][USB]already enable\r\n");
		} else {
#ifdef CONFIG_MTK_CLKMGR
			enable_clock(MT_CG_INFRA_USB, "INFRA_USB");
			enable_clock(MT_CG_INFRA_USB_MCU, "INFRA_USB_MCU");
			enable_clock(MT_CG_INFRA_ICUSB, "INFRA_ICUSB");
#else
			INFO("[Flow][USB]enable usb11 clock ++\r\n");
			clk_enable(usb11_pll_clk);
			clk_enable(usb11_mcu_clk);
			clk_enable(usb11_clk);
#endif
			clock_enabled = true;
		}
	} else {
		if (!clock_enabled)	{/*already disabled.*/
			INFO("[Flow][USB]already disabled\r\n");
		} else {
#ifdef CONFIG_MTK_CLKMGR
			disable_clock(MT_CG_INFRA_USB, "INFRA_USB");
			disable_clock(MT_CG_INFRA_USB_MCU, "INFRA_USB_MCU");
			enable_clock(MT_CG_INFRA_ICUSB, "INFRA_ICUSB");
#else
			INFO("[Flow][USB]disable usb11 clock --\r\n");
			clk_disable(usb11_pll_clk);
			clk_disable(usb11_mcu_clk);
			clk_disable(usb11_clk);
#endif
			clock_enabled = false;
		}
	}
}


void mt_usb11_poweron(struct musbfsh *musbfsh, int on)
{
	static bool recover;

	INFO("mt65xx_usb11_poweron++\r\n");
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
	if (on) {
		clk_enable(usb11_pll_clk);
		clk_enable(usb11_mcu_clk);
		clk_enable(usb11_clk);
		if (!recover) {
			INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
			mt65xx_usb11_phy_recover();
			recover = true;
		} else {
			INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
			mt65xx_usb11_phy_recover();
		}
		musbfsh_power = true;
	} else {
		mt65xx_usb11_phy_savecurrent();
		clk_disable(usb11_pll_clk);
		clk_disable(usb11_mcu_clk);
		clk_disable(usb11_clk);
		musbfsh_power = false;
	}
}


void mt_usb11_set_vbus(struct musbfsh *musbfsh, int is_on)
{
	INFO("is_on=%d\n", is_on);
#if 0
	mt_set_gpio_dir(GPIO_OTG_DRVVBUS_PIN, GPIO_DIR_OUT);
	if (1 == is_on) {
		if (oned)
			else {
				mt_set_gpio_out(GPIO_OTG_DRVVBUS_PIN, GPIO_OUT_ONE);
				oned = 1;
			}
	} else {
	if (!oned)
		else {
			mt_set_gpio_out(GPIO_OTG_DRVVBUS_PIN, GPIO_OUT_ZERO);
			oned = 0;
		}
	}
#endif
}

void musbfs_check_mpu_violation(u32 addr, int wr_vio)
{
	void __iomem *mregs = (void *)USB_BASE;

	INFO(KERN_CRIT "MUSB checks EMI MPU violation.\n");
	INFO(KERN_CRIT "addr = 0x%x, %s violation.\n", addr, wr_vio ? "Write" : "Read");
	INFO(KERN_CRIT "POWER = 0x%x,DEVCTL= 0x%x.\n", musbfsh_readb(mregs, MUSBFSH_POWER),
	musbfsh_readb((void __iomem *)USB11_BASE, MUSBFSH_DEVCTL));
	INFO(KERN_CRIT "DMA_CNTLch0 0x%04x,DMA_ADDRch0 0x%08x,DMA_COUNTch0 0x%08x\n",
	musbfsh_readw(mregs, 0x204), musbfsh_readl(mregs, 0x208), musbfsh_readl(mregs,
					0x20C));
	INFO(KERN_CRIT "DMA_CNTLch1 0x%04x,DMA_ADDRch1 0x%08x,DMA_COUNTch1 0x%08x\n",
	musbfsh_readw(mregs, 0x214), musbfsh_readl(mregs, 0x218), musbfsh_readl(mregs,
					0x21C));
	INFO(KERN_CRIT "DMA_CNTLch2 0x%04x,DMA_ADDRch2 0x%08x,DMA_COUNTch2 0x%08x\n",
	musbfsh_readw(mregs, 0x224), musbfsh_readl(mregs, 0x228), musbfsh_readl(mregs,
					0x22C));
	INFO(KERN_CRIT "DMA_CNTLch3 0x%04x,DMA_ADDRch3 0x%08x,DMA_COUNTch3 0x%08x\n",
	musbfsh_readw(mregs, 0x234), musbfsh_readl(mregs, 0x238), musbfsh_readl(mregs,
					0x23C));
	INFO(KERN_CRIT "DMA_CNTLch4 0x%04x,DMA_ADDRch4 0x%08x,DMA_COUNTch4 0x%08x\n",
	musbfsh_readw(mregs, 0x244), musbfsh_readl(mregs, 0x248), musbfsh_readl(mregs,
					0x24C));
	INFO(KERN_CRIT "DMA_CNTLch5 0x%04x,DMA_ADDRch5 0x%08x,DMA_COUNTch5 0x%08x\n",
	musbfsh_readw(mregs, 0x254), musbfsh_readl(mregs, 0x258), musbfsh_readl(mregs,
					0x25C));
	INFO(KERN_CRIT "DMA_CNTLch6 0x%04x,DMA_ADDRch6 0x%08x,DMA_COUNTch6 0x%08x\n",
	musbfsh_readw(mregs, 0x264), musbfsh_readl(mregs, 0x268), musbfsh_readl(mregs,
					0x26C));
	INFO(KERN_CRIT "DMA_CNTLch7 0x%04x,DMA_ADDRch7 0x%08x,DMA_COUNTch7 0x%08x\n",
	musbfsh_readw(mregs, 0x274), musbfsh_readl(mregs, 0x278), musbfsh_readl(mregs,
					0x27C));
}

int mt_usb11_init(struct musbfsh *musbfsh)
{
	INFO("++\n");
	if (!musbfsh) {
		ERR("musbfsh_platform_init,error,musbfsh is NULL");
		return -1;
	}

	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
	mt_usb11_poweron(musbfsh, true);
	return 0;
}

int mt_usb11_exit(struct musbfsh *musbfsh)
{
	INFO("++\n");
	mt_usb11_poweron(musbfsh, false);
	/* put it here because we can't shutdown PHY power during suspend */
	/* hwPowerDown(MT65XX_POWER_LDO_VUSB, "USB11");  */
	return 0;
}

void musbfsh_hcd_release(struct device *dev)
{
/*    INFO("musbfsh_hcd_release++,dev = 0x%08X.\n", (uint32_t)dev);*/
}

static const struct musbfsh_platform_ops mt_usb11_ops = {
	.init = mt_usb11_init,
	.exit = mt_usb11_exit,
	.set_vbus = mt_usb11_set_vbus,
	.set_power = mt_usb11_poweron,
};

static u64 mt_usb11_dmamask = DMA_BIT_MASK(32);

static int __init mt_usb11_probe(struct platform_device *pdev)
{
	struct musbfsh_hdrc_platform_data *pdata = pdev->dev.platform_data;
	struct platform_device *musbfsh;
	struct mt_usb11_glue *glue;
	struct musbfsh_hdrc_config *config;
	int ret = -ENOMEM;
	int musbfshid;
	struct device_node *np = pdev->dev.of_node;

	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
	glue = kzalloc(sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		dev_err(&pdev->dev, "failed to allocate glue context\n");
		goto err0;
	}

	/* get the musbfsh id */
	musbfshid = musbfsh_get_id(&pdev->dev, GFP_KERNEL);
	if (musbfshid < 0) {
		dev_err(&pdev->dev, "failed to allocate musbfsh id\n");
		ret = -ENOMEM;
		goto err1;
	}

	musbfsh = platform_device_alloc("musbfsh-hdrc", musbfshid);
	if (!musbfsh) {
		dev_err(&pdev->dev, "failed to allocate musb device\n");
		goto err2;
	}
#ifdef CONFIG_OF
	usb11_dts_np = pdev->dev.of_node;
	INFO("[usb11] usb11_dts_np %p\n", usb11_dts_np);
	/*usb_irq_number1 = irq_of_parse_and_map(pdev->dev.of_node, 0);
	usb_mac = (unsigned long)of_iomap(pdev->dev.of_node, 0);
	usb_phy_base = (unsigned long)of_iomap(pdev->dev.of_node, 1);*/
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ERR("failed to allocate musb platform data\n");
		goto err2;
	}

	config = devm_kzalloc(&pdev->dev, sizeof(*config), GFP_KERNEL);
	if (!config) {
		ERR("failed to allocate musb hdrc config\n");
		goto err2;
	}
	of_property_read_u32(np, "mode", (u32 *)&pdata->mode);

	/*of_property_read_u32(np, "dma_channels",    (u32 *)&config->dma_channels); */
	of_property_read_u32(np, "num_eps", (u32 *)&config->num_eps);
	config->multipoint = of_property_read_bool(np, "multipoint");
	/*
	config->dyn_fifo = of_property_read_bool(np, "dyn_fifo");
	config->soft_con = of_property_read_bool(np, "soft_con");
	config->dma = of_property_read_bool(np, "dma");
	*/

	pdata->config = config;
	INFO("[Flow][USB11]mode = %d ,num_eps = %d,multipoint = %d\n", pdata->mode,
	config->num_eps, config->multipoint);
#endif

	musbfsh->id = musbfshid;
	musbfsh->dev.parent = &pdev->dev;
	musbfsh->dev.dma_mask = &mt_usb11_dmamask;
	musbfsh->dev.coherent_dma_mask = mt_usb11_dmamask;
#ifdef CONFIG_OF
	pdev->dev.dma_mask = &mt_usb11_dmamask;
	pdev->dev.coherent_dma_mask = mt_usb11_dmamask;
#endif

	glue->dev = &pdev->dev;
	glue->musbfsh = musbfsh;

	pdata->platform_ops = &mt_usb11_ops;

	platform_set_drvdata(pdev, glue);

	ret = platform_device_add_resources(musbfsh, pdev->resource, pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "failed to add resources\n");
		goto err3;
	}

	ret = platform_device_add_data(musbfsh, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(&pdev->dev, "failed to add platform_data\n");
		goto err3;
	}

	ret = platform_device_add(musbfsh);

	if (ret) {
		dev_err(&pdev->dev, "failed to register musbfsh device\n");
		goto err3;
	}

	return 0;

err3:
	platform_device_put(musbfsh);

err2:
	musbfsh_put_id(&pdev->dev, musbfshid);

err1:
	kfree(glue);

err0:
	return ret;
}

static int __exit mt_usb_remove(struct platform_device *pdev)
{
	struct mt_usb11_glue *glue = platform_get_drvdata(pdev);

	musbfsh_put_id(&pdev->dev, glue->musbfsh->id);
	platform_device_del(glue->musbfsh);
	platform_device_put(glue->musbfsh);
	kfree(glue);

	return 0;
}

static struct platform_driver mt_usb11_driver = {
	.remove = __exit_p(mt_usb_remove),
	.probe = mt_usb11_probe,
	.driver = {
	.name = "mt_usb11",
#ifdef CONFIG_OF
	.of_match_table = apusb_of_ids,
#endif
	},
};

int usb11_init(void)
{
	INFO("[Flow][USB11]%s:%d\n", __func__, __LINE__);
	return platform_driver_register(&mt_usb11_driver);
}

/*mubsys_initcall(usb11_init);*/

void usb11_exit(void)
{
	platform_driver_unregister(&mt_usb11_driver);
}

/*module_exit(usb11_exit) */
