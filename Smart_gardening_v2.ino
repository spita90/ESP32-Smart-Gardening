#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Husarnet.h>
#include <ArduinoJson.h>


// WiFi credentials
#define NUM_NETWORKS 4

const char* ssidTab[NUM_NETWORKS] = {
  "wifi-ssid-1",
  "wifi-ssid-2",
  "wifi-ssid-3",
  "wifi-ssid-4",
};
const char* passwordTab[NUM_NETWORKS] = {
  "wifi-pass-1",
  "wifi-pass-2",
  "wifi-pass-3",
  "wifi-pass-4",
};

const char* hostName = "esp32";  //the name of the 1st ESP32 device at https://app.husarnet.com
const char* husarnetJoinCode = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; //join code of Husarnet device
#define HTTP_PORT 8000
#define WEBSOCKET_PORT 8001
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);
HusarnetServer server(HTTP_PORT);
bool wsconnected = false;

StaticJsonBuffer<200> jsonBufferTx;
JsonObject& rootTx = jsonBufferTx.createObject();
StaticJsonBuffer<100> jsonBufferRx;

const char* html =
#include "html.h"
  ;

const char* ntpServer = "pool.ntp.org";         //parameters for time synchronization
const long  gmtOffset_sec = 3600;				//change the offset according to your time zone
const int   daylightOffset_sec = 3600;
struct tm timeinfo;

const int soilSensorPin = 32;                   //pin layout
const int waterPumpPin = 33;
const int lightSensorPin = 36;
const int lampSwitchPin = 15;

//Hours are from 00 to 23
const int wateringStartHour = 22;               //if necessary, soil will be watered each day only between this hour
const int wateringStopHour = 7;                 //and this hour
const bool ignoreWateringHours = false;         //if set to "true" it ignores the above defined watering hours and waters the soil
                                                //each time the soil moisture goes below preferred value.
                                                //Warning: it can water the soil even in plain daylight, potentially cooking the plant

const int lampStartHour = 16;                   //turn on lamp each day from this hour
const int lampStopHour = 19;                    //until this hour

const int soilPrefMoisturePerc = 60;            //if soil moisture goes below this percentage it will be watered
const int lightSensorThreshold = 3000;          //light sensor threshold to guess if the lamp is on or off

const int loopWaitMillis = 1000;
const int chartUpdateFreqSec = 15;

int waterPumpStatus = 0;
int lampStatus = 0;
int overrideWaterPump = 0;
int overrideLamp = 0;


void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      {
        wsconnected = false;
        Serial.printf("[%u] Disconnected\r\n", num);
      }
      break;
    case WStype_CONNECTED:
      {
        wsconnected = true;
        Serial.printf("\r\n[%u] Connection from Husarnet \r\n", num);
      }
      break;

    case WStype_TEXT:
      {
        JsonObject& rootRx = jsonBufferRx.parseObject(payload);
        jsonBufferRx.clear();

        overrideWaterPump = rootRx["waterOverride"];
        overrideLamp = rootRx["lampOverride"];
      }
      break;

    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }

}

void setup()
{
  Serial.begin(115200);

  pinMode(soilSensorPin, INPUT);
  pinMode(waterPumpPin, OUTPUT);
  pinMode(lightSensorPin, INPUT);
  pinMode(lampSwitchPin, OUTPUT);

  //reset actuators
  digitalWrite(waterPumpPin, LOW);
  if(lampIsOn())
    toggleLamp();

  xTaskCreate(
    taskWifi,          /* Task function. */
    "taskWifi",        /* String with name of task. */
    10000,            /* Stack size in bytes. */
    NULL,             /* Parameter passed as input of the task */
    1,                /* Priority of the task. */
    NULL);            /* Task handle. */

  xTaskCreate(
    taskHTTP,          /* Task function. */
    "taskHTTP",        /* String with name of task. */
    10000,            /* Stack size in bytes. */
    NULL,             /* Parameter passed as input of the task */
    2,                /* Priority of the task. */
    NULL);            /* Task handle. */

  xTaskCreate(
    taskWebSocket,          /* Task function. */
    "taskWebSocket",        /* String with name of task. */
    10000,            /* Stack size in bytes. */
    NULL,             /* Parameter passed as input of the task */
    1,                /* Priority of the task. */
    NULL);            /* Task handle. */

  xTaskCreate(
    taskStatus,          /* Task function. */
    "taskStatus",        /* String with name of task. */
    10000,            /* Stack size in bytes. */
    NULL,             /* Parameter passed as input of the task */
    1,                /* Priority of the task. */
    NULL);            /* Task handle. */
}

void taskStatus( void * parameter )
{
  int currentHour;
  String output;
  
  while (1) {
    
    while(!getLocalTime(&timeinfo)){
      delay(200);
    }
    currentHour = timeinfo.tm_hour;
      
    if(overrideLamp==1 && !lampIsOn()){                   //current state, and defined on and off hours
      Serial.println("Lamp ON");
      toggleLamp();
      lampStatus = 1;
    }else if (overrideLamp==0){
      if(!lampIsOn() && hourIsInHourRange(currentHour, lampStartHour, lampStopHour)){
        Serial.println("Lamp ON");
        toggleLamp();
        lampStatus = 1;
      }else if(lampIsOn() && !hourIsInHourRange(currentHour, lampStartHour, lampStopHour)){
        Serial.println("Lamp OFF");
        toggleLamp();
        lampStatus = 0;
      }
    }
    
    int soilMoistPerc = currentSoilMoistPerc();                     //get current soil moisture percentage
    
    if(overrideWaterPump==1){                                  //handle water pump power accrding to ooverride, soil moisture,
      Serial.println("Watering...");                         //boolean ignoreWateringHours, and defined watering hours
      digitalWrite(waterPumpPin, HIGH);
      waterPumpStatus = 1;
    }else if(overrideWaterPump==0){
      if((soilMoistPerc<soilPrefMoisturePerc) && (ignoreWateringHours || hourIsInHourRange(currentHour, wateringStartHour, wateringStopHour))){
        Serial.println("Watering...");
        digitalWrite(waterPumpPin, HIGH);
        waterPumpStatus = 1;
      }else{
        digitalWrite(waterPumpPin, LOW);
        waterPumpStatus = 0;
      }
    }

    if (wsconnected == true) {
      output = "";

      rootTx["jsonMode"] = "default";
      rootTx["soil"] = soilMoistPerc;
      rootTx["lamp"] = lampStatus;
      rootTx["water"] = waterPumpStatus;
      rootTx.printTo(output);

      Serial.print(F("Sending: "));
      Serial.print(output);
      Serial.println();

      webSocket.sendTXT(0, output);
    }
    delay(loopWaitMillis);
  }
}

void loop()
{
	//this only handles the chart updating

  while(!getLocalTime(&timeinfo)){
      delay(200);
    }
    
  int currentHour = timeinfo.tm_hour;
  int currentMin = timeinfo.tm_min;
  int soilMoistPerc = currentSoilMoistPerc();
  
  if (wsconnected == true) {
    String chartOutput = "";
  
    rootTx["jsonMode"] = "chart";
    rootTx["hour"] = currentHour;
    rootTx["min"] = currentMin;
    rootTx["reading"] = soilMoistPerc;
    rootTx.printTo(chartOutput);
  
    Serial.print(F("Sending: "));
    Serial.print(chartOutput);
    Serial.println();
  
    webSocket.sendTXT(0, chartOutput);
  }
  
  delay(chartUpdateFreqSec*1000);
}


void taskWifi( void * parameter ) {
  while (1) {
    for (int i = 0; i < NUM_NETWORKS; i++) {
      Serial.println();
      Serial.print("Connecting to ");
      Serial.print(ssidTab[i]);
      WiFi.begin(ssidTab[i], passwordTab[i]);
      for (int j = 0; j < 10; j++) {
        if (WiFi.status() != WL_CONNECTED) {
          delay(3000);
          Serial.print(".");
        } else {
          Serial.println("done");
          Serial.print("IP address: ");
          Serial.println(WiFi.localIP());

          configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);       //synchronize time from internet
          printLocalTime();                                               //and print it
          
          Husarnet.join(husarnetJoinCode, hostName);
          Husarnet.start();

          server.begin();

          while (WiFi.status() == WL_CONNECTED) {
            delay(500);
          }
        }
      }
    }
  }
}

void taskHTTP( void * parameter )
{
  String header;

  while (1) {
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }

    HusarnetClient client = server.available();

    if (client) {
      Serial.println("New Client.");
      String currentLine = "";
      Serial.printf("connected: %d\r\n", client.connected());
      while (client.connected()) {

        if (client.available()) {
          char c = client.read();
          Serial.write(c);
          header += c;
          if (c == '\n') {
            if (currentLine.length() == 0) {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

              client.println(html);
              break;
            } else {
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
        }
      }

      header = "";

      client.stop();
      Serial.println("Client disconnected.");
      Serial.println("");
    } else {
      delay(200);
    }
  }
}

void taskWebSocket( void * parameter )
{
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  while (1) {
    webSocket.loop();
    delay(1);
  }
}

bool hourIsInHourRange(int hour, int startHour, int endHour){
  if(startHour<endHour){
    if(hour>=startHour && hour<=endHour){
      return true;
    }else{
      return false;
    }
  }else{
    if(hour>=startHour || hour<=endHour){
      return true;
    }else{
      return false;
    }
  }
}

bool lampIsOn(){
  if(analogRead(lightSensorPin)>=lightSensorThreshold){
    return true;
  }else{
    return false;
  }
}

int lampOn(){
  if(lampIsOn()){
    return 1;
  }else{
    return 0;
  }
}

void toggleLamp(){
  digitalWrite(lampSwitchPin, LOW);
  digitalWrite(lampSwitchPin, HIGH);
  delay(50);
  digitalWrite(lampSwitchPin, LOW);
}

int currentSoilMoistPerc(){
  float soilRead = analogRead(soilSensorPin);
  return (3480 - soilRead)/1830.0*100.0;
}

void printLocalTime(){
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
