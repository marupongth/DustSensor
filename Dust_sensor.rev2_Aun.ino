#include <HTTPClient.h>
#include <Arduino_JSON.h>

#include <WiFi.h>           // ESP32
//#include <ESP8266WiFi.h>  // ESP8266
#include "WiFiClient.h"
#include <TridentTD_LineNotify.h>
#include <Wire.h>

/* include libray to connect EGAT wifi */
#include "esp_wpa2.h"
#include <WiFiClient.h>

//======================================== Variables for HTTP POST request data.
String postData = ""; //--> Variables sent for HTTP POST request data.
String payload = "";  //--> Variable for receiving response from HTTP POST.
String send_Status_Read = "";

/* EGATwifi */
#define EAP_IDENTITY "596352"
#define EAP_PASSWORD "T@GET@GET@GET@GE159951"
const char* SSID = "EGATWIFI";
/*
 * Date : 21/07/23
 * Dev by : MP
 * Source : https://circuitdigest.com/microcontroller-projects/interfacing-dust-sensor-with-arduino
*/

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

//int measurePin = A0;
/* เปลี่ยนมาใช้ขา D15 ในการรับค่า Analog จาก Dust sensor แทน*/
int measurePin = 15;  // ขา Vo เส้นสีดำ

/* ใช้ขา D5 เป็นตัวสั่งให้ LEของ Dust sensor ยิงแสงเพื่อวัดฝุ่น */
int ledPower = 5;    //Pin LED เส้นสีขาว

int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

void setup() {
  Serial.begin(115200);
  pinMode(ledPower, OUTPUT);
  lcd.begin();
  lcd.backlight();       // เปิด backlight

    // Connect to EGATWIFI
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
    
  WiFi.begin(SSID);   // connect to WGATwifi
  //WiFi.hostname("ESP32_SAC FLOOD");
  Serial.printf("Wifi connecting to %s\n", SSID);
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(1000);
  }

  Serial.printf("\nEGATWifi connected\nIP : ");
  Serial.println(WiFi.localIP());
} // end setup

void loop() {
  
  //long dustDensity;
  
  // Turn "ON the LED
  digitalWrite(ledPower, LOW); // Turn "ON the LED
  delayMicroseconds(samplingTime); // ให้ Turn ON the LED เป็นเวลา 280 microsecond. (Recommend by the datasheet)

  voMeasured = analogRead(measurePin); // read the dust value
  delayMicroseconds(deltaTime); // ให้หน่วงเวลาเพิ่มอีก 40 microsec อีก เพื่อ LED ON ค้างเป็นเวลา = 280+40 = 320 microsec

  // หลังจากนั้นก็ให้ Turn OFF LED โดยการให้ status ของ LED อยู่สถานะ HIGH.
  digitalWrite(ledPower, HIGH); // turn the LED off

  // และให้ Turn OFF เป็นเลานาน 9680 microsec.
  delayMicroseconds(sleepTime);

  //// 0 - 3.3V mapped to 0 - 1023 integer values
  //// recover voltage
  ////calcVoltage = voMeasured * (3.3 / 1024);

  // Calculate the voltage value from the ADC value.
  // Please make sure that the arduino is powered with the proper +5V and not powered with USB
  // if so, you will get errors in your result.
  //calcVoltage = voMeasured*(5.0/1024.0); // microcontroller บางแบบมี range = 1024
  
  /* ESP32 : These analog input pins have 12-bit resolution.
  This means that when you read an analog input,
  its range may vary from 0 to 4095.*/
  calcVoltage = voMeasured*(5.0/4096.0);

  //// linear eqaution taken from https://www.ab.in.th/b/59
  //// Chris Nafis (c) 2012
  //dustDensity = 0.17 * calcVoltage - 0.1;

  // Finally, we calculate the amount of suspended dust particles
  // by using the linear equation provided by Chris Nafis; the unit is in ug/m3
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
  delay(1000);
  
    /// ทำใหม่ 2023 ///////
              // Check if any reads failed.
        if (isnan(dustDensity)) {
          Serial.println("Failed to read from sonic sensor!");
          dustDensity = 0.00;
          send_Status_Read = "FAILED";
        } else {
          send_Status_Read = "SUCCEED";
        }
  //      Serial.print(dustDensity);
  //      Serial.println("µg/m³");
/////////////////////////////////////////////

//////////////// ทำใหม่ 2023 /////////////
  //---------------------------------------- Check WiFi connection status.
  if(WiFi.status()== WL_CONNECTED) {
    HTTPClient http;  //--> Declare object of class HTTPClient.
    int httpCode;     //--> Variables for HTTP return code.


 
    //........................................ Process to get LEDs data from database to control LEDs.
    postData = "id=dust";
     
    Serial.println();
    Serial.println("---------------getdata.php");
    
    http.begin("http://10.232.1.66/esp32/pm25/getdata.php");  //--> Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");        //--> Specify content-type header
   
    httpCode = http.POST(postData); //--> Send the request
    payload = http.getString();     //--> Get the response payload
  
    Serial.print("httpCode : ");
    Serial.println(httpCode); //--> Print HTTP return code
    Serial.print("payload  : ");
    Serial.println(payload);  //--> Print request response payload
    
    http.end();  //--> Close connection
    Serial.println("---------------");
     //........................................ 
    delay(1000);

    
    postData = "id=dust";
    postData += "&dust=" + String(dustDensity);
    postData += "&status_read_sensor=" + send_Status_Read;
  
    payload = "";
  
    Serial.println();
    Serial.println("---------------update_pm25.php");
   
    http.begin("http://10.232.1.66/esp32/pm25/update_pm25.php");  //--> Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //--> Specify content-type header
   
    httpCode = http.POST(postData); //--> Send the request
    payload = http.getString();  //--> Get the response payload
  
    Serial.print("httpCode : ");
    Serial.println(httpCode); //--> Print HTTP return code
    Serial.print("payload  : ");
    Serial.println(payload);  //--> Print request response payload
    
    http.end();  //Close connection
    Serial.println("---------------");      
    delay(4000); 
  } //WL_CONNECTED
  //---------------------------------------- 
} // end loop
