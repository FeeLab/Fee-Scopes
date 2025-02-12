#ifndef PYHTON480_INFO_H
#define PYHTON480_INFO_H

// Sensor Registers:
// =================
#define SENSOR_REG_CHIP_ID               0
#define SENSOR_REG_CHIP_INFO             1
#define SENSOR_REG_CHIP_CONFIG           2
#define SENSOR_REG_SOFT_RESET_PLL        8
#define SENSOR_REG_SOFT_RESET_CGEN       9
#define SENSOR_REG_SOFT_RESET_ANALOG    10
#define SENSOR_REG_PLL_CONFIG           16
#define SENSOR_REG_IO_CONFIG            20
#define SENSOR_REG_PLL_LOCK             24
#define SENSOR_REG_CLOCK_CONFIG         32
#define SENSOR_REG_CLOCK_ENABLE         34
#define SENSOR_REG_IMAGE_CORE_CONFIG1   40
#define SENSOR_REG_IMAGE_CORE_CONFIG2   41
#define SENSOR_REG_AFE_CONFIG           48
#define SENSOR_REG_BIAS_PWR_CONFIG      64
#define SENSOR_REG_BIAS_CONFIG          65
#define SENSOR_REG_AFE_BIAS_CONFIG      66
#define SENSOR_REG_COLUMN_BIAS_CONFIG   67
#define SENSOR_REG_LVDS_BIAS_CONFIG     68
#define SENSOR_REG_ADC_BIAS             69
#define SENSOR_REG_CHARGE_PUMP_CONFIG   72
#define SENSOR_REG_TEMP_SENSOR_CONFIG   96
#define SENSOR_REG_TEMP_SENSOR_STATUS   97
#define SENSOR_REG_LVDS_CONFIG         112
#define SENSOR_REG_BLACK_CAL           128
#define SENSOR_REG_TEST_PAT_CONFIG     144
#define SENSOR_REG_TEST_PAT_0_1        146
#define SENSOR_REG_TEST_PAT_2_3        147
#define SENSOR_REG_TEST_PAT_CONFIG     144
#define SENSOR_REG_TEST_PAT_LSB_01     146
#define SENSOR_REG_TEST_PAT_LSB_23     147
#define SENSOR_REG_TEST_PAT_MSB_0123   150
#define SENSOR_REG_AEC_INTENSITY       161
#define SENSOR_REG_SEQ_GEN_CONFIG      192
#define SENSOR_REG_SEQ_DEL_CONFIG      193
#define SENSOR_REG_INTEGRATION_CONTROL 194
#define SENSOR_REG_ACTIVE_ROI          195
#define SENSOR_REG_BLACK_LINE_CONFIG   197
#define SENSOR_REG_MULT_TIMER_0        199
#define SENSOR_REG_FR_RST_LENGTH       200
#define SENSOR_REG_EXPOSURE0           201
#define SENSOR_REG_ANALOG_GAIN         204
#define SENSOR_REG_DIGITAL_GAIN        205
#define SENSOR_REG_ROI0_X_CONFIG       256
#define SENSOR_REG_ROI0_Y_START        257
#define SENSOR_REG_ROI0_Y_END          258
#define SENSOR_REG_ROI1_X_CONFIG       259
#define SENSOR_REG_ROI1_Y_START        260
#define SENSOR_REG_ROI1_Y_END          261
#define SENSOR_REG_ROI2_X_CONFIG       262
#define SENSOR_REG_ROI2_Y_START        263
#define SENSOR_REG_ROI2_Y_END          264
#define SENSOR_REG_ROI3_X_CONFIG       265
#define SENSOR_REG_ROI3_Y_START        266
#define SENSOR_REG_ROI3_Y_END          267

// Other info:
// ===========
#define SENSOR_NUM_ROIS                  4
#define SENSOR_PYTHON_480_WIDTH        808
#define SENSOR_PYTHON_480_HEIGHT       608
#define SENSOR_FR_LEN_480             8030

#endif