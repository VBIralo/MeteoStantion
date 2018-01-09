//#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "Seeed_BME280.h"
#include <Wire.h>

BME280 bme280;
WiFiClient client;

char auth[] = "Blynk_api_key";
char ssid[] = "WiFi SSID(name)";
char pass[] = "WiFi PASSWORD";
char server[] = "api.thingspeak.com";
char api_key[] = "THINGSPEAK_WRITE_KEY";

void setup(){
  Wire.begin();
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED){
  Serial.println("WiFi connected");
  }
  if(!bme280.init()){
  Serial.println("Device error!");
  }
}

void loop(){
  Blynk.run();

  float temp = bme280.getTemperature();
  float pressure = (bme280.getPressure() / 100.0)*0.7500637; //hPa
  float humidity = bme280.getHumidity();
  double dewpoint = dewPoint(temp, humidity);
  
  //getting and sending temperature
  Serial.print("Температура: ");
  Serial.print(temp);
  Serial.println("C");
  Blynk.virtualWrite(0, temp); // virtual pin 0

  //getting and sending pressure
  Serial.print("Давление:    ");
  Serial.print(pressure);
  Serial.println("гПа");
  Blynk.virtualWrite(1, pressure); // virtual pin 1

  //getting and sending humidity
  Serial.print("Влажность:   ");
  Serial.print(humidity);
  Serial.println("%");
  Blynk.virtualWrite(2, humidity); // virtual pin 2

  //getting and sending dewpoint
  Serial.print("Точка росы:  ");
  Serial.print(dewpoint);
  Serial.println("C");
  Blynk.virtualWrite(3, dewpoint); // virtual pin 3

  postData();
  
  ESP.deepSleep(300e6); // 60e6 == 1min // 300e6 == 5 min
}

long postData() {
  float temp = bme280.getTemperature();
  float pressure = (bme280.getPressure() / 100.0)*0.7500637;
  float humidity = bme280.getHumidity();
  float dewpoint = dewPoint(temp, humidity);
  
  if (client.connect(server,80)) {
  Serial.println("Connect to ThingSpeak - OK"); 
  Serial.println("////////////////////////////////////////////");

  String dataToThingSpeak = "";
  dataToThingSpeak+="GET /update?api_key=";
  dataToThingSpeak+=api_key;
   
  dataToThingSpeak+="&field1=";
  dataToThingSpeak+=String(temp);

  dataToThingSpeak+="&field2=";
  dataToThingSpeak+=String(humidity);

  dataToThingSpeak+="&field3=";
  dataToThingSpeak+=String(pressure);
  
  dataToThingSpeak+="&field4=";
  dataToThingSpeak+=String(dewpoint);
   
  dataToThingSpeak+=" HTTP/1.1\r\nHost: a.c.d\r\nConnection: close\r\n\r\n";
  dataToThingSpeak+="";
  client.print(dataToThingSpeak);

  int timeout = millis() + 5000;
  while (client.available() == 0) {
    if (timeout - millis() < 0) {
      Serial.println("Error: Client Timeout!");
      client.stop();
      return false;
    }
  }
}
}

long dewPoint(float temp, float humidity) {
  double b = 17.62;
  double c = 243.50;
  double gamma = log(humidity/100) + (b*temp / (c+temp));
  double dewpoint = c*gamma / (b-gamma);

  return dewpoint;
}
