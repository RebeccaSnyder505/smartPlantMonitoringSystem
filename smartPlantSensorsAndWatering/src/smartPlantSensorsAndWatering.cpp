/* 
 * Project: Smart Plant Monitoring System, for CNM IoT Fall 2023
 * Author: Rebecca Snyder
 * Date: 2023-11-07
 * 
 * 
 *  NOTE 2023-11-07 possible wiring problem with dust sensor 5v
 *  NOTE OLED needs something more, displaying stuff but oddly
 * 
 *
 * 
 *  must install library "Adafruit_SSD1306" with ctrl-shift-p
 *  install library "IoTClassroom_CNM" with ctrl-shift-p
 *  install "Arduino"
 *  install "Grove_Air_quality_Sensor"
 *  install "Adafruit_MQTT"
 *  install "Adafruit_SSD1306"
 *  install "Adafruit_BME280"
 * 
 */

#include "Particle.h"
#include "Arduino.h"
#include "Grove_Air_quality_Sensor.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"
#include "Adafruit_BME280.h"
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
Adafruit_MQTT_Publish feedAirPressure = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/smartflowerpot.airpress");
Adafruit_MQTT_Publish feedTempCelcius = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/smartflowerpot.tempcelcius");
Adafruit_MQTT_Publish feedHumidity = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/smartflowerpot.humidity");
Adafruit_MQTT_Publish feedPlantMoisture = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/smartflowerpot.plantmoisture");
Adafruit_MQTT_Subscribe feedWaterButton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/smartflowerpot.waterbutton");

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

// Define BME280 objext
Adafruit_BME280 bme; // initialize I2C temp sensor

const int AQPIN = A5;
const int DUSTPIN = A2;
int currentAQ =-1;
float currentDust; 
float currentAQRaw;
int prevAQTime;
int prevDustTime;
const int timeIntervalAQ = 2000;
const int timeIntervalDust = 5000;
int prevAdafruitTime;
const int timeIntervalAdafruit = 40000; 
const int MOISTPIN = A1;
int moistureRead;
const int timeIntervalMoisture = 5000;
int prevMoistureTime;
bool status;
int prevBME289Time;
const int timeIntervalBME280 = 5000;
float tempC; 
float pressPA;
float humidRH;
const int pumpPin = D9;
const int moistureLow = 10; // ridiculously low, needs testing to find number
bool givePlantWater;

AirQualitySensor airquality(AQPIN); // declaring object airquality of class AirQualitySensor



SYSTEM_MODE(SEMI_AUTOMATIC);



// setup() runs once, when the device is first turned on
void setup() {
  Serial.begin(9600);
  waitFor(Serial.isConnected,10000);

  //initialize OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  //initialize BME280
  status = bme.begin(0x76);
  if (status == false) {
    Serial.printf("BME280 falied to start");
  }
  
  WiFi.on();
  WiFi.connect();
  while (WiFi.connecting()) {
    Serial.printf(".");
  }
  Serial.printf("\n\n"); 
  prevAdafruitTime = 0;
  pinMode(MOISTPIN, INPUT);
  pinMode(pumpPin, OUTPUT);
  givePlantWater = false;
  mqtt.subscribe(&feedWaterButton); 
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
    if (moistureRead <= moistureLow) {
      givePlantWater = true;
      Serial.printf("moistureLow, give plant water = true \n");
    }
  }
  if ((millis() - prevBME289Time) > timeIntervalBME280) { //read BME280 temp pres humid
    prevBME289Time = millis();
    tempC = bme.readTemperature(); // in degrees Celcius
    pressPA = bme.readPressure();  // in Pascals
    humidRH = bme.readHumidity();  // in %RH
    Serial.printf("Temp %f Press %f Humid %f \n", tempC , pressPA , humidRH );
  }

  //check button
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) { // the subscription part
    if (subscription == &feedWaterButton) {
      Serial.printf("line 184, give plant water = true \n");
      subValue = atof((char *)feedWaterButton.lastread);
      Serial.printf("button pushed on adafrui");

      givePlantWater = true;
    }
  }
//   ///////begin pasting
//       if( mqtt.Update() )
//     {
//         // this is our 'wait for incoming subscription packets' busy subloop
//         // try to spend your time here
//         Adafruit_MQTT_Subscribe *subscription;
//         while ((subscription = mqtt.readSubscription(5000)))
//         {
//             if (subscription == &feedWaterButton)
//             {
//                 Serial.print(F("Got: "));
//                 Serial.println((char *)feedWaterButton.lastread);
//             }
//         }
//     }
// ///end pasted code from adafruit-mqtt-spark-test.ino



  if ((millis()-prevAdafruitTime) > timeIntervalAdafruit) { //publish to Adafruit
    if(mqtt.Update()) {
      prevAdafruitTime = millis();
      Serial.printf("attempting adafruit publish");
      AQProcessedFeed.publish(currentAQ);
      AQRawFeed.publish(currentAQRaw);
      dustFeed.publish(currentDust);
      feedAirPressure.publish(pressPA);
      feedHumidity.publish(humidRH);
      feedTempCelcius.publish(tempC);
      feedPlantMoisture.publish(moistureRead);
    }
  }
  if (givePlantWater == true) {
    givePlantWater = false;
    digitalWrite(pumpPin, HIGH);
    Serial.printf("relay closed \n");
    delay (1000); // relay closed for very short time
    digitalWrite(pumpPin, LOW);
    Serial.printf("relay open \n");
    delay(3000); // removoe after testing
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







