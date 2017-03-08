//
// Reads sensor values via a 433MHz receiver and the Sensor433/RCSwitch library
// The sensor id maps to an MQTT topic and the value is published to an MQTT broker
//
#include <Sensor433.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "WIFI_and_broker_parameters.h"

//
// WIFI and MQTT setup
//
#define CLIENT_NAME "433Bridge"
WiFiClient wifiClient;
PubSubClient mqttClient(BROKER_IP,BROKER_PORT,wifiClient);

//
// 433MHz receiver setup
//
#define RECEIVER_INTERRUPT_PIN  12
Sensor433::Receiver receiver = Sensor433::Receiver(RECEIVER_INTERRUPT_PIN);

//
// The sensor id:s, their encoding and corresponding MQTT topics
//
#define NUMTYPES 10

#define TOPFLOOR_TEMP_ID         1
#define BMP_PRESSURE_ID          2
#define FRONT_DOOR_OPENED_ID     3
#define GARDEN_TEMP_ID           4
#define GARDEN_SOIL_TEMP_ID      5
#define BATHROOM_TEMP_ID         6
#define BATHROOM_HUMIDITY_ID     7
#define LAUNDRY_TEMP_ID          8
#define LAUNDRY_HUMIDITY_ID      9

#define ENC_NOTDEFINED  0
#define ENC_WORD        1
#define ENC_FLOAT       2

const byte encodingTypes[] =  { 
  ENC_NOTDEFINED, 
  ENC_FLOAT,                  
  ENC_WORD,                 
  ENC_WORD,
  ENC_FLOAT,
  ENC_FLOAT,
  ENC_FLOAT,
  ENC_FLOAT,
  ENC_FLOAT,
  ENC_FLOAT
};

const char* topics[] = { 
"Dummy",        
"Home/TopFloor/Temperature",
"Home/TopFloor/Pressure", 
"Home/FrontDoor/Status",
"Home/Garden/Temperature",
"Home/Garden/Soil/Temperature",
"Home/Bathroom/Temperature",
"Home/Bathroom/Humidity",
"Home/Laundry/Temperature",
"Home/Laundry/Humidity"
};


void setup() 
{  
  Serial.begin(9600);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
}
  
void loop() 
{
  if (receiver.hasNewData()) 
  {
    Sensor433::ReceivedMessage message = receiver.getData();

    BridgeToMQTT(message);
  }
}

int frontdoorcount=0;
void BridgeToMQTT(Sensor433::ReceivedMessage message)
{
  word data = message.dataAsWord;
  byte sensorId = message.sensorId;
  
  if (sensorId == FRONT_DOOR_OPENED_ID)
  {
    data = data + frontdoorcount;
    frontdoorcount++;
    if (frontdoorcount == 50)
    {
      frontdoorcount=0;
    }
  }

  float pubValue = data;
  if (encodingTypes[sensorId] == ENC_FLOAT)
  {
    pubValue = message.dataAsFloat;
  }

  if (!mqttClient.connected()) 
  {
    connectToWiFiAndBroker();
  }

  mqttClient.loop();

  publishFloatValue(pubValue,topics[sensorId]);
}

void connectToWiFiAndBroker() 
{
  Serial.print("Connecting to WIFI");
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected to WIFI!");

  Serial.println("Connecting to broker");
  while (!mqttClient.connect(CLIENT_NAME)) 
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected to broker!");
}

char msg[50];
void publishFloatValue(float value, const char* topic)
{
  if (isnan(value)) 
  {
    Serial.println("Invalid value!");
    return;
  }
  
  Serial.println("Publishing a new value");
  ftoa(msg,value);
  Serial.println(msg);
  mqttClient.publish(topic, msg);
}

char *ftoa(char *buffer, float f)
{ 
  char *returnString = buffer;
  long integerPart = (long)f;
  itoa(integerPart, buffer, 10);
  while (*buffer != '\0') buffer++;
  *buffer++ = '.';
  long decimalPart = abs((long)((f - integerPart) * 100));
  itoa(decimalPart, buffer, 10);
  return returnString;
}

