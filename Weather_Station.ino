//#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "Seeed_BME280.h"
#include <Wire.h>
#include "config.h"

// Init BME & WIFI
BME280 bme280;
float temp(NAN), humi(NAN), pres(NAN);
float dp(NAN), hi(NAN);
WiFiClient client;

ADC_MODE(ADC_VCC);
long vcc = 0;
String WIFI_Hostname;

void setup()
{
  Serial.begin(9600);
  Serial.println("Waking up");
  delay(1000);
  // Init BME
  Wire.begin();
  if (!bme280.init())
  {
    Serial.println("Device error!");
  }
  // Init Blynk
  Blynk.begin(BLYNK_KEY, WIFI_SSID, WIFI_PASS);
  // Init WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WIFI_Hostname = "ESP" + WiFi.macAddress();
  WIFI_Hostname.replace(":", "");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Connecting WiFi:");
    Serial.println(WIFI_Hostname);
    Serial.println(WIFI_SSID);
  }
  /// Input Voltage
  vcc = ESP.getVcc();

  if (vcc <= BATT_CRT)
  {
    Serial.print("The battery charge is critical, go to sleep!");
    goDeepSleep();
  }
}

void loop()
{
  Blynk.run();

  // Getting temperature & pressure & humidity
  float temp = bme280.getTemperature();
  float pres = (bme280.getPressure() / 100.0) * 0.7500637; //mmHg
  float humi = bme280.getHumidity();

  // Calculate HeatIndex and DewPoint
  float dp = computeDewPoint(temp, humi);
  float hi = computeHeatIndex(temp, humi);

  // Getting and sending temperature
  Serial.print("Temperature:");
  Serial.print(temp);
  Serial.println("C");
  Blynk.virtualWrite(0, temp); // virtual pin 0

  // Getting and sending pressure
  Serial.print("Pressure:   ");
  Serial.print(pres);
  Serial.println("mmHg");
  Blynk.virtualWrite(1, pres); // virtual pin 1

  // Getting and sending humidity
  Serial.print("Humidity:   ");
  Serial.print(humi);
  Serial.println("%");
  Blynk.virtualWrite(2, humi); // virtual pin 2

  // Getting and sending dewpoint
  Serial.print("DewPoint:   ");
  Serial.print(dp);
  Serial.println("C");
  Blynk.virtualWrite(3, dp); // virtual pin 3

  // Getting and sending heatindex
  Serial.print("HeatIndex:  ");
  Serial.print(hi);
  Serial.println("C");
  Blynk.virtualWrite(4, hi); // virtual pin 4

  // Getting and sending VCC
  Serial.print("Vcc:        ");
  Serial.print(vcc);
  Serial.println("mV");
  Blynk.virtualWrite(5, vcc); // virtual pin 5

  postData(temp, pres, humi, dp, hi, vcc);
  SendToNarodmon(temp, pres, humi);

  Serial.println("Work is done. Going back to sleep.");
  goDeepSleep();
}

long postData(float temp, float pres, float humi, float dewpoint, float heatindex, float vcc)
{
  if (client.connect(TS_HOST, 80))
  {
    Serial.println("Connect to ThingSpeak - OK");
    Serial.println("////////////////////////////////////////////");

    String dataToThingSpeak = "";
    dataToThingSpeak += "GET /update?api_key=";
    dataToThingSpeak += TS_KEY;

    dataToThingSpeak += "&field1=";
    dataToThingSpeak += String(temp);

    dataToThingSpeak += "&field2=";
    dataToThingSpeak += String(humi);

    dataToThingSpeak += "&field3=";
    dataToThingSpeak += String(pres);

    dataToThingSpeak += "&field4=";
    dataToThingSpeak += String(dewpoint);

    dataToThingSpeak += "&field5=";
    dataToThingSpeak += String(heatindex);

    dataToThingSpeak += "&field6=";
    dataToThingSpeak += String(vcc);

    dataToThingSpeak += " HTTP/1.1\r\nHost: a.c.d\r\nConnection: close\r\n\r\n";
    dataToThingSpeak += "";
    client.print(dataToThingSpeak);

    int timeout = millis() + 5000;
    while (client.available() == 0)
    {
      if (timeout - millis() < 0)
      {
        Serial.println("Error: Client Timeout!");
        client.stop();
        return false;
      }
    }
  }
}

long SendToNarodmon(float temp, float pres, float humi)
{
  String buf;
  buf = "#" + WIFI_Hostname + "#Meteo_VBiralo" + "\r\n"; // HEADER AND NAME
  buf = buf + "#T1#" + String(temp) + "#Улица\r\n";
  buf = buf + "#H1#" + String(humi) + "#Влажность\r\n";
  buf = buf + "#P1#" + String(pres) + "#Давление\r\n";
  buf = buf + "#LAT#54.872339\r\n";
  buf = buf + "#LNG#26.929278\r\n";
  buf = buf + "#ELE#243\r\n";
  buf = buf + "##"; // close packet

  if (!client.connect("narodmon.ru", 8283))
  {
    Serial.println("Connect to NarodMon - FAILED");
    return false;
  }
  else
  {
    Serial.println("Connect to NarodMon - OK");
    Serial.println(WIFI_Hostname);
    Serial.println("////////////////////////////////////////////");
    client.print(buf); //send data
    while (client.available())
    {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
  }
}

void goDeepSleep()
{
  Serial.print("Deep Sleep (us):");
  long sleepTime = (INTERVAL_NRM - millis()) * 1000;
  if (vcc <= BATT_LOW)
  {
    sleepTime = (INTERVAL_LOW - millis()) * 1000;
  }
  if (sleepTime < 1)
  {
    sleepTime = 1;
  }

  Serial.println(sleepTime);
  ESP.deepSleep(sleepTime);
}

long computeDewPoint(float temp, float humi)
{
  // Equations courtesy of Brian McNoldy
  // http://andrew.rsmas.miami.edu;

  float dewpoint = NAN;
  double b = 17.625;
  double c = 243.04;

  dewpoint = c * (log(humi / 100.0) + ((b * temp) / (c + temp))) / (b - log(humi / 100.0) - ((b * temp) / (c + temp)));

  return dewpoint;
}

float computeHeatIndex(float temp, float humi)
{
  // Using both Rothfusz and Steadman's equations
  // http://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml

  float heatindex;
  temp = convertCtoF(temp);

  heatindex = 0.5 * (temp + 61.0 + ((temp - 68.0) * 1.2) + (humi * 0.094));

  if (heatindex > 79)
  {
    heatindex = -42.379 +
                2.04901523 * temp +
                10.14333127 * humi +
                -0.22475541 * temp * humi +
                -0.00683783 * pow(temp, 2) +
                -0.05481717 * pow(humi, 2) +
                0.00122874 * pow(temp, 2) * humi +
                0.00085282 * temp * pow(humi, 2) +
                -0.00000199 * pow(temp, 2) * pow(humi, 2);

    if ((humi < 13) && (temp >= 80.0) && (temp <= 112.0))
    {
      heatindex -= ((13.0 - humi) * 0.25) * sqrt((17.0 - abs(temp - 95.0)) * 0.05882);
    }
    else if ((humi > 85.0) && (temp >= 80.0) && (temp <= 87.0))
    {
      heatindex += ((humi - 85.0) * 0.1) * ((87.0 - temp) * 0.2);
    }
  }

  return convertFtoC(heatindex);
}

float convertCtoF(float c)
{
  return c * 1.8 + 32;
}

float convertFtoC(float f)
{
  return (f - 32) * 0.55555;
}
