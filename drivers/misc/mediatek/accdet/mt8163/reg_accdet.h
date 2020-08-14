#define ACCDET_BASE               0x00000000
#define TOP_RST_ACCDET		 (ACCDET_BASE + 0x0114)
#define TOP_RST_ACCDET_SET       (ACCDET_BASE + 0x0116)
#define TOP_RST_ACCDET_CLR       (ACCDET_BASE + 0x0118)

#define INT_CON_ACCDET           (ACCDET_BASE + 0x0166)
#define INT_CON_ACCDET_SET	 (ACCDET_BASE + 0x0168)
#define INT_CON_ACCDET_CLR       (ACCDET_BASE + 0x016A)

#define INT_STATUS_ACCDET        (ACCDET_BASE + 0x0174)

#define TOP_CKPDN                (ACCDET_BASE + 0x0108)
#define TOP_CKPDN_SET            (ACCDET_BASE + 0x010A)
#define TOP_CKPDN_CLR            (ACCDET_BASE + 0x010C)


#define ACCDET_RSV               (ACCDET_BASE + 0x077A)

#define ACCDET_CTRL              (ACCDET_BASE + 0x077C)
#define ACCDET_STATE_SWCTRL      (ACCDET_BASE + 0x077E)
#define ACCDET_PWM_WIDTH         (ACCDET_BASE + 0x0780)
#define ACCDET_PWM_THRESH        (ACCDET_BASE + 0x0782)
#define ACCDET_EN_DELAY_NUM      (ACCDET_BASE + 0x0784)
#define ACCDET_DEBOUNCE0         (ACCDET_BASE + 0x0786)
#define ACCDET_DEBOUNCE1         (ACCDET_BASE + 0x0788)
#define ACCDET_DEBOUNCE2         (ACCDET_BASE + 0x078A)
#define ACCDET_DEBOUNCE3         (ACCDET_BASE + 0x078C)

#define ACCDET_DEFAULT_STATE_RG  (ACCDET_BASE + 0x078E)


#define ACCDET_IRQ_STS           (ACCDET_BASE + 0x0790)

#define ACCDET_CONTROL_RG        (ACCDET_BASE + 0x0792)
#define ACCDET_STATE_RG          (ACCDET_BASE + 0x0794)

#define ACCDET_CUR_DEB	         (ACCDET_BASE + 0x0796)
#define ACCDET_RSV_CON0		 (ACCDET_BASE + 0x0798)
#define ACCDET_RSV_CON1		 (ACCDET_BASE + 0x079A)

#define ACCDET_CTRL_EN           (1<<0)
#define ACCDET_MIC_PWM_IDLE      (1<<6)
#define ACCDET_VTH_PWM_IDLE      (1<<5)
#define ACCDET_CMP_PWM_IDLE      (1<<4)
#define ACCDET_CMP_EN            (1<<0)
#define ACCDET_VTH_EN            (1<<1)
#define ACCDET_MICBIA_EN         (1<<2)


#define ACCDET_ENABLE            (1<<0)
#define ACCDET_DISABLE           (0<<0)

#define ACCDET_RESET_SET         (1<<4)
#define ACCDET_RESET_CLR         (1<<4)

#define IRQ_CLR_BIT              0x100
#define IRQ_STATUS_BIT           (1<<0)

#define RG_ACCDET_IRQ_SET        (1<<2)
#define RG_ACCDET_IRQ_CLR        (1<<2)
#define RG_ACCDET_IRQ_STATUS_CLR (1<<2)

#define RG_ACCDET_CLK_SET        (1<<9)
#define RG_ACCDET_CLK_CLR        (1<<9)


#define ACCDET_PWM_EN_SW         (1<<15)
#define ACCDET_MIC_EN_SW         (1<<15)
#define ACCDET_VTH_EN_SW         (1<<15)
#define ACCDET_CMP_EN_SW         (1<<15)

#define ACCDET_SWCTRL_EN         0x07

#define ACCDET_IN_SW             0x10

#define ACCDET_PWM_SEL_CMP       0x00
#define ACCDET_PWM_SEL_VTH       0x01
#define ACCDET_PWM_SEL_MIC       0x10
#define ACCDET_PWM_SEL_SW        0x11
#define ACCDET_SWCTRL_IDLE_EN    (0x07<<4)


#define ACCDET_TEST_MODE5_ACCDET_IN_GPI        (1<<5)
#define ACCDET_TEST_MODE4_ACCDET_IN_SW         (1<<4)
#define ACCDET_TEST_MODE3_MIC_SW               (1<<3)
#define ACCDET_TEST_MODE2_VTH_SW               (1<<2)
#define ACCDET_TEST_MODE1_CMP_SW               (1<<1)
#define ACCDET_TEST_MODE0_GPI                  (1<<0)

/*power mode and auxadc switch on/off*/
#define ACCDET_1V9_MODE_OFF   0x1A10
#define ACCDET_2V8_MODE_OFF   0x5A10
#define ACCDET_1V9_MODE_ON    0x1E10
#define ACCDET_2V8_MODE_ON    0x5A20
#define ACCDET_MICBIAS1_OFF_BIT (1<<13)
#define ACCDET_2V8_MODE_OFF_IND_CTL (ACCDET_2V8_MODE_ON | ACCDET_MICBIAS1_OFF_BIT)

/* reg 0x077A is not defined on MT6323 datasheet, this is a internal test reg */
/* bit 4-6 stand for SWCTRL of voltage detection in ADC, it is a changeable resistence in fact. */
/*  - 0b001 is for 1.9V mode, 0b010 is for 2.8V mode, 0b100 is 200K res, reserved for internal tests */
/* bit 12-15 stand for RSV */
/*  - bit 12 is always 1, bit 15 is reserved, not used */
/*  - bit 13 is for enabling power to MICBIAS0 only (0b1) or both MICBIAS0 & MICBIAS1 (0b0) */
/*  - bit 14 is for 1.9V mode (0b0) or 2.8V mode (0b1) selection*/
