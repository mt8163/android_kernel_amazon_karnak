#ifndef __DOUGH_H__
#define __DOUGH_H__

#define DOUGH_AUDIO_SAMPLE_WIDTH 3

#if defined CONFIG_SND_SOC_8_MICS
#define DOUGH_AUDIO_NUM_CHANNELS 9
#define DOUGH_AUDIO_FRAME_BUF    255
#elif defined CONFIG_SND_SOC_4_MICS
#define DOUGH_AUDIO_NUM_CHANNELS 6
#define DOUGH_AUDIO_FRAME_BUF    256
#else
#define DOUGH_AUDIO_NUM_CHANNELS 4
#define DOUGH_AUDIO_FRAME_BUF    256
#endif

#define DOUGH_AUDIO_FRAME_BYTES  ((DOUGH_AUDIO_SAMPLE_WIDTH)*\
				 (DOUGH_AUDIO_NUM_CHANNELS))
#define DOUGH_FPGA_REV_MIN       30
#define DOUGH_FPGA_REV_MAX       251

enum dough_fw_cmd {
	dough_fw_nop = 0,
	dough_fw_off = 0x80,
	dough_fw_i2s = 0x81,
	dough_fw_tpg = 0x83,
};

struct dough_audio_frame {
	uint8_t audio_data[DOUGH_AUDIO_FRAME_BYTES];
};

struct __attribute__((__packed__)) dough_status_frame {
#if defined CONFIG_SND_SOC_8_MICS
	uint8_t rsvd0[15];          /* bytes 12-26 */
	uint8_t rsvd0[6];           /* bytes 12-17 */
#elif defined CONFIG_SND_SOC_4_MICS
	uint8_t rsvd0[6];           /* bytes 12-17 */
#endif
	uint32_t timestamp_48mhz;   /* bytes 8-11 */
	uint16_t num_audio_frames;  /* bytes 6-7 */
	uint8_t rsvd1[1];           /* bytes 5 */
	uint8_t mode;               /* byte 4 */
	uint8_t dac_inactive;       /* byte 3 */
	uint8_t i2s_inactive;       /* byte 2 */
	uint8_t overrun;            /* byte 1 */
	uint8_t fpga_rev;           /* byte 0 */
};

struct __attribute__((__packed__)) dough_frame {
	struct dough_status_frame dsf;
	struct dough_audio_frame daf[DOUGH_AUDIO_FRAME_BUF];
};

#endif
