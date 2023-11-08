/* 
 * Project: Smart Plant Monitoring System, for CNM IoT Fall 2023
 * Author: Rebecca Snyder
 * Date: 2023-11-07
 * 
 * 
 *  NOTE 2023-11-07 possible wiring problem with dust sensor 5v
 *  NOTE OLED needs something more, displaying stuff but oddly
 * 
 *  must install library "Adafruit_SSD1306" with ctrl-shift-p
 *  install library "IoTClassroom_CNM" with ctrl-shift-p
 *  install "Arduino"
 *  install "Grove_Air_quality_Sensor"
 *  install "Adafruit_MQTT"
 * 
 */

#include "Particle.h"
#include "Arduino.h"
#include "Grove_Air_quality_Sensor.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"
#include <math.h>



// begin adafruit publishing stuff
#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "credentials.h"
/************ Global State (you don't need to change this!) ***   ***************/ 
TCPClient TheClient; 

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details. 
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 

/****************************** Feeds ***************************************/ 
// Setup Feeds to publish or subscribe 
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname> 

Adafruit_MQTT_Publish AQProcessedFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/seeedsensors.air-quality-from-class-airqualitysensor");
Adafruit_MQTT_Publish AQRawFeed= Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/seeedsensors.raw-air-quality-data");
Adafruit_MQTT_Publish dustFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/seeedsensors.dust-sensor");

/************Declare Variables*************/
unsigned int last, lastTime;
float subValue,pubValue;

/************Declare Functions*************/
void MQTT_connect();
bool MQTT_ping();
// end adafruit publishing stuff

//declare object "display" to use with OLED
#define OLED_RESET D4
Adafruit_SSD1306 display(OLED_RESET); 
void testdrawcircle(void);

const int AQPIN = A5;
const int DUSTPIN = A2;
int currentAQ =-1;
float currentDust; 
float currentAQRaw;
int prevAQTime;
int prevDustTime;
const int timeIntervalAQ = 2000;
const int timeIntervalDust = 5000;
int prevAdafruiutTime;
const int timeIntervalAdafruit = 25000; 
const int MOISTPIN = A1;
int moistureRead;
const int timeIntervalMoisture = 5000;
int prevMoistureTime;

AirQualitySensor airquality(AQPIN); // declaring object airquality of class AirQualitySensor



SYSTEM_MODE(SEMI_AUTOMATIC);



// setup() runs once, when the device is first turned on
void setup() {
  Serial.begin(9600);
  waitFor(Serial.isConnected,10000);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  WiFi.on();
  WiFi.connect();
  while (WiFi.connecting()) {
    Serial.printf(".");
  }
  Serial.printf("\n\n"); 
  prevAdafruiutTime = 0;
  pinMode(MOISTPIN, INPUT);
}


void loop() {
  MQTT_connect();
  MQTT_ping();
  if ((millis() - prevAQTime) > timeIntervalAQ) { //read Air Quality
    prevAQTime = millis();
    currentAQ = airquality.slope();
    if (currentAQ >= 0) { //if a valid datum is returned
      if (currentAQ == 0){
        Serial.printf("Whew that's bad!!");
      }
      if (currentAQ == 1) {
        Serial.printf("getting worse");
      }
      if (currentAQ == 2) {
        Serial.printf("less than fresh");
      }
      if (currentAQ == 3) {
        Serial.printf("so fresh");
      }
      currentAQRaw = airquality.getValue();
      Serial.printf("AQ sensor: %f \n", currentAQRaw);
    }
  }
  if ((millis() - prevDustTime) > timeIntervalDust) { //read Dust Sensor
    prevDustTime = millis();
    //read the dust sensor
    currentDust = analogRead(DUSTPIN);
    Serial.printf("Dustness level: %f \n", currentDust);
  }
  if ((millis() - prevMoistureTime) > timeIntervalMoisture) { //read Moisture Sensor
    prevMoistureTime = millis();
    moistureRead = analogRead(MOISTPIN);
    Serial.printf("moisture reading %d \n", moistureRead);  
  }
  if ((millis()-prevAdafruiutTime) > timeIntervalAdafruit) { //publish to Adafruit
    if(mqtt.Update()) {
      prevAdafruiutTime = millis();
      Serial.printf("attempting adafruit publish");
      AQProcessedFeed.publish(currentAQ);
      AQRawFeed.publish(currentAQRaw);
      dustFeed.publish(currentDust);
    }
  }
}


void MQTT_connect() {
  int8_t ret;
 
  // Return if already connected.
  if (mqtt.connected()) {
    return;
  }
 
  Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.printf("Error Code %s\n",mqtt.connectErrorString(ret));
       Serial.printf("Retrying MQTT connection in 15 seconds...\n");
       mqtt.disconnect();
       delay(15000);  // wait 15 seconds and try again
  }
  Serial.printf("MQTT Connected!\n");
}

bool MQTT_ping() {
  static unsigned int last;
  bool pingStatus;

  if ((millis()-last)>120000) {
      Serial.printf("Pinging MQTT \n");
      pingStatus = mqtt.ping();
      if(!pingStatus) {
        Serial.printf("Disconnecting \n");
        mqtt.disconnect();
      }
      last = millis();
  }
  return pingStatus;
}

void testdrawcircle(void) {
  for (int16_t i=0; i<display.height(); i+=2) {
    display.drawCircle(display.width()/2, display.height()/2, i, WHITE);
    display.display();
  }
}