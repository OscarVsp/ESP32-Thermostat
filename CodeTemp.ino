/*
  INCLUDES
*/
#include "DHT.h"
#include <SSD1306.h>
#include <TaskScheduler.h>
#include <OneButton.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

/*
  DEFINES
*/

#define RELAY_PIN 0
#define DHTPIN 17
#define DHTTYPE DHT22   // DHT 22  (AM2302)
#define RX1PIN 35
#define TX1PIN 13
#define BTN_P_PIN 13
#define BTN_N_PIN 12
#define BTN_T_PIN 21
#define BTN_M_PIN 23

#define TEMP_INTERVAL 5000  //5sec
#define CTRL_START 30000    //15 min
#define NTP_UPDATE 86400000  //1 Hour
#define NTP_RETRY 10000  //10 sec
#define SIZE_DATAS 6  //30sec mean_temp
#define T_MARG_HIGH 0.15
#define T_MARG_LOW 0.05

/*
  INITIALIZATIONS
*/

//Display
SSD1306 display(0x3c, 4, 15);

//WiFi & ntp
const char* ssid = "telenet-38BBB15";
const char* password = "aRya4dsdeN3p";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 3600);
bool wifi_state = 0;
bool ntp_state = 0;

//Temp & Humi sensor
DHT dht(DHTPIN, DHTTYPE);
float temps[SIZE_DATAS] = {NAN};
float humi[SIZE_DATAS] = {NAN};
int idx = -1;
float mean_temp;
float mean_humi;
bool user_mode = 0;        //0 Manual, 1 Auto
int temp_mode = 0;      //0 nuit, 1 jour, 2 antigel
float target_temp = 15;   //day = 18, night = 15, freeze = 5
float temp_day = 18;    //temp_mode = 1
float temp_night = 15;    //temp_mode = 0
float temp_freeze = 5;    //temp_mode = 2
float last_temp_mean = 0;
float last_humi_mean = 0;
bool last_heat_state = 0;
bool temp_error = 0;
bool humi_error = 0;

//Tasks
Scheduler runner;
void btn_tick();
void temperature();
void wifi_wakeup();
void ntp();
void update_display();
Task task_btn_tick(40, TASK_FOREVER, &btn_tick);
Task task_temperature(TEMP_INTERVAL, TASK_FOREVER, &temperature);
Task task_wifi_wakeup(NTP_UPDATE, TASK_FOREVER, &wifi_wakeup);
Task task_ntp(NTP_RETRY, TASK_FOREVER, &ntp);
Task task_update_display(500, TASK_FOREVER, &update_display);

//Buttons
OneButton BTN_P = OneButton(BTN_P_PIN,true,true); // Input pin for the button, Button is active LOW, Enable internal pull-up resistor
OneButton BTN_N = OneButton(BTN_N_PIN,true,true);
OneButton BTN_T = OneButton(BTN_T_PIN,true,true);
OneButton BTN_M = OneButton(BTN_M_PIN,true,true);

void onBTN_P_clicked() {
  if (user_mode == 0) {
    target_temp += 0.5;
    control();
    update_display();
  }
}

void onBTN_N_clicked() {
  if (user_mode == 0) {
    target_temp -= 0.5;
    control();
    update_display();
  }
}

void onBTN_T_pressed() {
  user_mode == 0;
  if (temp_mode == 2) {
    temp_mode = 1;
    target_temp = temp_day;
    control();
    update_display();
  } else {
    temp_mode = 2;
    target_temp = temp_freeze;
    control();
    update_display();
  }
}

void onBTN_M_pressed(){
  user_mode = !(user_mode);
  if (user_mode == 0){
    temp_mode = 1;
    target_temp = temp_day;
    control();
    update_display();
  } else if (user_mode == 1){
    //TODO schedule
    temp_mode = 1;
    target_temp = temp_day;
    control();
    update_display();
  }
}

void onBTN_T_clicked() {
  if (user_mode == 0) {
    temp_mode = !temp_mode ;
    if (temp_mode == 0) {
      target_temp = temp_night;
    } else {      
      target_temp = temp_day;
    }
    control();
    update_display(); 
  }
}

void btn_tick(){
  BTN_P.tick();
  BTN_N.tick();
  BTN_T.tick();
}



//Utils
String temp_to_string(float temp) {
  if (isnan(temp)){return "--.- °";}
  if (temp >= 10 || temp < 0) {if (temp > -10) {return String(temp, 1) + "°";} else {return String(temp, 0) + "°";}} else {return "0" + String(temp, 1) + "°";}
}

String humi_to_string(float humi) {
  if (isnan(humi)){return "--.- %";} else {return ""+String(humi, 1) + "%";}
}

// 'drop', 16x10px
const unsigned char epd_bitmap_drop [] PROGMEM = {
	0x00, 0x1f, 0xc0, 0x3f, 0x70, 0x7c, 0x1c, 0xfc, 0x0e, 0xfc, 0x0e, 0xfc, 0x1c, 0xfc, 0x70, 0x7c,
	0xc0, 0x3f, 0x00, 0x1f
};

// 'flame', 16x16px
const unsigned char epd_bitmap_flame [] PROGMEM = {
  0x00,	0x00, 0x00, 0x0e, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x9f, 0xc0, 0x07, 0xe0, 0x01, 0xfe, 0x03, 
	0xfe, 0x00, 0x7c, 0x00, 0xf8, 0x01, 0xe0, 0xc7, 0xc8, 0x7f, 0xf0, 0x3f, 0xc0, 0x1f, 0x00, 0x02
};
// 'auto', 16x16px
const unsigned char epd_bitmap_auto [] PROGMEM = {
	0xe0, 0x07, 0xf8, 0x1f, 0x1c, 0x38, 0x0e, 0x70, 0x06, 0x60, 0x03, 0xc0, 0x03, 0xc0, 0xf3, 0xc1,
	0x03, 0xc3, 0x03, 0xc2, 0x03, 0xc6, 0x06, 0x64, 0x0e, 0x70, 0x1c, 0x38, 0xf8, 0x1f, 0xe0, 0x07
};
// 'day', 16x16px
const unsigned char epd_bitmap_day [] PROGMEM = {
	0x80, 0x01, 0x80, 0x01, 0x8c, 0x31, 0x1c, 0x30, 0xd8, 0x1b, 0xe0, 0x07, 0x30, 0x0c, 0x37, 0xec,
	0x37, 0xec, 0x30, 0x0c, 0xe0, 0x07, 0xd8, 0x0b, 0x1c, 0x38, 0x8c, 0x31, 0x80, 0x01, 0x80, 0x01
};
// 'night', 16x16px
const unsigned char epd_bitmap_night [] PROGMEM = {
	0xe0, 0x07, 0xf8, 0x1f, 0x1c, 0x38, 0x06, 0x60, 0x06, 0x60, 0x03, 0xc0, 0x03, 0xc0, 0x7b, 0xc0,
	0xff, 0xc1, 0x83, 0xc3, 0x01, 0xc6, 0x00, 0x64, 0x00, 0x6c, 0x00, 0x3c, 0x00, 0x1c, 0x00, 0x04
};
// 'freeze', 16x16px
const unsigned char epd_bitmap_freeze [] PROGMEM = {
	0x80, 0x01, 0x80, 0x01, 0xc0, 0x03, 0x88, 0x11, 0x96, 0x69, 0x98, 0x19, 0xfe, 0x7d, 0xc0, 0x03,
	0xc0, 0x03, 0xfe, 0x7d, 0x98, 0x19, 0x96, 0x69, 0x88, 0x11, 0xc0, 0x03, 0x80, 0x01, 0x80, 0x01
};
// 'target', 16x16px
const unsigned char epd_bitmap_target [] PROGMEM = {
	0x00, 0x00, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 
	0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01, 0x80, 0x01, 0x00, 0x00
};
// 'sensor', 24x10px
const unsigned char epd_bitmap_sensor [] PROGMEM = {
	0x00, 0x00, 0x1f, 0x00, 0x80, 0x20, 0xfe, 0x7f, 0x4e, 0x03, 0x00, 0x9f, 0x01, 0xff, 0xbf, 0x01,
	0xff, 0xbf, 0x03, 0x00, 0x9f, 0xfe, 0x7f, 0x4e, 0x00, 0x80, 0x20, 0x00, 0x00, 0x1f
};

/*
  BEGINS
*/
void begin_display() {
  pinMode(16, OUTPUT);  //Reset pin for display !
  digitalWrite(16, LOW);  // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH);  // while OLED is running, must set GPIO16 in high、
  display.init();
  //display.flipScreenVertically();
  display.setBrightness(63);
}

void setup() {

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, 0);
  BTN_P.attachClick(onBTN_P_clicked);
  BTN_N.attachClick(onBTN_N_clicked);
  BTN_T.attachClick(onBTN_T_clicked);
  BTN_T.attachLongPressStart(onBTN_T_pressed);
  BTN_M.attachLongPressStart(onBTN_M_pressed);
  begin_display();
  dht.begin();
  timeClient.begin();
  update_display();
  runner.init();
  runner.addTask(task_btn_tick);
  runner.addTask(task_temperature);
  runner.addTask(task_wifi_wakeup);
  runner.addTask(task_ntp);
  runner.addTask(task_update_display);
  task_btn_tick.enable();
  task_temperature.enable();
  task_wifi_wakeup.enable();
  task_update_display.enable();
}

/*
  LOOPS
*/

//Update the OLED display
void update_display() {
  display.clear();
  if (task_update_display.isEnabled()){
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 10, "Loading...");
    float p = (millis()*100)/CTRL_START;
    display.drawProgressBar(14,40,100,10,p);
    display.display();
    return;
  }
//Left side
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  //Main T°C
  display.setFont(ArialMT_Plain_24);
  display.drawFastImage(0, 0, 10, 24, epd_bitmap_sensor);
  display.drawString(12, 0, temp_to_string(mean_temp));
  //Humidity
  display.setFont(ArialMT_Plain_16);
  display.drawFastImage(0, 25, 10, 16, epd_bitmap_drop);
  display.drawString(12, 25, humi_to_string(mean_humi));
  //Time
  if (timeClient.isTimeSet()){
    display.setFont(ArialMT_Plain_24);
    display.drawString(0, 42, timeClient.getFormattedTime().substring(0, 5));
  }
  //WiFi
  if (!ntp_state) {
    display.setFont(ArialMT_Plain_10);
    display.drawString(60, 42, "!e ntp");
  }
  if (!wifi_state && WiFi.status() != WL_CONNECTED) {
    display.setFont(ArialMT_Plain_10);
    display.drawString(60, 54, "!e @");
  }
//Rigt side
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  //Target T°C
  display.setFont(ArialMT_Plain_16);
  display.drawFastImage(75, 0, 16, 16, epd_bitmap_target);
  display.drawString(128, 0, temp_to_string(target_temp));
  //States
  if (user_mode == 1) {display.drawFastImage(70, 25, 16, 16, epd_bitmap_auto);}
  if (temp_mode == 0 && target_temp == temp_night) {display.drawFastImage(90, 25, 16, 16, epd_bitmap_night);} else if (temp_mode == 1 && target_temp == temp_day) {display.drawFastImage(90, 25, 16, 16, epd_bitmap_day);} else if (temp_mode == 2 && target_temp == temp_freeze) {display.drawFastImage(90, 25, 16, 16, epd_bitmap_freeze);}
  if (digitalRead(RELAY_PIN) == 1) {display.drawFastImage(110, 25, 16, 16, epd_bitmap_flame);}
  //errors
  display.setFont(ArialMT_Plain_10);
  if(temp_error){display.drawString(128, 42, "!e T°C");}
  if(humi_error){display.drawString(128, 54, "!e H%");}
  
  display.display();
}

void temperature_mes() {
  if (idx < SIZE_DATAS - 1){idx++;} else {idx=0;}  //INDEX
  float t = dht.readTemperature(); //TEMP
  float n = 0;
  if (isnan(t)){
    temp_error = 1;
  } else {
    temp_error = 0;
    temps[idx] = t;
    mean_temp = 0;
    for (int i = 0; i < SIZE_DATAS; i++) {
      if (!isnan(temps[i])){
        mean_temp += temps[i];
        n++;
      }
    }
    mean_temp = mean_temp / n;
  }
  
  btn_tick();
  float h = dht.readHumidity();//HUMI
  if (isnan(t)){
    humi_error = 1;
  } else {
    humi_error = 0;
    n = 0;
    humi[idx] = h;
    mean_humi = 0;
    for (int i = 0; i < SIZE_DATAS; i++) {
      if (!isnan(humi[i])){
        mean_humi += humi[i];
        n++;
      }
    }
    mean_humi = mean_humi / n;
  }
}

void control() {
  if (task_update_display.isEnabled()){
    if (millis() >= CTRL_START){task_update_display.disable();}
    return;
  }
  
  if (digitalRead(RELAY_PIN) == 0) {if (mean_temp < (target_temp - T_MARG_HIGH)) {digitalWrite(RELAY_PIN, 1);}
  } else {if (temps[idx] > (target_temp - T_MARG_LOW)) {digitalWrite(RELAY_PIN, 0);}}
}

void temperature() {
  last_temp_mean = mean_temp;
  last_humi_mean = mean_humi;
  last_heat_state = digitalRead(RELAY_PIN);
  temperature_mes();
  control();

  if (last_temp_mean != mean_temp || last_humi_mean != mean_humi || last_heat_state != digitalRead(RELAY_PIN)){update_display();}
  Serial.println(String(temps[idx]) + "," + String(mean_temp) + "," + target_temp +","+String(humi[idx]) + "," + String(mean_humi) + "," + String(idx) + ","+String(user_mode)+","+String(digitalRead(RELAY_PIN)));   
}

void wifi_wakeup(){
  task_wifi_wakeup.disable(); 
  WiFi.reconnect();
  task_ntp.enableDelayed(NTP_RETRY);
}

void ntp(){
  wifi_state = (WiFi.status() == WL_CONNECTED);
  if (wifi_state) {
    task_ntp.disable();
    ntp_state = timeClient.forceUpdate();
    if (ntp_state){
      WiFi.disconnect();
      task_wifi_wakeup.enableDelayed(NTP_UPDATE);
      update_display();
    } else {task_ntp.enableDelayed(NTP_RETRY);}
  }
}


void loop() {
  runner.execute();
}
