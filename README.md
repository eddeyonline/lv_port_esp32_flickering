# lv_port_esp32_flickering

A little vgl implementation on esp32 demonstrating flickering with wifi in AP mode.

This occurs using a 240x320 2.4" TFT LCD from AliExpress with ILI9341 display driver and XPT2046 touch driver.
The display works perfectly when wifi is not enabled or when the ESP32 is connected in STA mode. As soon as it is in AP or APSTA mode, without changing any other factors or connecting to the AP, the display has a constant flicker (maybe 20-30Hz just by looking at it).

esp32_pins_to_display indicates what esp32 pin should be connected to what pin on display module.
