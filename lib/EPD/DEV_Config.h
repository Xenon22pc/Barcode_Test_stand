/*****************************************************************************
* | File      	:   DEV_Config.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-02-19
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

extern const char *TAG;

#define USE_DEBUG 1
#if USE_DEBUG
	#define Debug(__info) ESP_LOGI(TAG, __info)
#else
	#define Debug(__info)  
#endif

//SPI
#define EPD_HOST VSPI_HOST
#define SPI_SPEED 2000000   //2MHz
spi_device_handle_t spi;

/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

/**
 * GPIO config
**/

//TTGO T5

#define TTGO_USE 1

#ifdef TTGO_USE
/*
* [env:esp32doit-devkit-v1]
*/
	#define EPD_SCK_PIN  18
	#define EPD_MOSI_PIN 23
	#define EPD_CS_PIN   5

	#define EPD_RST_PIN  12
	#define EPD_DC_PIN   19
	#define EPD_BUSY_PIN 4
#else
/*
* [env:az-delivery-devkit-v4]
*/
	#define EPD_SCK_PIN  18
	#define EPD_MOSI_PIN 23
	#define EPD_CS_PIN   5

	#define EPD_RST_PIN  2
	#define EPD_DC_PIN   15
	#define EPD_BUSY_PIN 4
#endif



/**
 * GPIO read and write
**/
#define DEV_Digital_Write(_pin, _value) gpio_set_level(_pin, _value)
#define DEV_Digital_Read(_pin) gpio_get_level(_pin)

/**
 * delay x ms
**/
#define DEV_Delay_ms(__xms) vTaskDelay(__xms / portTICK_RATE_MS)

/*------------------------------------------------------------------------------------------------------*/
UBYTE DEV_Module_Init(void);
void DEV_SPI_WriteByte(const UBYTE data);

#endif
