// TEMPERATURE
#include "DHT.h"
#include <SSD1306.h>
#include <TaskScheduler.h>
#include <OneButton.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ssid = "telenet-38BBB15";
const char* password = "aRya4dsdeN3p";
bool connected = 0;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 3600);

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
#define CTRL_START 120000    //2 min
#define NTP_UPDATE 86400000  //1 Hour
#define SIZE_DATAS 24  //2min mean_temp
#define T_MARG_HIGH 0.15
#define T_MARG_LOW 0.0

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

//SPIFFS

// display
SSD1306 display(0x3c, 4, 15);

bool ntp_init = 0;
bool ntp_state = 0;

float last_temp_mean = 0;
float last_humi_mean = 0;
bool last_heat_state = 0;

Scheduler runner;

void btn_tick();
void temperature();
void wifi_wakeup();
void ntp();

Task task_btn_tick(40, TASK_FOREVER, &btn_tick);
Task task_temperature(TEMP_INTERVAL, TASK_FOREVER, &temperature);
Task task_wifi_wakeup(NTP_UPDATE, TASK_FOREVER, &wifi_wakeup);
Task task_ntp(NTP_UPDATE, TASK_FOREVER, &ntp);



OneButton BTN_P = OneButton(
  BTN_P_PIN,  // Input pin for the button
  true,        // Button is active LOW
  true         // Enable internal pull-up resistor
);

OneButton BTN_N = OneButton(
  BTN_N_PIN,  // Input pin for the button
  true,        // Button is active LOW
  true         // Enable internal pull-up resistor
);

OneButton BTN_T = OneButton(
  BTN_T_PIN,  // Input pin for the button
  true,        // Button is active LOW
  true         // Enable internal pull-up resistor
);

OneButton BTN_M = OneButton(
  BTN_M_PIN,  // Input pin for the button
  true,        // Button is active LOW
  true         // Enable internal pull-up resistor
);


void onBTN_P_clicked() {
  //Serial.println("P_C");
  if (user_mode == 0) {
    target_temp += 0.5;
    control();
    update_display();
  }
}

void onBTN_N_clicked() {
  //Serial.println("N_C");
  if (user_mode == 0) {
    target_temp -= 0.5;
    control();
    update_display();
  }
}

void onBTN_T_pressed() {
  //Serial.println("T_LP");
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
  Serial.println("M_LC");
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
  //Serial.println("T_C");
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

void begin_display() {
  //Reset pin for display !
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);  // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH);  // while OLED is running, must set GPIO16 in high、
  display.init();
  //display.flipScreenVertically();
  display.setBrightness(63);
}

String temp_to_string(float temp) {
  if (isnan(temp)){
    return "--.- °C";
  }

  if (temp >= 10 || temp < 0) {
    if (temp > -10) {
      return String(temp, 1) + "°C";
    } else {
      return String(temp, 0) + "°C";
    }
  } else {
    return "0" + String(temp, 1) + "°C";
  }
}

String humi_to_string(float humi) {
  if (isnan(humi)){
    return "H: --.- %";
  }
  return "H: "+String(humi, 1) + "%";
}

void time_setup(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  begin_display();
  timeClient.begin();
  delay(2000);
  if (WiFi.status() == WL_CONNECTED) {
    ntp_init = 1;
    ntp_state = 1;
    timeClient.update();
    Serial.println("Time updated !");
  }
  WiFi.disconnect(true);
}



void setup() {

  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, 0);
  BTN_P.attachClick(onBTN_P_clicked);
  BTN_N.attachClick(onBTN_N_clicked);
  BTN_T.attachClick(onBTN_T_clicked);
  BTN_T.attachLongPressStart(onBTN_T_pressed);
  BTN_M.attachLongPressStart(onBTN_M_pressed);
  dht.begin();
  time_setup();
  update_display();
  runner.init();
  runner.addTask(task_btn_tick);
  runner.addTask(task_temperature);
  runner.addTask(task_wifi_wakeup);
  runner.addTask(task_ntp);
  task_btn_tick.enable();
  task_temperature.enable();
  task_wifi_wakeup.enable();
}


//Update the OLED display
void update_display() {
  display.clear();

  //Left side
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  //Main T°C
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 4, temp_to_string(mean_temp));

  //Humidity
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 29, humi_to_string(mean_humi));

  //Time
  if (ntp_init){
    display.setFont(ArialMT_Plain_24);
    display.drawString(0, 40, timeClient.getFormattedTime().substring(0, 5));
  }

  //WiFi
  if (ntp_state) {
    display.setFont(ArialMT_Plain_10);
    display.drawString(60, 50, "@");
  }

  //Rigt side
  display.setTextAlignment(TEXT_ALIGN_RIGHT);

  //Target T°C
  display.setFont(ArialMT_Plain_16);
  display.drawString(128, 0, temp_to_string(target_temp));

  //Heat State
  display.setFont(ArialMT_Plain_10);
  if (digitalRead(RELAY_PIN) == 1) {
    display.drawString(128, 18, "Heating");
  }

  //User Mode
  display.setFont(ArialMT_Plain_10);
  if (user_mode == 0) {
    display.drawString(128, 38, "Manuel");
  } else if (user_mode == 1) {
    display.drawString(128, 38, "Auto");
  } 

  //Temp mode
  if (temp_mode == 0 && target_temp == temp_night) {
    display.drawString(128, 50, "Night");
  } else if (temp_mode == 1 && target_temp == temp_day) {
    display.drawString(128, 50, "Day");
  } else if (temp_mode == 2 && target_temp == temp_freeze) {
    display.drawString(128, 50, "Antigel");
  }
  display.display();
}

void temperature_mes() {
  //INDEX
  if (idx < SIZE_DATAS - 1){
    idx++;
  } else {
    idx=0;
  }
  //TEMP
  float t = dht.readTemperature();
  if (isnan(t)){
    Serial.println("Failed to read temp from DHT sensor!");
    //Add screen indicator ?
    return;
  }
  temps[idx] = t;
  float n = 0;
  mean_temp = 0;
  for (int i = 0; i < SIZE_DATAS; i++) {
    if (!isnan(temps[i])){
      mean_temp += temps[i];
      n++;
    }
  }
  mean_temp = mean_temp / n;
  
  //HUMI
  btn_tick();
  float h = dht.readHumidity();
  if (isnan(t)){
    Serial.println("Failed to read humi from DHT sensor!");
    //Add screen indicator ?
    return;
  }
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


void control() {
  if (millis() < CTRL_START){
    return;
  }
  if (digitalRead(RELAY_PIN) == 0) {
    if (mean_temp < (target_temp - T_MARG_HIGH)) {
      digitalWrite(RELAY_PIN, 1);
    }
  } else {
    if (temps[idx] > (target_temp - T_MARG_LOW)) {
      digitalWrite(RELAY_PIN, 0);
    }
  }
}

void temperature() {
  last_temp_mean = mean_temp;
  last_humi_mean = mean_humi;
  last_heat_state = digitalRead(RELAY_PIN);
  temperature_mes();
  control();

  if (last_temp_mean != mean_temp || last_humi_mean != mean_humi || last_heat_state != digitalRead(RELAY_PIN)){
      update_display();
  }
  Serial.println(String(temps[idx]) + "," + String(mean_temp) + "," + target_temp +","+String(humi[idx]) + "," + String(mean_humi) + "," + String(idx) + ","+String(user_mode)+","+String(digitalRead(RELAY_PIN)));   
}

void wifi_wakeup(){
  WiFi.reconnect();
  task_ntp.delay(5000);
}

void ntp(){
  if (WiFi.status() == WL_CONNECTED) {
    ntp_init = 1;
    ntp_state = 1;
    timeClient.update();
  } else {
    ntp_state = 0;
  }
  WiFi.disconnect(true);
}

void loop() {
  runner.execute();
}
