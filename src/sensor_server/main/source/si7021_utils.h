#ifndef _SI7021_UTILS_H_
#define _SI7021_UTILS_H_

#define _I2C_NUMBER(num) I2C_NUM_##num
#define I2C_NUMBER(num) _I2C_NUMBER(num)

#define I2C_MASTER_NUM  I2C_NUMBER(CONFIG_I2C_MASTER_PORT_NUM) /*!< I2C port number for master dev */

#define ACK_CHECK_EN   0x1   /*!< I2C master will check ack from slave */
#define ACK_CHECK_DIS  0x0   /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0   /*!< I2C ack value */
#define NACK_VAL       0x1   /*!< I2C nack value */
#define I2C_TIMEOUT_MS 1000

#define SLAVE_ADDR  0x40 /*!< slave address for SGP30 sensor */
#define SENSOR_DELAY 20

#define DELAY_TIME_ITEMS CONFIG_DELAY_TIME_ITEMS
#define WINDOW_SIZE      CONFIG_WINDOW_SIZE
#define SIZE             CONFIG_BUFFER_SIZE

#endif