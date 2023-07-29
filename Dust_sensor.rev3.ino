/*
 * Date : 29/07/23
 * Dev by : MP
 * Source : https://circuitdigest.com/microcontroller-projects/interfacing-dust-sensor-with-arduino
 * Detail :
 * - เพิ่ม Connect EGATWIFI
 * - Connect to P Aun's server
 * Test result : Not Test yet!
*/


#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <TridentTD_LineNotify.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>

// include library to conncet EGAT
#include "esp_wpa2.h"
#include <WiFiClient.h>

/* For Server */
#include <HTTPClient.h>
#include <Arduino_JSON.h>

/* Replace with your network credentials for EGATwifi */
#define EAP_IDENTITY "596352"
#define EAP_PASSWORD "T@GET@GET@GET@GE159951"
String line;
const char* SSID = "EGATWIFI";

//For web server, BUT it caannot use with EGAT wifi
WebServer server(80);

/* token Test Group */
#define LINE_TOKEN "hMumnvoo4ajPj36hFbxjDCTWvG2G1m1tNfz3mmopwoo"          // J.A.R.V.I.S. - TEST Group

LiquidCrystal_I2C lcd(0x27, 16, 2);

/* Define Pin to connect with Dust sensor */
//int measurePin = A0;
/* เปลี่ยนมาใช้ขา D15 ในการรับค่า Analog จาก Dust sensor แทน*/
int measurePin = 15;  // ขา Vo เส้นสีดำ
/* ใช้ขา D5 เป็นตัวสั่งให้ LEของ Dust sensor ยิงแสงเพื่อวัดฝุ่น */
int ledPower = 5;    //Pin LED เส้นสีขาว


/* Define Variable */
int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

float send_dustDensity;
String send_Status_Read_dustDensity;
String room = "ControlRoom";

/* Variables for HTTP POST request data. */
String postData = ""; //--> Variables sent for HTTP POST request data.
String payload = "";  //--> Variable for receiving response from HTTP POST.

/* ================================= */

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println(LINE.getVersion());

  // Connect to EGAT Wi-Fi network
  Serial.print("Connecting to EGATWIFI");
  Serial.println(SSID);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide identity
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT(); //set config to default (fixed for 2018 and Arduino 1.8.5+)
  esp_wifi_sta_wpa2_ent_enable(&config); //set config to enable function (fixed for 2018 and Arduino 1.8.5+)

  delay(100);

  WiFi.begin(SSID);  // connect to Wifi
  //WiFi.setHostname("ESP32name") // set Hostname for your device - not neccesary
  //WiFi.hostname("ESP32_SAC") // set hostname for ESP8266
  Serial.printf("Wifi connecting to %s\n", SSID);
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(1000);
  }  
  Serial.print("\nEGATWiFi connected\n : ");
  Serial.println(WiFi.localIP());

  /* ====== Server ====== */
  server.on("/",handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");


  LINE.setToken(LINE_TOKEN);
  Serial.println("LINE_TOKEN setted");
  LINE.notify("DustDensity Sensor is READY");
  Serial.println("ESP32 connected to EGATwifi");
    
  pinMode(ledPower, OUTPUT);
  lcd.begin();
  lcd.backlight();       // เปิด backlight
}

/* === Subroutine to read and get data from the DHT22 sensor. === */
/* === Function get_DHT22_sensor_data() === */
void get_sensor_data(float send_dustDensity_to_Server) {
  // เป็น Void Function ที่ไม่คืนค่า และให้ Function นี้รับค่าและเก็บในตัวแปร "send_dustDensity_to_Server" เลย
  Serial.println("---get_sensor_data()---");
  //send_dustDensity = send_dustDensity_to_Server;
  if (isnan(send_dustDensity_to_Server)) {
    Serial.println("Failed to read from sensor!");
    send_dustDensity = 0.00;
    send_Status_Read_dustDensity = "FAILED";
  }else {
    send_Status_Read_dustDensity = "SUCCEED";
  }
  Serial.printf("Dust Density : %.2f ug/m3\n", send_dustDensity_to_Server);
  Serial.printf("Status Read Sensor : %s\n", send_Status_Read_dustDensity);
  Serial.println("-------------\n\n");
}

void loop() {
  // Turn "ON the LED
  digitalWrite(ledPower, LOW); // Turn "ON the LED
  delayMicroseconds(samplingTime); // ให้ Turn ON the LED เป็นเวลา 280 microsecond. (Recommend by the datasheet)

  voMeasured = analogRead(measurePin); // read the dust value
  delayMicroseconds(deltaTime); // ให้หน่วงเวลาเพิ่มอีก 40 microsec อีก เพื่อ LED ON ค้างเป็นเวลา = 280+40 = 320 microsec

  // หลังจากนั้นก็ให้ Turn OFF LED โดยการให้ status ของ LED อยู่สถานะ HIGH.
  digitalWrite(ledPower, HIGH); // turn the LED off

  // และให้ Turn OFF เป็นเลานาน 9680 microsec.
  delayMicroseconds(sleepTime);

  /* Microcontroller บางแบบมี range สำหรับรับค่า Analog แค่ 1024 */
  // 0 - 3.3V mapped to 0 - 1023 integer values
  // recover voltage
  // calcVoltage = voMeasured * (3.3 / 1024);

  // Calculate the voltage value from the ADC value.
  // Please make sure that the arduino is powered with the proper +5V and not powered with USB
  // if so, you will get errors in your result.
  // calcVoltage = voMeasured*(5.0/1024.0); // microcontroller บางแบบมี range = 1024
  
  /*  
   *  ESP32 : These analog input pins have 12-bit resolution.
   *  This means that when you read an analog input,
   *  its range may vary from 0 to 4095.  */
  calcVoltage = voMeasured*(5.0/4096.0);

  /* linear eqaution taken from https://www.ab.in.th/b/59
   * Chris Nafis (c) 2012
   * dustDensity = 0.17 * calcVoltage - 0.1;
   * Finally, we calculate the amount of suspended dust particles
   * by using the linear equation provided by Chris Nafis; the unit is in ug/m3
  */
  dustDensity = 0.17*calcVoltage - 0.1;
  
  Serial.print("Raw Signal Value (0-4095): ");
  Serial.print(voMeasured);

  Serial.print(" - Voltage: ");
  Serial.print(calcVoltage);

  if (dustDensity <= 0.00) {
    dustDensity = 0.00;
  }

  dustDensity = dustDensity * 1000; // แปลง dustDensity จากหน่วย mg/m3 เป็น ug/ m3

  Serial.print(" - Dust Density: ");
  Serial.print(dustDensity);
  Serial.println(" µg/m³");
  lcd.home();
  lcd.setCursor(1, 0);
  lcd.print("Dust Density ");
  lcd.setCursor(2, 1);
  lcd.print(dustDensity);
  lcd.print(" ug/m3  ");

  /* === Connecting to SAC Server === */
  /* Check WiFi connection status. */
  if(WiFi.status()== WL_CONNECTED) {
    HTTPClient http;  //--> Declare object of class HTTPClient.
    int httpCode;     //--> Variables for HTTP return code.
    
    /* === Calls the get_sensor_data() subroutine. === */
    get_sensor_data(float dustDensity);

    Serial.println("---getdata.php in VOID LOOP ---");
    
    http.begin("http://10.232.1.66/esp32/pm25/getdata.php");             //--> Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded"); //--> Specify content-type header

    httpCode = http.POST(postData); //--> Send the request
    payload = http.getString();     //--> Get the response payload

    Serial.print("httpCode : ");
    Serial.println(httpCode);       //--> Print HTTP return code
    Serial.print("payload  : ");
    Serial.println(payload);        //--> Print request response payload
    
    http.end();                     //--> Close connection
    Serial.println("---------------");

    /* Process to get LEDs data from database to control LEDs. */
    postData = "id=dust";
    postData += "&dust=" + String(dustDensity);
    // === Recheck กับพี่อั้นว่าต้องมี Room มั้ย ===
    //postData += "&room=" + room;
    payload = "";
    
    Serial.println();
    Serial.println("--- updatePM2.5.php --- ");
   
    http.begin("http://10.232.1.66/esp32/pm25/update_pm25.php");          //--> Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //--> Specify content-type header
   
    httpCode = http.POST(postData);   //--> Send the request
    payload = http.getString();       //--> Get the response payload
  
    Serial.print("httpCode : ");
    Serial.println(httpCode);         //--> Print HTTP return code
    Serial.print("payload  : ");
    Serial.println(payload);          //--> Print request response payload
    
    http.end();  //Close connection
    Serial.println("---------------");      
    //delay(1000);   // จำเป็นมั้ย
  }
  delay(2000);
}
