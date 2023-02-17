// TEMPERATURE
#include <OneWire.h>
#include <DallasTemperature.h>
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
#define ONE_WIRE_BUS 17
#define RX1PIN 35
#define TX1PIN 13
#define BTN_P_PIN 13
#define BTN_N_PIN 12
#define BTN_T_PIN 21
#define BTN_M_PIN 23

#define TEMP_INTERVAL 5000  //10sec
#define CTRL_START 60000    //1 min
#define NTP_UPDATE 86400000  //1 Hour
#define SIZE_TEMPS 24  //1min mean
#define T_OFFSET -0.7
#define T_MARG_HIGH 0.2
#define T_MARG_LOW 0.0
#define VAR_STABLE 0.05

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temps[SIZE_TEMPS];
float mean;
float var;
bool init_temp = 0;  //stable measure
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

int last_temp_mean = 0;
bool last_heat_state = 0;

void btn_tick();
void temperature();
void ntp();

Task task_btn_tick(40, TASK_FOREVER, &btn_tick);
Task task_temperature(TEMP_INTERVAL, TASK_FOREVER, &temperature);
Task task_ntp(NTP_UPDATE, TASK_FOREVER, &ntp);

Scheduler runner;

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
  display.flipScreenVertically();
  display.setBrightness(63);
}

String temp_to_string(float temp) {
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




void setup() {

  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, 0);
  BTN_P.attachClick(onBTN_P_clicked);
  BTN_N.attachClick(onBTN_N_clicked);
  BTN_T.attachClick(onBTN_T_clicked);
  BTN_T.attachLongPressStart(onBTN_T_pressed);
  BTN_M.attachLongPressStart(onBTN_M_pressed);
  sensors.begin();
  sensors.requestTemperatures();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  timeClient.begin();
  begin_display();
  delay(5000);
  sensors.requestTemperatures();
  temps[0] = sensors.getTempCByIndex(0) + T_OFFSET;
  for (int i = 1; i < SIZE_TEMPS; i++) {
    temps[i] = temps[0];
  }
  update_display();
  runner.init();
  runner.addTask(task_btn_tick);
  runner.addTask(task_temperature);
  runner.addTask(task_ntp);
  task_btn_tick.enable();
  task_temperature.enable();
  task_ntp.enable();
}


//Update the OLED display
void update_display() {
  display.clear();

  //Left side
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  //Main T°C
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 4, temp_to_string(mean));


  //Time
  if (!ntp_init)
    display.setFont(ArialMT_Plain_24);
  display.drawString(0, 40, timeClient.getFormattedTime().substring(0, 5));

  //WiFi
  if (WiFi.status() == WL_CONNECTED) {
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

void temperature_mes(bool setup = 0) {
  var = 0;
  mean = 0;
  //Shift all values (more perfomant way ?)
  for (int i = SIZE_TEMPS - 1; i > 0; i--) {
    temps[i] = temps[i - 1];
    mean += temps[i];
    var += temps[i] * temps[i];
  }
  //Add new value
  
  //Serial.println("Temp duration :"+String((millis()-last_temp)));
  sensors.requestTemperatures();
  btn_tick();
  temps[0] = sensors.getTempCByIndex(0) + T_OFFSET;
  mean += temps[0];
  var += temps[0] * temps[0];
  mean = mean / SIZE_TEMPS;
  var = (var / SIZE_TEMPS) - (mean * mean);
  
  if (var < 0) { var = -var; }
  if (setup == 0 && init_temp == 0 && var < VAR_STABLE) {
    init_temp = 1;
  }
}


void control() {
  if (init_temp == 1) {  
    if (digitalRead(RELAY_PIN) == 0) {
      if ((mean < (target_temp - T_MARG_LOW)) && (temps[0] < (target_temp - T_MARG_HIGH))) {
        digitalWrite(RELAY_PIN, 1);
      }
    } else {
      if ((mean > (target_temp - T_MARG_HIGH)) && (temps[0] > (target_temp - T_MARG_LOW))) {
        digitalWrite(RELAY_PIN, 0);
      }
    }
  }
}

void temperature() {
  last_temp_mean = mean;
  last_heat_state = digitalRead(RELAY_PIN);
  temperature_mes(millis() < CTRL_START);
  control();
  if (last_temp_mean != mean || last_heat_state != digitalRead(RELAY_PIN)){
      update_display();
  }
  Serial.println(String(temps[0]) + "," + String(mean) + "," + target_temp + "," + String(var)+""+String(user_mode)+","+String(digitalRead(RELAY_PIN)));   
}

void ntp(){
  if (WiFi.status() == WL_CONNECTED && ntp_init == 0) {
    if (ntp_init == 0) {
      ntp_init = 1;
    }
    timeClient.update();
  }
}

void loop() {
  runner.execute();
}
