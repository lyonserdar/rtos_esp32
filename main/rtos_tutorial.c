#include "driver/gpio.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "lcd/lcd_common.h"
#include "lcd/oled_ssd1331.h"
#include "portmacro.h"
#include "ssd1306_8bit.h"
#include "ssd1306_fonts.h"
#include "ssd1306_generic.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SPI_CS 5
#define SPI_RES 17
#define SPI_DC 16
#define BTN_UP 26
#define BTN_DOWN 27
#define BTN_SELECT 25
#define BOUNCE_TIME 250
#define BLINK_GPIO 2

static QueueHandle_t interuptQueue;
static TaskHandle_t task_1 = NULL;

static uint32_t button_time = 0;
static uint32_t last_button_time = 0;

static const char msg[] = "Running!";

static void startTask(void *pvParameter) {
  while (true) {
    for (int i = 0; i < strlen(msg); i++)
      printf("%c", msg[i]);
    printf("\n");

    if (task_1 != NULL) {
      vTaskDelete(task_1);
      task_1 = NULL;
    }
  }
}

static void blinkLED(void *pvParameter) {
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

  while (1) {
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static SAppMenu menu;
static const char *menuItems[] = {
    "Connect",
    "Disconnect",
};

static void gpio_interrupt_handler(void *args) {
  button_time = xTaskGetTickCountFromISR();
  if (button_time - last_button_time > BOUNCE_TIME / portTICK_PERIOD_MS) {
    int pinNumber = (uintptr_t)args;
    xQueueSendFromISR(interuptQueue, &pinNumber, NULL);
    last_button_time = button_time;
  }
}

static void controlDisplay(void *pvParameter) {
  int pinNumber = 0;
  while (true) {
    if (xQueueReceive(interuptQueue, &pinNumber, portMAX_DELAY)) {
      switch (pinNumber) {
      case BTN_UP:
        ssd1306_menuUp(&menu);
        break;
      case BTN_DOWN:
        ssd1306_menuDown(&menu);
        break;
      case BTN_SELECT:
        switch (ssd1306_menuSelection(&menu)) {
        case 0:
          printf("Connect\n");
          break;
        case 1:
          printf("Disconnect\n");
          break;
        default:
          break;
        }

        break;
      default:
        break;
      }
      ssd1331_showMenu8(&menu);
    }
  }
}

static void init_led() {
  ssd1306_setFixedFont(ssd1306xled_font6x8);
  ssd1331_96x64_spi_init(SPI_RES, SPI_CS, SPI_DC);
  ssd1331_96x64_init();
  ssd1331_setMode(LCD_MODE_NORMAL);
  ssd1331_clearScreen8();
  ssd1306_createMenu(&menu, menuItems, sizeof(menuItems) / sizeof(char *));
  ssd1331_showMenu8(&menu);
  // ssd1331_fillScreen8((uint8_t)0xFFFF);
  // ssd1331_setRgbColor(255, 255, 255);
  // ssd1331_fillScreen8((uint8_t)0xFFFF);
}

static void setup_gpio() {
  gpio_pullup_en(BTN_UP);
  gpio_pullup_en(BTN_DOWN);
  gpio_pullup_en(BTN_SELECT);
  gpio_set_direction(BTN_UP, GPIO_MODE_INPUT);
  gpio_set_direction(BTN_DOWN, GPIO_MODE_INPUT);
  gpio_set_direction(BTN_SELECT, GPIO_MODE_INPUT);
  gpio_set_intr_type(BTN_UP, GPIO_INTR_NEGEDGE);
  gpio_set_intr_type(BTN_DOWN, GPIO_INTR_NEGEDGE);
  gpio_set_intr_type(BTN_SELECT, GPIO_INTR_NEGEDGE);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BTN_UP, gpio_interrupt_handler, (void *)BTN_UP);
  gpio_isr_handler_add(BTN_DOWN, gpio_interrupt_handler, (void *)BTN_DOWN);
  gpio_isr_handler_add(BTN_SELECT, gpio_interrupt_handler, (void *)BTN_SELECT);
}

void app_main(void) {
  interuptQueue = xQueueCreate(10, sizeof(uint32_t));

  setup_gpio();
  init_led();

  xTaskCreate(&controlDisplay, "Control Display", 2048, NULL, 1, NULL);
  xTaskCreate(&startTask, "Start Task", 1024, NULL, 1, &task_1);
  xTaskCreate(&blinkLED, "Blink LED", 1024, NULL, 1, NULL);

  while (true) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  // if (gpio_get_level(26))
  //   ssd1306_menuUp(&menu);
  // if (gpio_get_level(27))
  //   ssd1306_menuDown(&menu);

  // ssd1331_showMenu8(&menu);
  // }
  // vTaskDelete(NULL);

  // while (true) {
  //   vTaskSuspend(task_2);
  //   vTaskDelay(2000 / portTICK_PERIOD_MS);
  //   vTaskResume(task_2);
  //   vTaskDelay(2000 / portTICK_PERIOD_MS);
  //
  //   if (task_1 != NULL) {
  //     vTaskDelete(task_1);
  //     task_1 = NULL;
  //   }
  // }
}
