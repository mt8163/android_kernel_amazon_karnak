#ifndef __MT_AMZ_MCLK_H__
#define __MT_AMZ_MCLK_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#define DEBUG_ISP_MMCLK
/* Enable MCLK with register write
 * Returns non-zero value if it fails to load correct register offset from DTS
 */
int mclk_enable_reg(int enable);

/* Set MCLK frequency
 * NOTE: Not all values are possible. See manual and implementation for details
 * Returns non-zero value if it fails to load correct register offset from DTS
*/
int mclk_divider_reg(uint64_t freq);

/* Loads ISP clocks from the dts
 * Returns error code if it fails to find clocks in DTS
 */
int isp_clk_init(void);

/* Enables/Disables ISP clocks
 * Returns error code if it fails to prepare_enable() any clock
 */
int isp_clk_enable(int enable);

/* Loads CCF clocks from the dts
 * Returns error code if it fails to find clocks in DTS
 */
int ccf_clk_init(void);

/* Enables/Disables CCF clocks
 * Returns error code if it fails to prepare_enable() any clock
 */
int ccf_clk_enable(int enable);

#ifdef DEBUG_ISP_MMCLK
/* Dumps core ISP registers in log */
void register_dump(void);
#endif
#endif /* __MT_AMZ_MCLK_H__  */
