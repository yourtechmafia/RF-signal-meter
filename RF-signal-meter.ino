/* Blynk cloud connection with SIM800 GSM module
    Fill in the credentials obtained from your Blynk account
*/
#define BLYNK_TEMPLATE_ID " " //  Blynk template ID from your blynk.io account
#define BLYNK_DEVICE_NAME "Radio Frequency Power Meter"
#define BLYNK_AUTH_TOKEN " " // Blynk auth token from your blynk.io account
#define TINY_GSM_MODEM_SIM800 //  TinyGSM SIM800L module library
//  Include all necessary libraries
#include <TinyGsmClient.h> //  TinyGSM module client
#include <BlynkSimpleTinyGSM.h> //  TinyGSM Blynk library
#include <SoftwareSerial.h> //  Software serial library for SIM800L communication on Arduino Uno
#include <LiquidCrystal.h>  //  LCD module library & config
#include "DHTStable.h"  //  DHT11 library
//  Libraries constants for initialization
const char apn[] = " ", user[] = "", pass[] = ""; //  Set your ISP apn value here, username and password are both optional
SoftwareSerial SerialAT(3, 4); // RX, TX | Software serial on Uno pins 3 & 4
TinyGsm modem(SerialAT);  //  Initialize modem instance
LiquidCrystal lcd(7, 6, 9, 10, 11, 12);  //  LCD module pins on board - RS,EN,D4,D5,D6,D7
DHTStable DHT;  //  Initialize DHT instance
//  IDE constants
#define DHT11_PIN A4  //  DHT11 data pin
#define LM35Pin A3  //  LM35 temperature sensor pin
#define VMAG A0 //  AD8302 VMAG pin
#define battPin A1  //  Battery pin to measure system voltage
#define blue_led 5  //  A pin for a blue led
#define green_led 8 //  A pin for a green led
//  Global variables
int temp, humidity; //  Temperature and humidity variables
int batteryVolt;  //  System battery voltage variable
int rfSum, rfValue; //  Radio frequency meter variables
unsigned long sync_data, sync_data_2;  //  Time counters to trigger actions in loop()

void setup() {
  pinMode(blue_led, OUTPUT); pinMode(green_led, OUTPUT);
  lcd.begin(16, 2); lcd.clear();  //  Initialize LCD and clear screen
  lcd.setCursor(0, 0);  lcd.print("IoT RF-EMR METER");
  lcd.setCursor(0, 1);  lcd.print((char)223);
  lcd.print("C, %H, dBm, Web");
  delay(5000);
  lcd.clear();
  lcd.home(); lcd.print("  CONNECTING...");
  lcd.setCursor(0, 1);  lcd.print(" WAIT A MINUTE");
  digitalWrite(blue_led, 1);  //  Turn on blue led to indicate data connection in progress
  Serial.begin(9600); //  Initialize baud rate for serial monitor
  SerialAT.begin(9600); //  Initialize software serial baud rate for SIM module
  modem.restart();  //  Initialize modem
  Blynk.begin(BLYNK_AUTH_TOKEN, modem, apn, user, pass);  //  Initiate connection to blynk cloud
  lcd.clear();
  lcd.print("   CONNECTION");
  lcd.setCursor(3, 1);  lcd.print("SUCCESSFUL");
  digitalWrite(blue_led, 0);  //  Turn off blue led, and
  digitalWrite(green_led, 1); //  Turn on green led to indicate connection successful
  lcd.clear();
  int checkDHT = DHT.read11(DHT11_PIN); //  Obtain data from DHT11 sensor
  temp = DHT.getTemperature(); // Hold the temperature value retrieved from sensor
  humidity = DHT.getHumidity(); //  Hold the humidity value retrieved from sensor
  double batteryRead = analogRead(battPin); //  Battery +ve terminal is connected to analog pin "battPin"
  /*  Maps battery voltage (batteryRead) to a level between 0 - 100%
     3.7V is equivalent to 613.8 bits and denotes 0%
     4.2V is equivalent to 865 bits and denotes 100%
  */
  batteryVolt = map(batteryRead, 613.8, 865, 0, 100);
}

void loop() {
  Blynk.run();  //  Maintain connection with Blynk server
  lcd.setCursor(2, 0);  lcd.print(temp);
  lcd.print((char)223);   lcd.print("C | ");
  lcd.print(humidity);  lcd.print("%H   ");
  lcd.setCursor(0, 1);
  lcd.print(rfValue);   lcd.print("dBm | (");
  lcd.print(batteryVolt); lcd.print("%B) ");
  if (millis() - sync_data >= 1000) { //  Runs every 1 second
    sync_data = millis();
    rfValue = -20000 / analogRead(VMAG);  //  Random value for Radio frequency analog signal obtained from AD8302 module
    rfSum += rfValue / 60;  //  rfSum holds the average value of rfValue for a minute
    Blynk.virtualWrite(V0, rfValue);  //  Sync to Blynk cloud
  }
  /*  Temperature and humidity doesn't change over a long period of time.
      Hence, the reason for fetching them every 1 minute
  */
  if (millis() - sync_data_2 >= 60000) {  //  Runs every 1 minute (60 seconds)
    sync_data_2 = millis();
    int checkDHT = DHT.read11(DHT11_PIN); //  Evaluate new data from DHT11
    temp = DHT.getTemperature();  //  Retrieve temperature
    humidity = DHT.getHumidity(); //  Retrieve humidity
    double batteryRead = analogRead(battPin);
    batteryVolt = map(batteryRead, 613.8, 865, 0, 100);
    //  Sync new data to Blynk cloud
    Blynk.virtualWrite(V2, temp);
    Blynk.virtualWrite(V3, humidity);
    Blynk.virtualWrite(V4, batteryVolt);
    //  Print new data to serial monitor
    Serial.print("A-RF:");
    Serial.print(rfSum); Serial.print(", ");
    Serial.print("Temperature:");
    Serial.print(temp); Serial.print(", ");
    Serial.print("Humidity:");
    Serial.print(humidity); Serial.print(", ");
    Serial.println();
  }
}
