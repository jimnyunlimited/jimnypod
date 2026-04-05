#include "FT3168.h"
#include "esp_err.h"
#include "lcd_config.h"
#include "i2c_bsp.h"

#define TEST_I2C_PORT I2C_NUM_0

void Touch_Init(void)
{
  uint8_t data = 0x00;
  I2C_writr_buff(I2C_ADDR_FT3168,0x00,&data,1); //Switch to normal mode

}
uint8_t getTouch(uint16_t *x,uint16_t *y)
{
  uint8_t data;
  uint8_t buf[4];
  I2C_read_buff(I2C_ADDR_FT3168,0x02,&data,1);
  if(data)
  {
    I2C_read_buff(I2C_ADDR_FT3168,0x03,buf,4);
    *x = (((uint16_t)buf[0] & 0x0f)<<8) | (uint16_t)buf[1];
    *y = (((uint16_t)buf[2] & 0x0f)<<8) | (uint16_t)buf[3];
    if(*x > EXAMPLE_LCD_H_RES)
    *x = EXAMPLE_LCD_H_RES;
    if(*y > EXAMPLE_LCD_V_RES)
    *y = EXAMPLE_LCD_V_RES;
    return 1;
  }
  return 0;
}


