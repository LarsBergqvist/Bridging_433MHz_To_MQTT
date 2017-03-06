//
// Reads sensor values via a 433MHz receiver and the RCSwitch library
// The sensor id maps to an MQTT topic and the value is published to an MQTT broker
//
#include <RCSwitch.h>
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
RCSwitch receiver = RCSwitch();

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
   
  pinMode(12, INPUT);
  receiver.enableReceive(12);
}

  
int frontdoorcount=0;
int prevValue=-1;
int numIdenticalInRow=1;
void loop() 
{
  if (receiver.available()) 
  {
    int value = receiver.getReceivedValue();
    
    if (value == 0) 
    {
      Serial.print("Unknown encoding");
    } 
    else 
    {
      DecodeAndPublish(value);
      receiver.resetAvailable();
    }
  }
}

void DecodeAndPublish(int value)
{
  if (value == prevValue)
  {
    numIdenticalInRow++;
  }
  else
  {
    numIdenticalInRow = 0;
  }

  // Get the different parts of the 32-bit / 4-byte value
  // that has been read over 433MH<
  unsigned int checksum = value & 0x000000FF;
  unsigned int data = (value >> 8) & 0x0000FFFF;
  unsigned int byte3 = (value >> 24) & 0x000000FF;
  unsigned int seq = byte3 & 0x0F;
  unsigned int typeID = (byte3 & 0xF0) >> 4;

  byte calculatedCheckSum = 0xFF & (typeID + seq + data);
  
  if ((typeID <= (NUMTYPES-1)) && (calculatedCheckSum == checksum) && (seq <= 15))
  {
    prevValue = value;

    // Require at least two readings of the same value in a row
    // to reduce risk of reading noise. Ignore any further duplicated
    // values
    if (numIdenticalInRow == 2)
    {
      //
      // Add increment for static value from door sensor
      // so that Home Assistant detects event changes
      //
      if (typeID == FRONT_DOOR_OPENED_ID)
      {
        data = data + frontdoorcount;
        frontdoorcount++;
        if (frontdoorcount == 50)
        {
          frontdoorcount=0;
        }
      }

      float pubValue = data;
      if (encodingTypes[typeID] == ENC_FLOAT)
      {
        pubValue = DecodeTwoBytesToFloat(data);
      }
  
      if (!mqttClient.connected()) 
      {
        connectToWiFiAndBroker();
      }
  
      mqttClient.loop();
  
      publishFloatValue(pubValue,topics[typeID]);
    }
  }
}

float DecodeTwoBytesToFloat(unsigned int word)
{
  bool sign = false;
  
  if ((word & 0x8000) == 0x8000) 
    sign=true;  

  float fl = (word & 0x7FFF) / 100.0;
        
  if (sign)
    fl = -fl;

  return fl;
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

