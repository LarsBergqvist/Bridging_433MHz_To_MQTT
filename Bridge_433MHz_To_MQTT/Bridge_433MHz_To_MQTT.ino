//
// Reads sensor values via a 433MHz transmitter and the RCSwitch protocol
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
#define TOPFLOOR_TEMP_ID         1
#define BMP_PRESSURE_ID          2
#define FRONT_DOOR_OPENED_ID     3

#define ENC_NOTDEFINED  0
#define ENC_WORD        1
#define ENC_FLOAT       2

byte encodingTypes[] =  { ENC_NOTDEFINED, ENC_FLOAT,                  ENC_WORD,                 ENC_WORD };
char* topics[] =        { "Dummy",        "Home/TopFloor/Temperature","Home/TopFloor/Pressure", "Home/FrontDoor/Status"};


void setup() 
{  
  Serial.begin(9600);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
   
  pinMode(12, INPUT);
  receiver.enableReceive(12);  // Receiver on interrupt 0 => that is pin #2
}


int prevValue=-1;
int numIdenticalInRow=0;
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
      if (value == prevValue)
      {
        numIdenticalInRow++;
      }
      else
      {
        numIdenticalInRow = 0;
      }

      unsigned long checksum = value & 0x000000FF;
      unsigned long data = (value >> 8) & 0x0000FFFF;
      unsigned long byte3 = (value >> 24) & 0x000000FF;
      unsigned long seq = byte3 & 0x0F;
      unsigned long measId = (byte3 & 0xF0) >> 4;

      byte calculatedCheckSum = 0xFF & (measId + seq + data);
      
      if ((measId <= 3) && (calculatedCheckSum == checksum) && (seq <= 15))
      {
        prevValue = value;

        if (numIdenticalInRow == 2)
        {
          unsigned int word = DecodeValue(value);
          float pubValue = word;
          if (encodingTypes[measId] == ENC_FLOAT)
          {
            pubValue = DecodeTwoBytesToFloat(word);
          }

          if (!mqttClient.connected()) 
          {
            connectToWiFiAndBroker();
          }

          mqttClient.loop();

          publishFloatValue(pubValue,topics[measId]);
        }
      }
    }
    
    receiver.resetAvailable();
  }
}

float DecodeTwoBytesToFloat(unsigned int word)
{
  bool sign = false;
  
  if ((word & 0x8000) == 0x8000) 
    sign=true;  

  float fl = (word & 0x7FFF) / 100;
        
  if (sign)
    fl = -fl;

  return fl;
}

unsigned long DecodeValue(unsigned long value)
{
    unsigned long checksum = value & 0xFF;
    unsigned long data = (value >> 8) & 0xFFFF;
    unsigned long byte3 = (value >> 24) & 0xFF;
    unsigned long seq = byte3 & 0x0F;
    unsigned long measId = (byte3 & 0xF0) >> 4;    
    return data;
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
void publishFloatValue(float value, char* topic)
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

