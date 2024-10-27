
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>

#define SERVER "solesensor.site"
#define PORT 80

Adafruit_AHTX0 aht;
SocketIOclient socketIO;

sensors_event_t humi, temp;
float fhum, ftem;
float fhums_hour[60] = {};
float ftems_hour[60] = {};
float fhums_min[6] = {};
float ftems_min[6] = {};
bool isWiFiConnected = false;
unsigned long lastRun = 0;
//
void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case sIOtype_DISCONNECT:
      Serial.printf("[IOc] Disconnected!\n");
      break;
    case sIOtype_CONNECT:
      Serial.printf("[IOc] Connected to url: %s\n", payload);

      // join default namespace (no auto join in Socket.IO V3)
      socketIO.send(sIOtype_CONNECT, "/");
      break;
    case sIOtype_EVENT:
      {
        String temp = String((char *)payload);
      }
      break;
    case sIOtype_ACK:
      Serial.printf("[IOc] get ack: %u\n", length);
      break;
    case sIOtype_ERROR:
      Serial.printf("[IOc] get error: %u\n", length);
      break;
    case sIOtype_BINARY_EVENT:
      Serial.printf("[IOc] get binary: %u\n", length);
      break;
    case sIOtype_BINARY_ACK:
      Serial.printf("[IOc] get binary ack: %u\n", length);
      break;
  }
}

void setup() {
  // put your setup code here, to run once:
  WiFi.disconnect();
  Serial.begin(9600);
  !aht.begin() ? Serial.println("Could not find AHT? Check wiring") : Serial.println("AHT10 or AHT20 found");
  WiFiManager wm;
  wm.autoConnect("ESP32-C3");
  Serial.println("Wifi connected :)");


  socketIO.begin(SERVER, PORT, "/socket.io/?EIO=4");
  // event handler
  socketIO.onEvent(socketIOEvent);
}

void loop() {
  socketIO.loop();
  if (millis() - lastRun > 10000) {
    lastRun = millis();
    if (WiFi.status() == WL_CONNECTED) isWiFiConnected = true;
    else isWiFiConnected = false;
    // put your main code here, to run repeatedly:
    aht.getEvent(&humi, &temp);
    Serial.println(temp.temperature);
    Serial.println(humi.relative_humidity);

    static int i = 0;
    static int y = 0;
    if (isWiFiConnected) {
      if (i > 0 || y > 0) {
        for (int t = 0; t < i; t++) {
          sendDataToServer(ftems_hour[i], fhums_hour[i]);
        }
        i = 0;
        y = 0;
        resetVal();
      }
      Serial.println("sending data to server");
      sendDataToServer(temp.temperature, humi.relative_humidity);
    } else {
      if (i == 60) {
        Serial.println("Limit reached");
        return;
      }
      ftems_min[y] = temp.temperature;
      fhums_min[y] = humi.relative_humidity;
      y++;
      if (y == 6) {
        ftems_hour[i] = calAverage(ftems_min, 6);
        fhums_hour[i] = calAverage(fhums_min, 6);
        i++;
        y = 0;
      }
    }
  }
}

float calAverage(float *arr, int length) {
  float sum = 0.0;
  for (int i = 0; i < length; i++) {
    sum += arr[i];
  }
  return sum / (float)length;
}

float randomFloat(int min, int max) {
  return random(min * 100, max * 100) / 100.0;
}

void resetVal() {
  for (int z = 0; z < 6; z++) {
    fhums_min[z] = 0;
    fhums_min[z] = 0;
  }
  for (int z = 0; z < 60; z++) {
    fhums_hour[z] = 0;
    fhums_hour[z] = 0;
  }
}


void sendDataToServer(float tem, float humi) {
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  array.add("message");
  JsonObject param1 = array.createNestedObject();

  param1["temp"] = tem;
  param1["humi"] = humi;
  param1["clientID"] = "esp";

  String output;
  serializeJson(doc, output);
  socketIO.sendEVENT(output);
}