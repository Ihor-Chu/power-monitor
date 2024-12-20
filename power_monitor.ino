
#include "user_config.h"

extern "C" {
#include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "ThingSpeak.h"

#include <ESP8266WebServer.h>
//#include <ESP8266HTTPClient.h>

#include "TickTwo.h"

#include "data.h"

//#include "Array.h"



//typedef Array<calibrate_s, MAX_CALIBRATE> calibrate_array_t;

//calibrate_array_t calibrate_data;
bool is_calibrate_data_dirty = false;

calibrate_eeprom calibrate_eeprom_data;


#include <Arduino_CRC32.h>
Arduino_CRC32 crc32;



rtc_data_s rtc_data_;

void save_rtc_data() {
  rtc_data_.crc32 = crc32.calc((uint8_t*)&rtc_data_ + sizeof(uint32_t), sizeof(rtc_data_) - sizeof(uint32_t));
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_data_, sizeof(rtc_data_));
}

bool load_rtc_data() {

  ESP.rtcUserMemoryRead(0, (uint32_t*)&rtc_data_, sizeof(rtc_data_));
  uint32_t crc32_int = crc32.calc((uint8_t*)&rtc_data_ + sizeof(uint32_t), sizeof(rtc_data_) - sizeof(uint32_t));
  if (crc32_int == rtc_data_.crc32) {
    Serial.println("RTC data valid");
    return true;
  } else {
    Serial.println("RTC data invalid, clean it");
    memset((uint8_t*)&rtc_data_, 0, sizeof(rtc_data_));
    return false;
  };
}





/*
rtc_flag_s rtc_flag_data;;

void set_rtc_flag(uint32_t value)
{
    rtc_flag_data.flag = value;
    rtc_flag_data.crc32 = crc32.calc((uint8_t*)&rtc_flag_data.flag, sizeof(rtc_flag_data.flag));    
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_flag_data, sizeof(rtc_flag_data));
}
bool verify_rtc_flag()
{
  ESP.rtcUserMemoryRead(0, (uint32_t *)&rtc_flag_data, sizeof(rtc_flag_data));
  uint32_t crc32_int  = crc32.calc((uint8_t*)&rtc_flag_data.flag, sizeof(rtc_flag_data.flag));  
  Serial.printf("%s: compare %04x && %04x\n", __func__, crc32_int, rtc_flag_data.crc32);
  Serial.println();  
  if (crc32_int == rtc_flag_data.crc32)
  {
     Serial.println("RTC data valid");
     return true;
  } else {
    Serial.println("RTC data invalid");
     return false;
  };
}
*/

// #include <EEPROM_Rotate.h>
// EEPROM_Rotate EEPROMr;
#include <EEPROM.h>

#define PIN_GREEN_LED 4
#define INTERNAL_BLUE_LED 2


ESP8266WebServer webserver(80);

WiFiClient wifi_client;

// change it
const char* myssid = "********";
const char* mypassword = "********";

const char* ssid = "*******";
const char* password = "*****************";

const char* thingspeak_api_key = "****************";
int thingspeak_channel_id = 0000000;
// /change it

#define SETUP_TIMEOUT 5 * 60 * 1000   /* 5 min in ms */
#define MEASURE_TIMEOUT 4 * 60 * 1000 /* every 4 min, in ms */
void do_measure_and_report();
void do_measure();
void do_ThingSpeak_report();
/*
void do_push_periodic()
{
  do_measure_and_report();
  if (is_calibrate_data_dirty)
  {
    save_calibrate_data();  
  }
}
*/
void do_green_blink() {
  digitalWrite(PIN_GREEN_LED, !digitalRead(PIN_GREEN_LED));
};
TickTwo tickerObject_do_measure(do_measure, MEASURE_TIMEOUT);
TickTwo tickerObject_do_report(do_ThingSpeak_report, 15 * 1000 + 500);
TickTwo tickerObject_do_blink(do_green_blink, 300);


static const char html_head[] PROGMEM = "<!DOCTYPE html>"
                                        "<html>"
                                        "<head>"
                                        "<style>\n"
                                        "body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\n"
                                        "table {\n"
                                        "table-layout:fixed;\n"
                                        "}\n"

                                        "</style></head>";

String inet_ssid;
String inet_pass;
int adc_value = 0;
bool connected = false;
bool setup_mode = true;
bool run_once = true;

unsigned long last_web_access = 0;
void handleRoot() {
  String state = webserver.arg("h");
  webserver.send(200, "text/html", "<h1>You are connected</h1><br>h=" + state + "<br>a0=" + String(adc_value));
  last_web_access = millis();
}
void handleCalibrateTable() {
  digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
  String response = FPSTR(html_head);
  response += "<body><table>";

  for (uint8_t i = 0; i < calibrate_eeprom_data.size; i++) {
    response += "<tr><td>" + String(calibrate_eeprom_data.data[i].adc) + "<td>" + String(calibrate_eeprom_data.data[i].mvolt);
    if (calibrate_eeprom_data.size > 2) {
      response += "<td><form action=/cm method=get><input type=submit name=rm value='Remove'><input type=hidden name=remove value='" + String(i) + "' ></form>";
    } else {
      response += "<td>&nbsp;";
    };
  };
  if (calibrate_eeprom_data.size < MAX_CALIBRATE) {
    response += "<tr><form action=/cm method=get><td><input type=text name=add_adc value=" + String(adc_value) + "><td><input type=text name=add_mv value=12000><td><input type=submit name=add_btn value=Add></form>";
  };
  response += "</table></body></html>";
  webserver.send(200, "text/html", response);
  last_web_access = millis();
  digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
}





void handleCalibrateTable_modify() {
  digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
  Serial.println("handleCalibrateTable_modify");
  String message = "";
  for (uint8_t i = 0; i < webserver.args(); i++) { message += " " + webserver.argName(i) + ": " + webserver.arg(i) + "\n"; }
  Serial.println(message);
  if (webserver.hasArg("add_btn")) {
    Serial.println("need add some");
    String str_adc = webserver.arg("add_adc");
    String str_mv = webserver.arg("add_mv");

    do_add_calibrate(&calibrate_eeprom_data, &is_calibrate_data_dirty, atoi(str_adc.c_str()), atoi(str_mv.c_str()));
  };
  if (webserver.hasArg("remove")) {
    int rm_idx = atoi(webserver.arg("remove").c_str());
    Serial.println("need rm some");
    do_rm_calibrate(&calibrate_eeprom_data, &is_calibrate_data_dirty, rm_idx);
  };
  save_calibrate_data();
  String response = FPSTR(html_head);
  response += "<head><meta http-equiv=\"Refresh\" content=\"1; url=/c\"></head></html>";
  webserver.send(200, "text/html", response);

  last_web_access = millis();
  digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
}
void init_calibrate_data() {
  //c_arr.clear();

  //  EEPROMr.size(4);
  //  EEPROMr.begin(4096);
  EEPROM.begin(512);
  EEPROM.get(0, calibrate_eeprom_data);
  uint8_t* p = (uint8_t*)(&calibrate_eeprom_data);
  for (uint8_t i = 0; i < sizeof(calibrate_eeprom_data); i++) {
    //    uint8_t x = EEPROMr.read(i);
    uint8_t x = *(p + i);
    Serial.printf("read from eeprom %02x to %p\n", x, p + i);
    Serial.println();
    //    *(p+i) = x;
  };
  uint8_t* q = (uint8_t*)&calibrate_eeprom_data.size;
  uint32_t const crc32_res = crc32.calc(q, sizeof(calibrate_eeprom_data) - sizeof(uint32_t));
  Serial.printf("compare %04x && %04x\n", crc32_res, calibrate_eeprom_data.crc32);
  Serial.println();
  if (crc32_res == calibrate_eeprom_data.crc32) {
    Serial.println("eeprom data is valid, apply it");
    is_calibrate_data_dirty = false;
    //    for (uint8_t i = 0; i < calibrate_eeprom_data.size; i++) {
    //      c_arr.push_back(calibrate_eeprom_data.data[i]);
    //    }
  } else {
    Serial.println("eeprom data is invalid; use default");
    is_calibrate_data_dirty = true;
    calibrate_eeprom_data.size = 2;
    calibrate_eeprom_data.data[0] = { 0, 0 };
    calibrate_eeprom_data.data[1] = { 750, 12000 };
    //    c_arr.push_back({ 0, 0 });
    //c_arr.push_back({ 750, 12000 });
  }
}

void save_calibrate_data() {
  if (!is_calibrate_data_dirty) {
    Serial.println("save_calibrate_data - skip");
    return;
  };

  //for (uint8_t i = 0; i < calibrate_eeprom_data.size; i++) {
  //  calibrate_eeprom_data.data[i] = c_arr[i];
  //};
  calibrate_eeprom_data.id = EEPROM_DATA_ID;
  uint8_t* q = (uint8_t*)&calibrate_eeprom_data.size;
  calibrate_eeprom_data.crc32 = crc32.calc(q, sizeof(calibrate_eeprom_data) - sizeof(uint32_t));
  EEPROM.put(0, calibrate_eeprom_data);
  uint8_t* p = (uint8_t*)(&calibrate_eeprom_data);
  for (uint8_t i = 0; i < sizeof(calibrate_eeprom_data); i++) {
    uint8_t x = *(p + i);
    Serial.printf("write to eeprom %02x to %d\n", x, i);
    Serial.println();
    //EEPROMr.write(i, x);
  };
  if (EEPROM.commit()) {
    Serial.println("Commit success");
  } else {
    Serial.println("Commit unsuccess");
  };
  is_calibrate_data_dirty = false;
}


WiFiEventHandler wifiConnectHandler;
void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi sucessfully.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  connected = true;
}

void init_wifi() {
  inet_ssid = ssid;
  inet_pass = password;

  wifi_fpm_do_wakeup();  // ?

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.softAP(myssid, mypassword);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  WiFi.begin(inet_ssid.c_str(), inet_pass.c_str());
  Serial.printf("Wait for WiFi '%s:%s' ...\n", inet_ssid.c_str(), inet_pass.c_str());
  Serial.println();
  for (uint8_t i = 0; i < 10; i++) {
    if (connected) break;
    Serial.println(".");
    delay(1000);
  }
  WiFi.printDiag(Serial);
  IPAddress local_ip = WiFi.localIP();
  Serial.print("AP IP address: ");
  Serial.println(local_ip);
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(A0, INPUT);
  pinMode(INTERNAL_BLUE_LED, OUTPUT);
  pinMode(PIN_GREEN_LED, OUTPUT);
  digitalWrite(INTERNAL_BLUE_LED, LOW);
  digitalWrite(PIN_GREEN_LED, HIGH);
  delay(1000);
  Serial.println("Reset reason: " + ESP.getResetReason());
  Serial.println(system_get_rst_info()->reason);  // 5 REASON_DEEP_SLEEP_AWAKE
  init_calibrate_data();
  save_calibrate_data();
  load_rtc_data();
  if (system_get_rst_info()->reason == REASON_DEEP_SLEEP_AWAKE /*&& verify_rtc_flag()*/) {
    setup_mode = false;
  };
  /*
//   make rtc flag invalid to manual reset
  rtc_flag_data.flag=0;
  rtc_flag_data.crc32=0;
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_flag_data, sizeof(rtc_flag_data));
*/
  load_rtc_data();
  ESP.eraseConfig();
  init_wifi();

  if (setup_mode) {
    Serial.println("start webserver");
    webserver.on("/", handleRoot);
    webserver.on("/c", handleCalibrateTable);
    webserver.on("/cm", handleCalibrateTable_modify);
    webserver.begin();
    tickerObject_do_measure.start();
    tickerObject_do_report.start();  //start the ticker.
  };
  tickerObject_do_blink.start();
}

/*
  void fpm_wakup_cb_func(void) {
  Serial.println("Light sleep is over, either because timeout or external interrupt");
  Serial.flush();
  }
*/

void yield_delay(unsigned long val) {
  unsigned long start_ = millis();
  while (val > millis() - start_) {
    tickerObject_do_blink.update();
    yield();
  }
}





int report_id;

void do_ThingSpeak_report() {
  if (report_id == 1) {
    digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
    ThingSpeak.begin(wifi_client);
    Serial.printf("send to ThingSpeak plot 1 %d\n", adc_value);
    Serial.println();
    ThingSpeak.writeField(thingspeak_channel_id, 1, adc_value, thingspeak_api_key);
    report_id--;
    digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
    return;
  };
  if (report_id == 2) {
    digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
    ThingSpeak.begin(wifi_client);
    float v = (float)((float)adc_to_mv(adc_value, &calibrate_eeprom_data) / 1000.0);
    Serial.printf("send to ThingSpeak plot 2 %f\n", v);
    Serial.println();
    ThingSpeak.writeField(thingspeak_channel_id, 2, v, thingspeak_api_key);
    report_id--;
    digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
    return;
  };
  if (report_id == 3) {
    digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
    ThingSpeak.begin(wifi_client);
    adc_data_window_s * adc_data_ = &rtc_data_.adc_data;
    float v = (float)((float)adc_to_mv(get_average_adc(adc_data_), &calibrate_eeprom_data) / 1000.0);
    Serial.printf("send to ThingSpeak plot 3 %f\n", v);
    Serial.println();
    ThingSpeak.writeField(thingspeak_channel_id, 3, v, thingspeak_api_key);
    report_id--;
    digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
    return;
  };


  report_id = 0;
};
void do_measure() {
  adc_value = analogRead(A0);
  if (adc_value < 300) {
    Serial.printf("to low voltage %d, skip report", adc_value);
    Serial.println();
    return;
  };
  add_measure_to_adc_data(adc_value, &rtc_data_.adc_data);
  save_rtc_data();
  report_id = 3;  // report later
}
/*
void do_measure_and_report() {
  ThingSpeak.begin(wifi_client);
  digitalWrite(INTERNAL_BLUE_LED, !digitalRead(INTERNAL_BLUE_LED));
  adc_value = analogRead(A0);
  unsigned int v = adc_to_mv(adc_value);
  Serial.printf("send to ThingSpeak %d and %d\n", adc_value, v);
  ThingSpeak.writeField(thingspeak_channel_id, 1, adc_value, thingspeak_api_key);
  yield_delay(10000);
  ThingSpeak.writeField(thingspeak_channel_id, 2, (float)((float)v / 1000.0), thingspeak_api_key);
  Serial.println("send to ThingSpeak done");
}
*/
void loop() {
  // put your main code here, to run repeatedly:
  tickerObject_do_blink.update();
  if (setup_mode) {
    if (run_once) {
      run_once = false;
      do_measure();
      //do_measure_and_report();
    };
    tickerObject_do_measure.update();
    tickerObject_do_report.update();

    webserver.handleClient();
    if (millis() - last_web_access > SETUP_TIMEOUT) {
      setup_mode = false;
      Serial.println("setup done, can sleep");
    };
    yield();
  } else {
    do_measure();
    tickerObject_do_blink.update();
    unsigned long started = millis();
    do_ThingSpeak_report();
    yield_delay(15000 - (millis() - started) + 500);
    started = millis();
    do_ThingSpeak_report();
    yield_delay(15000 - (millis() - started) + 500);
    do_ThingSpeak_report();   
    tickerObject_do_blink.update();
    Serial.println("Go sleep?");
    WiFi.forceSleepWake();
    delay(1);
    //set_rtc_flag(millis());
    ESP.deepSleep(MEASURE_TIMEOUT * 1000, WAKE_RF_DEFAULT);
  };
}
