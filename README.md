# ESP32 - THERMOSTAT

This is a thermostat build for my home usage using:

- Heltec ESP32 WiFi LoRa 32
- SSD1306 OLED display
- DHT22 temperature and humidity sensor
- 5V DC Relay

It doesn't use the LoRa and only connect to the WiFi occasionally to get NTP synced time.

## Current state

It currently sync the time over WiFi and NTP, read the temperature and humidity, use the button to change the target temperature, display all available informations on the display with nice icon, and of course control the temperature with the relay.

![Proto image](https://github.com/OscarVsp/ESP32-Thermostat/blob/main/images/proto.jpg?raw=true)

## TODO

- Menu: have a small GUI menu to changes the values
- EEPROM config: save the config on the EEPROM
- Auto mode: make the programmable mode with time
- Custom PCB
- 3D printed case for wall mount


