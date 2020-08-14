#ifndef __PWM_MUTEBUTTON_H
#define __PWM_MUTEBUTTON_H

#ifdef CONFIG_PWM_MUTEBUTTON
#define NUM_CHANNELS 1

#define NUM_LED_COLORS 1
#define NUM_LED_CALIB_PARAMS 3

#define LED_PWM_MAX_SCALING 0x7F
#define BYTEMASK 0xFF
#define LED_PWM_SCALING_DEFAULT 0x007F0000
#define LED_PWM_MAX_LIMIT_DEFAULT 0x00FF0000
#define INDEX_LEDCALIBENABLE 0
#define INDEX_PWMSCALING 1
#define INDEX_PWMMAXLIMIT 2

#define BRIGHTNESS_LEVEL_MAX 255
#define PWM_DUTY_CYCLE_MAX 40000
#define PWM_PERIOD_MAX 40000

static int ledcalibparams[NUM_LED_CALIB_PARAMS];

struct led_mutebutton {
	/* Mute LED brightness Calibration*/
	int ledparams;
	int ledpwmscaling;
	int ledpwmmaxlimiter;
};

struct led_mutebutton_data {
	unsigned int		period;
	int			duty;
	struct mutex 		lock;
	uint8_t			state[NUM_CHANNELS];
	uint8_t			ledpwmmaxlimiterrgb[NUM_LED_COLORS];
	uint8_t			ledpwmscalingrgb[NUM_LED_COLORS];
	int			ledparams;
	int			ledpwmscaling;
	int			ledpwmmaxlimiter;
};

struct led_mutebutton_priv {
	int num_leds;
	struct led_mutebutton_data leds[0];
};

extern unsigned int idme_get_ledparams_value(void);
extern unsigned int idme_get_ledcal_value(void);
extern unsigned int idme_get_ledpwmmaxlimit_value(void);
#endif


#endif	/* __PWM_MUTEBUTTON_H */
