/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Hongzhou.Yang <hongzhou.yang@mediatek.com>
 *         Yingjoe Chen <yingjoe.chen@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <dt-bindings/pinctrl/mt65xx.h>

#include "pinctrl-mtk-common.h"
#include "pinctrl-mtk-mt8163.h"

static const struct mtk_drv_group_desc mt8163_drv_grp[] =  {
	/* 0E4E8SR 4/8/12/16 */
	MTK_DRV_GRP(4, 16, 1, 2, 4),
	/* 0E2E4SR  2/4/6/8 */
	MTK_DRV_GRP(2, 8, 1, 2, 2),
	/* E8E4E2  2/4/6/8/10/12/14/16 */
	MTK_DRV_GRP(2, 16, 0, 2, 2)
};

static const struct mtk_pin_drv_grp mt8163_pin_drv[] = {
	MTK_PIN_DRV_GRP(0,   0xb00,  0, 1),
	MTK_PIN_DRV_GRP(1,   0xb00,  0, 1),
	MTK_PIN_DRV_GRP(2,   0xb00,  0, 1),
	MTK_PIN_DRV_GRP(3,   0xb00,  0, 1),
	MTK_PIN_DRV_GRP(4,   0xb00,  0, 1),
	MTK_PIN_DRV_GRP(5,   0xb00,  0, 1),
	MTK_PIN_DRV_GRP(6,   0xb00,  0, 1),
	MTK_PIN_DRV_GRP(7,   0xb00, 12, 1),
	MTK_PIN_DRV_GRP(8,   0xb00, 12, 1),
	MTK_PIN_DRV_GRP(9,   0xb00, 12, 1),
	MTK_PIN_DRV_GRP(10,  0xb00,  8, 1),
	MTK_PIN_DRV_GRP(11,  0xb00,  8, 1),
	MTK_PIN_DRV_GRP(12,  0xb00,  8, 1),
	MTK_PIN_DRV_GRP(13,  0xb00,  8, 1),
	MTK_PIN_DRV_GRP(14,  0xb10,  4, 0),
	MTK_PIN_DRV_GRP(15,  0xb10,  4, 0),
	MTK_PIN_DRV_GRP(16,  0xb10,  4, 0),
	MTK_PIN_DRV_GRP(17,  0xb10,  4, 0),
	MTK_PIN_DRV_GRP(18,  0xb10,  8, 0),
	MTK_PIN_DRV_GRP(19,  0xb10,  8, 0),
	MTK_PIN_DRV_GRP(20,  0xb10,  8, 0),
	MTK_PIN_DRV_GRP(21,  0xb10,  8, 0),
	MTK_PIN_DRV_GRP(22,  0xb20,  0, 0),
	MTK_PIN_DRV_GRP(23,  0xb20,  0, 0),
	MTK_PIN_DRV_GRP(24,  0xb20,  0, 0),
	MTK_PIN_DRV_GRP(25,  0xb20,  0, 0),
	MTK_PIN_DRV_GRP(26,  0xb20,  0, 0),
	MTK_PIN_DRV_GRP(27,  0xb20,  4, 0),
	MTK_PIN_DRV_GRP(28,  0xb20,  4, 0),
	MTK_PIN_DRV_GRP(29,  0xb20,  4, 0),
	MTK_PIN_DRV_GRP(30,  0xb20,  4, 0),
	MTK_PIN_DRV_GRP(31,  0xb20,  4, 0),
	MTK_PIN_DRV_GRP(32,  0xb20,  4, 0),
	MTK_PIN_DRV_GRP(33,  0xb30,  4, 1),
	MTK_PIN_DRV_GRP(34,  0xb30,  8, 1),
	MTK_PIN_DRV_GRP(35,  0xb30,  8, 1),
	MTK_PIN_DRV_GRP(36,  0xb30,  8, 1),
	MTK_PIN_DRV_GRP(37,  0xb30,  8, 1),
	MTK_PIN_DRV_GRP(38,  0xb30,  8, 1),
	MTK_PIN_DRV_GRP(39,  0xb30, 12, 1),
	MTK_PIN_DRV_GRP(40,  0xb30, 12, 1),
	MTK_PIN_DRV_GRP(41,  0xb30, 12, 1),
	MTK_PIN_DRV_GRP(42,  0xb30, 12, 1),
	MTK_PIN_DRV_GRP(43,  0xb40, 12, 0),
	MTK_PIN_DRV_GRP(44,  0xb40, 12, 0),
	MTK_PIN_DRV_GRP(45,  0xb40, 12, 0),
	MTK_PIN_DRV_GRP(46,  0xb50,  0, 2),
	MTK_PIN_DRV_GRP(47,  0xb50,  0, 2),
	MTK_PIN_DRV_GRP(48,  0xb50,  0, 2),
	MTK_PIN_DRV_GRP(49,  0xb50,  0, 2),
	MTK_PIN_DRV_GRP(50,  0xb70,  0, 1),
	MTK_PIN_DRV_GRP(51,  0xb70,  0, 1),
	MTK_PIN_DRV_GRP(52,  0xb70,  0, 1),
	MTK_PIN_DRV_GRP(53,  0xb50, 12, 1),
	MTK_PIN_DRV_GRP(54,  0xb50, 12, 1),
	MTK_PIN_DRV_GRP(55,  0xb50, 12, 1),
	MTK_PIN_DRV_GRP(56,  0xb50, 12, 1),
	MTK_PIN_DRV_GRP(59,  0xb40,  4, 1),
	MTK_PIN_DRV_GRP(60,  0xb40,  0, 1),
	MTK_PIN_DRV_GRP(61,  0xb40,  0, 1),
	MTK_PIN_DRV_GRP(62,  0xb40,  0, 1),
	MTK_PIN_DRV_GRP(63,  0xb40,  4, 1),
	MTK_PIN_DRV_GRP(64,  0xb40,  4, 1),
	MTK_PIN_DRV_GRP(65,  0xb40,  4, 1),
	MTK_PIN_DRV_GRP(66,  0xb40,  8, 1),
	MTK_PIN_DRV_GRP(67,  0xb40,  8, 1),
	MTK_PIN_DRV_GRP(68,  0xb40,  8, 1),
	MTK_PIN_DRV_GRP(69,  0xb40,  8, 1),
	MTK_PIN_DRV_GRP(70,  0xb40,  8, 1),
	MTK_PIN_DRV_GRP(71,  0xb40,  8, 1),
	MTK_PIN_DRV_GRP(72,  0xb50,  4, 1),
	MTK_PIN_DRV_GRP(73,  0xb50,  4, 1),
	MTK_PIN_DRV_GRP(74,  0xb50,  4, 1),
	MTK_PIN_DRV_GRP(79,  0xb50,  8, 1),
	MTK_PIN_DRV_GRP(80,  0xb50,  8, 1),
	MTK_PIN_DRV_GRP(81,  0xb50,  8, 1),
	MTK_PIN_DRV_GRP(82,  0xb50,  8, 1),
	MTK_PIN_DRV_GRP(83,  0xb50,  8, 1),
	MTK_PIN_DRV_GRP(84,  0xb50,  8, 1),
	MTK_PIN_DRV_GRP(85,  0xce0,  0, 2),
	MTK_PIN_DRV_GRP(86,  0xcd0,  0, 2),
	MTK_PIN_DRV_GRP(87,  0xcf0,  0, 2),
	MTK_PIN_DRV_GRP(88,  0xcf0,  0, 2),
	MTK_PIN_DRV_GRP(89,  0xcf0,  0, 2),
	MTK_PIN_DRV_GRP(90,  0xcf0,  0, 2),
	MTK_PIN_DRV_GRP(117, 0xb60, 12, 1),
	MTK_PIN_DRV_GRP(118, 0xb60, 12, 1),
	MTK_PIN_DRV_GRP(119, 0xb60, 12, 1),
	MTK_PIN_DRV_GRP(120, 0xb60, 12, 1),
	MTK_PIN_DRV_GRP(121, 0xc80,  0, 2),
	MTK_PIN_DRV_GRP(122, 0xc70,  0, 2),
	MTK_PIN_DRV_GRP(123, 0xc90,  0, 2),
	MTK_PIN_DRV_GRP(124, 0xc90,  0, 2),
	MTK_PIN_DRV_GRP(125, 0xc90,  0, 2),
	MTK_PIN_DRV_GRP(126, 0xc90,  0, 2),
	MTK_PIN_DRV_GRP(127, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(128, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(129, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(130, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(131, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(132, 0xc10,  0, 2),
	MTK_PIN_DRV_GRP(133, 0xc00,  0, 2),
	MTK_PIN_DRV_GRP(134, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(135, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(136, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(137, 0xc20,  0, 2),
	MTK_PIN_DRV_GRP(142, 0xb50,  0, 2),
	MTK_PIN_DRV_GRP(143, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(144, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(145, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(146, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(147, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(148, 0xf10,  0, 2),
	MTK_PIN_DRV_GRP(149, 0xf00,  0, 2),
	MTK_PIN_DRV_GRP(150, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(151, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(152, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(153, 0xf20,  0, 2),
	MTK_PIN_DRV_GRP(154, 0xf20,  0, 2),
};

static const struct mtk_pin_spec_pupd_set_samereg mt8163_spec_pupd[] = {
	MTK_PIN_PUPD_SPEC_SR(33,  0xd90, 2, 1, 0),	/* KPROW0 */
	MTK_PIN_PUPD_SPEC_SR(34,  0xd90, 6, 5, 4),	/* KPROW1 */
	MTK_PIN_PUPD_SPEC_SR(35,  0xd90, 10, 9, 8),	/* KPROW2 */
	MTK_PIN_PUPD_SPEC_SR(36,  0xda0, 2, 1, 0),	/* KPCOL0 */
	MTK_PIN_PUPD_SPEC_SR(37,  0xda0, 6, 5, 4),	/* KPCOL1 */
	MTK_PIN_PUPD_SPEC_SR(38,  0xda0, 10, 9, 8),	/* KPCOL2 */
	MTK_PIN_PUPD_SPEC_SR(46,  0xdb0, 2, 1, 0),	/* EINT14 */
	MTK_PIN_PUPD_SPEC_SR(47,  0xdb0, 6, 5, 4),	/* EINT15 */
	MTK_PIN_PUPD_SPEC_SR(48,  0xdb0, 10, 9, 8),	/* EINT16 */
	MTK_PIN_PUPD_SPEC_SR(49,  0xdb0, 14, 13, 12),	/* EINT17 */
	MTK_PIN_PUPD_SPEC_SR(85,  0xce0, 8, 9, 10),	/* MSDC2_CMD */
	MTK_PIN_PUPD_SPEC_SR(86,  0xcd0, 8, 9, 10),	/* MSDC2_CLK */
	MTK_PIN_PUPD_SPEC_SR(87,  0xd00, 0, 1, 2),	/* MSDC2_DAT0 */
	MTK_PIN_PUPD_SPEC_SR(88,  0xd00, 4, 5, 6),	/* MSDC2_DAT1 */
	MTK_PIN_PUPD_SPEC_SR(89,  0xd00, 8, 9, 10),	/* MSDC2_DAT2 */
	MTK_PIN_PUPD_SPEC_SR(90,  0xd00, 12, 13, 14),	/* MSDC2_DAT3 */
	MTK_PIN_PUPD_SPEC_SR(121, 0xc80, 8, 9, 10),	/* MSDC1_CMD */
	MTK_PIN_PUPD_SPEC_SR(122, 0xc70, 8, 9, 10),	/* MSDC1_CLK */
	MTK_PIN_PUPD_SPEC_SR(123, 0xca0, 0, 1, 2),	/* MSDC1_DAT0 */
	MTK_PIN_PUPD_SPEC_SR(124, 0xca0, 4, 5, 6),	/* MSDC1_DAT1 */
	MTK_PIN_PUPD_SPEC_SR(125, 0xca0, 8, 9, 10),	/* MSDC1_DAT2 */
	MTK_PIN_PUPD_SPEC_SR(126, 0xca0, 12, 13, 14),	/* MSDC1_DAT3 */
	MTK_PIN_PUPD_SPEC_SR(127, 0xc40, 12, 13, 14),	/* MSDC0_DAT7 */
	MTK_PIN_PUPD_SPEC_SR(128, 0xc40, 8, 9, 10),	/* MSDC0_DAT6 */
	MTK_PIN_PUPD_SPEC_SR(129, 0xc40, 4, 5, 6),	/* MSDC0_DAT5 */
	MTK_PIN_PUPD_SPEC_SR(130, 0xc40, 0, 1, 2),	/* MSDC0_DAT4 */
	MTK_PIN_PUPD_SPEC_SR(131, 0xc50, 0, 1, 2),	/* MSDC0_RSTB */
	MTK_PIN_PUPD_SPEC_SR(132, 0xc10, 8, 9, 10),	/* MSDC0_CMD */
	MTK_PIN_PUPD_SPEC_SR(133, 0xc00, 8, 9, 10),	/* MSDC0_CLK */
	MTK_PIN_PUPD_SPEC_SR(134, 0xc30, 12, 13, 14),	/* MSDC0_DAT3 */
	MTK_PIN_PUPD_SPEC_SR(135, 0xc30, 8, 9, 10),	/* MSDC0_DAT2 */
	MTK_PIN_PUPD_SPEC_SR(136, 0xc30, 4, 5, 6),	/* MSDC0_DAT1 */
	MTK_PIN_PUPD_SPEC_SR(137, 0xc30, 0, 1, 2),	/* MSDC0_DAT0 */
	MTK_PIN_PUPD_SPEC_SR(142, 0xdc0, 2, 1, 0),	/* EINT21 */
	MTK_PIN_PUPD_SPEC_SR(143, 0xf40, 12, 13, 14),	/* MSDC3_DAT7 */
	MTK_PIN_PUPD_SPEC_SR(144, 0xf40, 8, 9, 10),	/* MSDC3_DAT6 */
	MTK_PIN_PUPD_SPEC_SR(145, 0xf40, 4, 5, 6),	/* MSDC3_DAT5 */
	MTK_PIN_PUPD_SPEC_SR(146, 0xf40, 0, 1, 2),	/* MSDC3_DAT4 */
	MTK_PIN_PUPD_SPEC_SR(147, 0xf50, 0, 1, 2),	/* MSDC3_RSTB */
	MTK_PIN_PUPD_SPEC_SR(148, 0xf10, 8, 9, 10),	/* MSDC3_CMD */
	MTK_PIN_PUPD_SPEC_SR(149, 0xf00, 8, 9, 10),	/* MSDC3_CLK */
	MTK_PIN_PUPD_SPEC_SR(150, 0xf30, 12, 13, 14),	/* MSDC3_DAT3 */
	MTK_PIN_PUPD_SPEC_SR(151, 0xf30, 8, 9, 10),	/* MSDC3_DAT2 */
	MTK_PIN_PUPD_SPEC_SR(152, 0xf30, 4, 5, 6),	/* MSDC3_DAT1 */
	MTK_PIN_PUPD_SPEC_SR(153, 0xf30, 0, 1, 2),	/* MSDC3_DAT0 */
	MTK_PIN_PUPD_SPEC_SR(154, 0xf50, 4, 5, 6),	/* MSDC3_DSL */
};

static int mt8163_spec_pull_set(struct regmap *regmap, unsigned int pin,
		unsigned char align, bool isup, unsigned int r1r0)
{
	return mtk_pctrl_spec_pull_set_samereg(regmap, mt8163_spec_pupd,
		ARRAY_SIZE(mt8163_spec_pupd), pin, align, isup, r1r0);
}

static const struct mtk_pin_ies_smt_set mt8163_ies_set[] = {
	MTK_PIN_IES_SMT_SPEC(0, 9, 0x900, 0),
	MTK_PIN_IES_SMT_SPEC(10, 13, 0x900, 1),
	MTK_PIN_IES_SMT_SPEC(14, 28, 0x900, 2),
	MTK_PIN_IES_SMT_SPEC(29, 32, 0x900, 3),
	MTK_PIN_IES_SMT_SPEC(33, 33, 0x910, 11),
	MTK_PIN_IES_SMT_SPEC(34, 38, 0x900, 10),
	MTK_PIN_IES_SMT_SPEC(39, 42, 0x900, 11),
	MTK_PIN_IES_SMT_SPEC(43, 45, 0x900, 12),
	MTK_PIN_IES_SMT_SPEC(46, 49, 0x900, 13),
	MTK_PIN_IES_SMT_SPEC(50, 52, 0x910, 10),
	MTK_PIN_IES_SMT_SPEC(53, 56, 0x900, 14),
	MTK_PIN_IES_SMT_SPEC(57, 58, 0x910, 0),
	MTK_PIN_IES_SMT_SPEC(59, 65, 0x910, 2),
	MTK_PIN_IES_SMT_SPEC(66, 71, 0x910, 3),
	MTK_PIN_IES_SMT_SPEC(72, 74, 0x910, 4),
	MTK_PIN_IES_SMT_SPEC(75, 76, 0x900, 15),
	MTK_PIN_IES_SMT_SPEC(77, 78, 0x910, 1),
	MTK_PIN_IES_SMT_SPEC(79, 82, 0x910, 5),
	MTK_PIN_IES_SMT_SPEC(83, 84, 0x910, 6),
	MTK_PIN_IES_SMT_SPEC(117, 120, 0x910, 7),
	MTK_PIN_IES_SMT_SPEC(121, 121, 0xc80, 4),
	MTK_PIN_IES_SMT_SPEC(122, 122, 0xc70, 4),
	MTK_PIN_IES_SMT_SPEC(123, 126, 0xc90, 4),
	MTK_PIN_IES_SMT_SPEC(127, 131, 0xc20, 4),
	MTK_PIN_IES_SMT_SPEC(132, 132, 0xc10, 4),
	MTK_PIN_IES_SMT_SPEC(133, 133, 0xc00, 4),
	MTK_PIN_IES_SMT_SPEC(134, 137, 0xc20, 4),
	MTK_PIN_IES_SMT_SPEC(138, 141, 0x910, 9),
	MTK_PIN_IES_SMT_SPEC(142, 142, 0x900, 13),
	MTK_PIN_IES_SMT_SPEC(143, 147, 0xf20, 4),
	MTK_PIN_IES_SMT_SPEC(148, 148, 0xf10, 4),
	MTK_PIN_IES_SMT_SPEC(149, 149, 0xf00, 4),
	MTK_PIN_IES_SMT_SPEC(150, 153, 0xf20, 4),
	MTK_PIN_IES_SMT_SPEC(154, 154, 0xf20, 7),
};

static const struct mtk_pin_ies_smt_set mt8163_smt_set[] = {
	MTK_PIN_IES_SMT_SPEC(0, 9, 0x920, 0),
	MTK_PIN_IES_SMT_SPEC(10, 13, 0x920, 1),
	MTK_PIN_IES_SMT_SPEC(14, 28, 0x920, 2),
	MTK_PIN_IES_SMT_SPEC(29, 32, 0x920, 3),
	MTK_PIN_IES_SMT_SPEC(33, 33, 0x930, 11),
	MTK_PIN_IES_SMT_SPEC(34, 38, 0x920, 10),
	MTK_PIN_IES_SMT_SPEC(39, 42, 0x920, 11),
	MTK_PIN_IES_SMT_SPEC(43, 45, 0x920, 12),
	MTK_PIN_IES_SMT_SPEC(46, 49, 0x920, 13),
	MTK_PIN_IES_SMT_SPEC(50, 52, 0x930, 10),
	MTK_PIN_IES_SMT_SPEC(53, 56, 0x920, 14),
	MTK_PIN_IES_SMT_SPEC(57, 58, 0x930, 0),
	MTK_PIN_IES_SMT_SPEC(59, 65, 0x930, 2),
	MTK_PIN_IES_SMT_SPEC(66, 71, 0x930, 3),
	MTK_PIN_IES_SMT_SPEC(72, 74, 0x930, 4),
	MTK_PIN_IES_SMT_SPEC(75, 76, 0x920, 15),
	MTK_PIN_IES_SMT_SPEC(77, 78, 0x930, 1),
	MTK_PIN_IES_SMT_SPEC(79, 82, 0x930, 5),
	MTK_PIN_IES_SMT_SPEC(83, 84, 0x930, 6),
	MTK_PIN_IES_SMT_SPEC(85, 85, 0xce0, 11),
	MTK_PIN_IES_SMT_SPEC(86, 86, 0xcd0, 11),
	MTK_PIN_IES_SMT_SPEC(87, 87, 0xd00, 3),
	MTK_PIN_IES_SMT_SPEC(88, 88, 0xd00, 7),
	MTK_PIN_IES_SMT_SPEC(89, 89, 0xd00, 11),
	MTK_PIN_IES_SMT_SPEC(90, 90, 0xd00, 15),
	MTK_PIN_IES_SMT_SPEC(117, 120, 0x930, 7),
	MTK_PIN_IES_SMT_SPEC(121, 121, 0xc80, 11),
	MTK_PIN_IES_SMT_SPEC(122, 122, 0xc70, 11),
	MTK_PIN_IES_SMT_SPEC(123, 123, 0xca0, 3),
	MTK_PIN_IES_SMT_SPEC(124, 124, 0xca0, 7),
	MTK_PIN_IES_SMT_SPEC(125, 125, 0xca0, 11),
	MTK_PIN_IES_SMT_SPEC(126, 126, 0xca0, 15),
	MTK_PIN_IES_SMT_SPEC(127, 127, 0xc40, 15),
	MTK_PIN_IES_SMT_SPEC(128, 128, 0xc40, 11),
	MTK_PIN_IES_SMT_SPEC(129, 129, 0xc40, 7),
	MTK_PIN_IES_SMT_SPEC(130, 130, 0xc40, 3),
	MTK_PIN_IES_SMT_SPEC(131, 131, 0xc50, 3),
	MTK_PIN_IES_SMT_SPEC(132, 132, 0xc10, 11),
	MTK_PIN_IES_SMT_SPEC(133, 133, 0xc00, 11),
	MTK_PIN_IES_SMT_SPEC(134, 134, 0xc30, 15),
	MTK_PIN_IES_SMT_SPEC(135, 135, 0xc30, 11),
	MTK_PIN_IES_SMT_SPEC(136, 136, 0xc30, 7),
	MTK_PIN_IES_SMT_SPEC(137, 137, 0xc30, 3),
	MTK_PIN_IES_SMT_SPEC(138, 141, 0x930, 9),
	MTK_PIN_IES_SMT_SPEC(142, 142, 0x920, 13),
	MTK_PIN_IES_SMT_SPEC(143, 143, 0xf40, 15),
	MTK_PIN_IES_SMT_SPEC(144, 144, 0xf40, 11),
	MTK_PIN_IES_SMT_SPEC(145, 145, 0xf40, 7),
	MTK_PIN_IES_SMT_SPEC(146, 146, 0xf40, 3),
	MTK_PIN_IES_SMT_SPEC(147, 147, 0xf50, 3),
	MTK_PIN_IES_SMT_SPEC(148, 148, 0xf10, 11),
	MTK_PIN_IES_SMT_SPEC(149, 149, 0xf00, 11),
	MTK_PIN_IES_SMT_SPEC(150, 150, 0xf30, 15),
	MTK_PIN_IES_SMT_SPEC(151, 151, 0xf30, 11),
	MTK_PIN_IES_SMT_SPEC(152, 152, 0xf30, 7),
	MTK_PIN_IES_SMT_SPEC(153, 153, 0xf30, 3),
	MTK_PIN_IES_SMT_SPEC(154, 154, 0xf50, 3),
};

static int mt8163_ies_smt_set(struct regmap *regmap, unsigned int pin,
		unsigned char align, int value, enum pin_config_param arg)
{
	if (arg == PIN_CONFIG_INPUT_ENABLE)
		return mtk_pconf_spec_set_ies_smt_range(regmap, mt8163_ies_set,
			ARRAY_SIZE(mt8163_ies_set), pin, align, value);
	else if (arg == PIN_CONFIG_INPUT_SCHMITT_ENABLE)
		return mtk_pconf_spec_set_ies_smt_range(regmap, mt8163_smt_set,
			ARRAY_SIZE(mt8163_smt_set), pin, align, value);
	return -EINVAL;
}

static int mt8163_spec_ies_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_get_ies_smt_range(regmap, mt8163_ies_set,
		ARRAY_SIZE(mt8163_ies_set), pin);
}

static int mt8163_spec_smt_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_get_ies_smt_range(regmap, mt8163_smt_set,
		ARRAY_SIZE(mt8163_smt_set), pin);
}

static int mt8163_spec_pull_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_pull_get_samereg(regmap, mt8163_spec_pupd,
		ARRAY_SIZE(mt8163_spec_pupd), pin);
}

static const struct mtk_pinctrl_devdata mt8163_pinctrl_data = {
	.pins = mtk_pins_mt8163,
	.npins = ARRAY_SIZE(mtk_pins_mt8163),
	.grp_desc = mt8163_drv_grp,
	.n_grp_cls = ARRAY_SIZE(mt8163_drv_grp),
	.pin_drv_grp = mt8163_pin_drv,
	.n_pin_drv_grps = ARRAY_SIZE(mt8163_pin_drv),
	.spec_pull_set = mt8163_spec_pull_set,
	.spec_ies_smt_set = mt8163_ies_smt_set,
	.spec_pull_get = mt8163_spec_pull_get,
	.spec_ies_get = mt8163_spec_ies_get,
	.spec_smt_get = mt8163_spec_smt_get,
	.dir_offset = 0x0000,
	.pullen_offset = 0x0100,
	.pullsel_offset = 0x0200,
	.dout_offset = 0x0400,
	.din_offset = 0x0500,
	.pinmux_offset = 0x0600,
	.type1_start = 155,
	.type1_end = 155,
	.port_shf = 4,
	.port_mask = 0xf,
	.port_align = 4,
	.eint_offsets = {
		.name = "mt8163_eint",
		.stat      = 0x000,
		.ack       = 0x040,
		.mask      = 0x080,
		.mask_set  = 0x0c0,
		.mask_clr  = 0x100,
		.sens      = 0x140,
		.sens_set  = 0x180,
		.sens_clr  = 0x1c0,
		.soft      = 0x200,
		.soft_set  = 0x240,
		.soft_clr  = 0x280,
		.pol       = 0x300,
		.pol_set   = 0x340,
		.pol_clr   = 0x380,
		.dom_en    = 0x400,
		.dbnc_ctrl = 0x500,
		.dbnc_set  = 0x600,
		.dbnc_clr  = 0x700,
		.port_mask = 7,
		.ports     = 6,
	},
	.ap_num = 169,
	.db_cnt = 64,
};

static int mt8163_pinctrl_probe(struct platform_device *pdev)
{
	return mtk_pctrl_init(pdev, &mt8163_pinctrl_data, NULL);
}

static const struct of_device_id mt8163_pctrl_match[] = {
	{ .compatible = "mediatek,mt8163-pinctrl", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8163_pctrl_match);

static struct platform_driver mtk_pinctrl_driver = {
	.probe = mt8163_pinctrl_probe,
	.driver = {
		.name = "mediatek-mt8163-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = mt8163_pctrl_match,
		.pm = &mtk_eint_pm_ops,
	},
};

static int __init mtk_pinctrl_init(void)
{
	return platform_driver_register(&mtk_pinctrl_driver);
}

module_init(mtk_pinctrl_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT8163 Pinctrl Driver");
MODULE_AUTHOR("Yingjoe Chen <yingjoe.chen@mediatek.com>");
