#ifndef __TS3A225E_H
#define __TS3A225E_H

#define TS3A225E_ID_REG_ADDR         0x01
#define TS3A225E_CTRL1_REG_ADDR      0x02
#define TS3A225E_CTRL2_REG_ADDR      0x03
#define TS3A225E_CTRL3_REG_ADDR      0x04
#define TS3A225E_DAT1_REG_ADDR       0x05
#define TS3A225E_INT_REG_ADDR        0x06
#define TS3A225E_INT_MASK_REG_ADDR   0x07

#define TS3A225E_FIRST_REG_ADDR TS3A225E_ID_REG_ADDR
#define TS3A225E_LAST_REG_ADDR TS3A225E_INT_MASK_REG_ADDR

#define TS3A225E_CTRL3_REG_DEFAULT_VALUE  0x00
#define TS3A225E_ID_REG_DEFAULT_VALUE     0x02

/* Note: Keep this the same as in kernel/sound/soc/codecs/wcd9xxx-mbhc.h
         enum wcd9xxx_mbhc_plug_tye.
*/
typedef enum {
    INPUT_TYPE_INVALID = -1,
    INPUT_TYPE_NONE,
    INPUT_TYPE_HEADSET,
    INPUT_TYPE_HEADPHONE,
    INPUT_TYPE_HIGH_HPH,
    INPUT_TYPE_GND_MIC_SWAP,
} headset_plug_type_t;

int ts3a225e_headset_switch_init(void);
int ts3a225e_headset_switch_deinit(void);
int ts3a225e_headset_switch_set_type(headset_plug_type_t plug_type);
int ts3a225e_trigger_detection(headset_plug_type_t plug_type);


#endif
