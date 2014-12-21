#ifndef __INCLUDE_WIRELESS_H
#define __INCLUDE_WIRELESS_H

/*
 * Definitions for wireless sensor messages
 *
 *  uint8_t     id          Unique station ID
 *  uint8_t     nvalues     Number of sensor readings
 *
 *  nvalues * [
 *  uint8_t     type        Sensor type (WL_SENSOR_TYPE_*)
 *  int16_t     value       Sensor value (little endian)
 *  ]
 */

/*
 * Maximum number of sensor values in a message
 */
#define WL_SENSOR_MAX_VALUES            3

/*
 * Message component sizes
 */
#define WL_SENSOR_MSG_HDR_LEN           2
#define WL_SENSOR_MSG_VALUE_LEN         3
#define WL_SENSOR_MSG_SIZE(NVALUES) \
    (WL_SENSOR_MSG_HDR_LEN + (NVALUES) * WL_SENSOR_MSG_VALUE_LEN)

#define WL_SENSOR_MSG_MAX_SIZE      \
    WL_SENSOR_MSG_SIZE(WL_SENSOR_MAX_VALUES) \

/*
 * Standard sensor types
 */
#define WL_SENSOR_TYPE_TEMPERATURE      1
#define WL_SENSOR_TYPE_PRESSURE         2
#define WL_SENSOR_TYPE_COUNTER          3
#define WL_SENSOR_TYPE_LIGHT            4
#define WL_SENSOR_TYPE_HUMIDITY         5

/*
 * Macros to access message components
 */
#define WL_SENSOR_MSG_STATION_ID(MSG)   \
    *(uint8_t *)((MSG))

#define WL_SENSOR_MSG_NUM_VALUES(MSG)   \
    *(uint8_t *)((MSG) + 1)

#define WL_SENSOR_MSG_TYPE(MSG, N)      \
    *(uint8_t *)((MSG) + WL_SENSOR_MSG_HDR_LEN + (N) * WL_SENSOR_MSG_VALUE_LEN)

#define WL_SENSOR_MSG_VALUE(MSG, N)     \
    *(int16_t *)((MSG) + WL_SENSOR_MSG_HDR_LEN + (N) * WL_SENSOR_MSG_VALUE_LEN + 1)
            

#endif /* __INCLUDE_WIRELESS_H */
