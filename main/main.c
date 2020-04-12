/* LVGL Example project
 * 
 * Basic project to test LVGL on ESP32 based projects.
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

// Change whether AP or STA mode with define at line 55, note flicker in AP mode
// Fill in SSID and password for STA mode at lines 85-86


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h" //might not need this, deprecated
#include "nvs_flash.h"
#include "esp_freertos_hooks.h"
#include "esp_system.h"


/* Littlevgl specific */
#include "lvgl/lvgl.h"

#include "disp_spi.h"
#include "disp_driver.h"
#include "tp_spi.h"
#include "touch_driver.h"


/*********************
 *      DEFINES
 *********************/

#define MODE_AP_STA 1  //if this is 0 ESP32 will boot in AP mode, 1 will boot in STA mode

// Detect the use of a shared SPI Bus and verify the user specified the same SPI bus for both touch and tft
#if (CONFIG_LVGL_TOUCH_CONTROLLER == 1 || CONFIG_LVGL_TOUCH_CONTROLLER == 3) && TP_SPI_MOSI == DISP_SPI_MOSI && TP_SPI_CLK == DISP_SPI_CLK
#if CONFIG_LVGL_TFT_DISPLAY_SPI_HSPI == 1
#define TFT_SPI_HOST HSPI_HOST
#else
#define TFT_SPI_HOST VSPI_HOST
#endif
 
#if CONFIG_LVGL_TOUCH_CONTROLLER_SPI_HSPI == 1
#define TOUCH_SPI_HOST HSPI_HOST
#else
#define TOUCH_SPI_HOST VSPI_HOST
#endif

#if TFT_SPI_HOST != TOUCH_SPI_HOST
#error You must specifiy the same SPI host for both display and input driver
#endif

#define SHARED_SPI_BUS
#endif

 #define EXAMPLE_ESP_MAXIMUM_RETRY  5
// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#if MODE_AP_STA == 0
#define EXAMPLE_ESP_WIFI_SSID      "wifi_test"
#define EXAMPLE_ESP_WIFI_PASS      "password"
#define EXAMPLE_MAX_STA_CONN       4
static const char *TAG = "wifi AP mode";
#else
#define SSID  	"yourSSID" 
#define PASS  	"yourPassword"
static const char *TAG = "wifi STA mode";
#endif

static int s_retry_num = 0;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void IRAM_ATTR lv_tick_task(void);

#ifdef SHARED_SPI_BUS
/* Example function that configure two spi devices (tft and touch controllers) into the same spi bus */
static void configure_shared_spi_bus(void);
#endif

static void btn_event_cb(lv_obj_t * btn, lv_event_t event);

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_obj_t * cont;


/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Called when a button is released
 * @param btn pointer to the released button
 * @param event the triggering event
 * @return LV_RES_OK because the object is not deleted in this function
 */
static void btn_event_cb(lv_obj_t * btn, lv_event_t event)
{
    if(event == LV_EVENT_RELEASED) {
        lv_obj_t * label = lv_label_create(cont, NULL);
        lv_obj_t * btnLabel = lv_obj_get_child(btn, NULL);
        const char * txt = lv_label_get_text(btnLabel);
        printf("%s button pressed", txt);
        printf("button was pressed\n");
        lv_label_set_text(label, "Button Pressed");
    }
}



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}
void wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

#if MODE_AP_STA == 0
	wifi_config_t ap_wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) ); 
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_wifi_config) );
#else
	wifi_config_t sta_wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASS
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) ); 
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_wifi_config) );
#endif
    
    ESP_ERROR_CHECK(esp_wifi_start() );

#if MODE_AP_STA == 0
    ESP_LOGI(TAG, "Soft AP initialised. SSID:%s password:%s",
        EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
#else
	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 SSID, PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 SSID, PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    vEventGroupDelete(s_wifi_event_group);
#endif
    
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));   
}


void lv_tutorial_objects(void)
{

    /********************
     * CREATE A SCREEN
     *******************/
    /* Create a new screen and load it
     * Screen can be created from any type object type
     * Now a Page is used which is an objects with scrollable content*/
    static lv_style_t style;
    lv_style_copy(&style, &lv_style_plain);
    style.body.main_color = LV_COLOR_BLUE;
    style.body.grad_color = LV_COLOR_BLUE;
    style.text.color = LV_COLOR_WHITE;
    lv_obj_t * scr = lv_page_create(NULL, NULL);
    lv_page_set_style(scr, LV_PAGE_STYLE_BG, &style);
    lv_page_set_style(scr, LV_PAGE_STYLE_SCRL, &style);
    lv_page_set_sb_mode(scr, LV_SB_MODE_OFF);
    lv_disp_load_scr(scr);

    /****************
     * ADD A TITLE
     ****************/
    lv_obj_t * label = lv_label_create(scr, NULL); /*First parameters (scr) is the parent*/
    lv_label_set_text(label, "Test for flickering");  /*Set the text*/
    lv_obj_set_x(label, 30);                        /*Set the x coordinate*/

    /****************
     * ADD A STATUS FIELD
     ****************/

    cont = lv_cont_create(lv_scr_act(), NULL);              //Global static definition so accessible in CB
    lv_obj_set_auto_realign(cont, true);                    /*Auto realign when the size changes*/
    lv_obj_align_origo(cont, NULL, LV_ALIGN_CENTER, 0, 0);  /*This parametrs will be sued when realigned*/
    lv_cont_set_fit(cont, LV_FIT_TIGHT);
    lv_cont_set_layout(cont, LV_LAYOUT_COL_M);

    label = lv_label_create(cont, NULL);
    #if MODE_AP_STA == 0
    lv_label_set_text(label, "AP MODE");
	#else
	lv_label_set_text(label, "STA MODE");
	#endif
    

    /***********************
     * CREATE TWO BUTTONS
     ***********************/
    /*Create a button*/
    lv_obj_t * btn1 = lv_btn_create(lv_disp_get_scr_act(NULL), NULL);         /*Create a button on the currently loaded screen*/
    lv_obj_set_event_cb(btn1, btn_event_cb);                                  /*Set function to be called when the button is released*/
    lv_obj_align(btn1, scr, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);               /*Align below the label*/

    /*Create a label on the button (the 'label' variable can be reused)*/
    label = lv_label_create(btn1, NULL);
    lv_label_set_text(label, "<--");

    /*Copy the previous button*/
    lv_obj_t * btn2 = lv_btn_create(scr, btn1);                 /*Second parameter is an object to copy*/
    lv_obj_align(btn2, scr, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);    /*Align next to the prev. button.*/

    /*Create a label on the button*/
    label = lv_label_create(btn2, NULL);
    lv_label_set_text(label, "-->");

}

void app_main() {
   //Initialize NVS
  const static char* TAG = "app_main";
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

  lv_init();

  /* Interface and driver initialization */
#ifdef SHARED_SPI_BUS
  /* Configure one SPI bus for the two devices */
  configure_shared_spi_bus();
    
  /* Configure the drivers */
  disp_driver_init(false);
#if CONFIG_LVGL_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
  touch_driver_init(false);
#endif
#else
  /* Otherwise configure the SPI bus and devices separately inside the drivers*/
  disp_driver_init(true);
#if CONFIG_LVGL_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
  touch_driver_init(true);
#endif
#endif

  static lv_color_t buf1[DISP_BUF_SIZE];
  static lv_color_t buf2[DISP_BUF_SIZE];
  static lv_disp_buf_t disp_buf;
  lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

  lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.flush_cb = disp_driver_flush;
  disp_drv.buffer = &disp_buf;
  lv_disp_drv_register(&disp_drv);

#if CONFIG_LVGL_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
  lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.read_cb = touch_driver_read;
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  lv_indev_drv_register(&indev_drv);
#endif

  esp_register_freertos_tick_hook(lv_tick_task);

  lv_tutorial_objects();  

  while (1) {
    vTaskDelay(1);
    lv_task_handler();
  }


}

static void IRAM_ATTR lv_tick_task(void) {
  lv_tick_inc(portTICK_RATE_MS);
}

#ifdef SHARED_SPI_BUS
static void configure_shared_spi_bus(void)
{
  /* Shared SPI bus configuration */
  spi_bus_config_t buscfg = {
    .miso_io_num = TP_SPI_MISO,
    .mosi_io_num = DISP_SPI_MOSI,
    .sclk_io_num = DISP_SPI_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
#if CONFIG_LVGL_TFT_DISPLAY_CONTROLLER == TFT_CONTROLLER_ILI9341
    .max_transfer_sz = DISP_BUF_SIZE * 2,
#elif CONFIG_LVGL_TFT_DISPLAY_CONTROLLER == TFT_CONTROLLER_ST7789
    .max_transfer_sz = DISP_BUF_SIZE * 2,
#elif CONFIG_LVGL_TFT_DISPLAY_CONTROLLER == TFT_CONTROLLER_ILI9488
    .max_transfer_sz = DISP_BUF_SIZE * 3,
#elif CONFIG_LVGL_TFT_DISPLAY_CONTROLLER == TFT_CONTROLLER_HX8357
    .max_transfer_sz = DISP_BUF_SIZE * 2
#endif
  };

  esp_err_t ret = spi_bus_initialize(TFT_SPI_HOST, &buscfg, 1);
  assert(ret == ESP_OK);

  /* SPI Devices */
  disp_spi_add_device(TFT_SPI_HOST);
  tp_spi_add_device(TOUCH_SPI_HOST);
}
#endif
