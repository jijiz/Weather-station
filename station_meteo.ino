/*************************************************** 
  This is an example for the SHTC3 Humidity & Temp Sensor
  Designed specifically to work with the SHTC3 sensor from Adafruit
  ----> https://www.adafruit.com/products/4636
  These sensors use I2C to communicate, 2 pins are required to  
  interface
 ****************************************************/

#include "SHTSensor.h"

SHTSensor sht;
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
 
const String strWelcomeMessage = "La station météo vous souhaite la bienvenue";
String strHostname = "Station météo";
String strMDNSName = "Station météo";
const char* SSID = "Obi-free";
const char* password = "2BD";

// Relay pinout
const int pinGPIORelay = 0; // Relay pin
int iRelayStatus = 0; //0=off,1=on,2=dimmed

// Rain Gauge pulses
int iReedStatus;
int iReedPrevStatus;
int iImpulse = 0;
int pinGPIOReedSw = 17;

// Webserver
ESP8266WebServer server(80);

void relayOn(){
  server.send(200, "text/json", "{\"name\": \"On\"}");
  digitalWrite(pinGPIORelay, LOW);
  iRelayStatus = 1;
}

void relayOff(){
  server.send(200, "text/json", "{\"name\": \"Off\"}");
  digitalWrite(pinGPIORelay, HIGH);
  iRelayStatus = 0;
}

void relayStatus(){
  const String strStatus = String(iRelayStatus);
  server.send(200, "text/html", strStatus);
}

void rain(){
  // 203 impulsions = 1000 mL
  // 1 impulsion = 1000/203 mL
  int iImpulsion_par_mL = 1000/203;
  int iPluie_mL = iImpulse*iImpulsion_par_mL;
  // Surface : 63.25 cm^2 / 1 m^2 =  10000 cm^2
  int iRatio = 10000/63.25;
  int iPluie_mL_metre_carre = iRatio*iPluie_mL;
  server.send(200, "text/html", String(iPluie_mL_metre_carre));
  iImpulse = 0;
}

void humidity(){
  if (sht.readSample()) {
      server.send(200, "text/html", String(sht.getHumidity()));
  } else {
      Serial.print("Error in readSample()\n");
  }
}

void temperature(){
  if (sht.readSample()) {
      server.send(200, "text/html", String(sht.getTemperature()));
  } else {
      Serial.print("Error in readSample()\n");
  }  
}


// Manage not found URL
void handleNotFound(){
  String message = "Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  
  for (uint8_t i = 0; i < server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}
 
// Define routing
void restServerRouting(){
  server.on("/", HTTP_GET, []() {
    server.send(200, F("text/html"), strWelcomeMessage);
  });

  // Relay
  server.on(F("/on"), HTTP_GET, relayOn);
  server.on(F("/off"), HTTP_GET, relayOff);
  server.on(F("/status"), HTTP_GET, relayStatus);

  // Weather : raw data
  server.on(F("/rain"), HTTP_GET, rain);
  server.on(F("/humidity"), HTTP_GET, humidity);
  server.on(F("/temperature"), HTTP_GET, temperature);
}
  
void setup(void){
  Wire.begin();
  Serial.begin(115200);
  delay(1000);
  if (sht.init()) {
      Serial.print("init(): success\n");
  } else {
      Serial.print("init(): failed\n");
  }
  
  sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM);
  
  // Relay
  pinMode(pinGPIORelay, OUTPUT);
  digitalWrite(pinGPIORelay, HIGH);
  iRelayStatus = 0;

  // Reed switch
  pinMode(pinGPIOReedSw, INPUT_PULLUP);
  iReedStatus = digitalRead(pinGPIOReedSw);
  iReedPrevStatus = iReedStatus;
  
  // Wifi
  WiFi.mode(WIFI_STA);
  WiFi.hostname(strHostname);
  WiFi.begin(SSID, password);
  Serial.println("");
  
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
 
  // DNS hostmane strMDNSName.local
  if (MDNS.begin(strMDNSName)){
    Serial.println("MDNS responder started");
  }

  // Web server
  restServerRouting();
  server.onNotFound(handleNotFound);
  server.begin();
}
 
void loop(void){
  server.handleClient();
  
  // Rain gauge impulse
  int iReedStatus = digitalRead(pinGPIOReedSw);
  if (iReedStatus == HIGH && iReedPrevStatus == LOW){
    iImpulse++;
  }
  iReedPrevStatus = iReedStatus;
}
