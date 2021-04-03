#include <BME280.h>
#include <BME280I2C.h>
#include <BME280I2C_BRZO.h>
#include <BME280Spi.h>
#include <BME280SpiSw.h>
#include <EnvironmentCalculations.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const String strWelcomeMessage = "Les fraises vous souhaitent la bienvenue";
String strHostname = "Tomates/Station météo";
String strMDNSName = "Tomages Station météo";
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

// BME280
BME280I2C::Settings settings(
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_16,
   BME280::SpiEnable_False,
   BME280I2C::I2CAddr_0x76 // I2C address. I2C specific.
);

BME280I2C bme(settings);
float temp(NAN), hum(NAN), pres(NAN);
BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
BME280::PresUnit presUnit(BME280::PresUnit_Pa);
EnvironmentCalculations::TempUnit envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;

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
  bme.read(pres, temp, hum, tempUnit, presUnit);
  server.send(200, "text/html", String(hum));
}

void temperature(){
  bme.read(pres, temp, hum, tempUnit, presUnit);
  server.send(200, "text/html", String(temp));
}

void ATMPressure(){
  bme.read(pres, temp, hum, tempUnit, presUnit);
  server.send(200, "text/html", String(pres));
}

void heatIndex(){
  bme.read(pres, temp, hum, tempUnit, presUnit);
  server.send(200, "text/html", String(EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit)));
}

void absHumidity(){
  bme.read(pres, temp, hum, tempUnit, presUnit);
  server.send(200, "text/html", String(EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit)));
}

void dewPoint(){
  bme.read(pres, temp, hum, tempUnit, presUnit);
  server.send(200, "text/html", String(EnvironmentCalculations::DewPoint(temp, hum, envTempUnit)));
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
  server.on(F("/ATMPressure"), HTTP_GET, ATMPressure);

  // Weather : env. calculations
  server.on(F("/heatIndex"), HTTP_GET, heatIndex);
  server.on(F("/absHumidity"), HTTP_GET, absHumidity);
  server.on(F("/dewPoint"), HTTP_GET, dewPoint);
}
  
void setup(void){
  Serial.begin(115200);

  // Relay
  pinMode(pinGPIORelay, OUTPUT);
  digitalWrite(pinGPIORelay, HIGH);
  iRelayStatus = 0;

  // Reed switch
  pinMode(pinGPIOReedSw, INPUT_PULLUP);
  iReedStatus = digitalRead(pinGPIOReedSw);
  iReedPrevStatus = iReedStatus;
  
  // BME280
  bme.begin();
  
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
