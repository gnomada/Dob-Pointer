#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WebServerSecure.h>

#include <config/wifi.config.h>

ESP8266WebServer server(80);

// NTP
const long utcOffsetInSeconds = 0;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Status
int BOARD_STATUS_PIN = D0;
int WIFI_STATUS_PIN = D4;
int BOARD_STATUS = LOW;
int WIFI_STATUS = LOW;

Adafruit_MPU6050 mpu;


String ntp_now()
{
  timeClient.update();
  return timeClient.getFormattedTime();
}

void serial_log(String msg, String output)
{
  Serial.print("[" + ntp_now() + "] ");
  if (output != "")
  {
    Serial.print(msg);
    Serial.println(output);
  }
  else
  {
    Serial.println(msg);
  }
}

bool connectToWifi()
{
  byte timeout = 50;
  serial_log("WIFI: connecting to ", get_ssid());

  WiFi.begin(get_ssid(), get_pass());

  for (int i = 0; i < timeout; i++)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      serial_log("WIFI: connected to ", get_ssid());
      serial_log("WIFI: MAC address ", WiFi.macAddress());
      serial_log("WIFI: -> https://", WiFi.localIP().toString());

      if (MDNS.begin(get_name()))
      {
        // https://superuser.com/questions/491747/how-can-i-resolve-local-addresses-in-windows
        serial_log("MDNS: -> https://", String(get_name()) + ".local");
        pinMode(WIFI_STATUS_PIN, OUTPUT);
      }

      return true;
    }

    delay(1000);
  }

  serial_log("\nWIFI error: check network configiration or press RST", "");
  return false;
}

void get_data()
{
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Create a JSON object
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["acceleration_x"] = a.acceleration.x;
  jsonDoc["acceleration_y"] = a.acceleration.y;
  jsonDoc["acceleration_z"] = a.acceleration.z;
  jsonDoc["gyro_x"] = g.gyro.x;
  jsonDoc["gyro_y"] = g.gyro.y;
  jsonDoc["gyro_z"] = g.gyro.z;
  jsonDoc["temperature"] = temp.temperature;

  // Serialize the JSON object to a string
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  server.send(200, "application/json", jsonString);
}

void setup(void)
{
  Serial.begin(9600);
  pinMode(BOARD_STATUS_PIN, OUTPUT);

  while (!Serial)
    delay(10);

  if (!connectToWifi())
  {
    delay(10000);
    ESP.restart();
  }

  // NTP config
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // NTP start
  timeClient.begin();

  serial_log("Staring MPU6050", "");

  if (!mpu.begin()) {
    serial_log("Failed to find MPU6050 chip", "");
    while (1) {
      delay(10);
    }
  }
  serial_log("MPU6050 Found", "");

  // acce RANGE
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  switch (mpu.getAccelerometerRange()) {
  case MPU6050_RANGE_2_G:
    serial_log("Accelerometer range set to: ", "+- 2 G");
    break;
  case MPU6050_RANGE_4_G:
    serial_log("Accelerometer range set to: ", "+- 4 G");
    break;
  case MPU6050_RANGE_8_G:
    serial_log("Accelerometer range set to: ", "+- 8 G");
    break;
  case MPU6050_RANGE_16_G:
    serial_log("Accelerometer range set to: ", "+- 16 G");
    break;
  }

  // gyro RANGE
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  switch (mpu.getGyroRange()) {
  case MPU6050_RANGE_250_DEG:
    serial_log("Gyro range set to: ", "+- 250 deg/s");
    break;
  case MPU6050_RANGE_500_DEG:
    serial_log("Gyro range set to: ", "+- 500 deg/s");
    break;
  case MPU6050_RANGE_1000_DEG:
    serial_log("Gyro range set to: ", "+- 1000 deg/s");
    break;
  case MPU6050_RANGE_2000_DEG:
    serial_log("Gyro range set to: ", "+- 2000 deg/s");
    break;
  }

  // Filter bw
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  switch (mpu.getFilterBandwidth()) {
  case MPU6050_BAND_260_HZ:
    serial_log("Filter bandwidth set to: ", "+- 260 Hz");
    break;
  case MPU6050_BAND_184_HZ:
    serial_log("Filter bandwidth set to: ", "+- 184 Hz");
    break;
  case MPU6050_BAND_94_HZ:
    serial_log("Filter bandwidth set to: ", "+- 94 Hz");
    break;
  case MPU6050_BAND_44_HZ:
    serial_log("Filter bandwidth set to:  ", "+- 44 Hz");
    break;
  case MPU6050_BAND_21_HZ:
    serial_log("Filter bandwidth set to: ", "+- 21 Hz");
    break;
  case MPU6050_BAND_10_HZ:
    serial_log("Filter bandwidth set to: ", "+- 10 Hz");
    break;
  case MPU6050_BAND_5_HZ:
    serial_log("Filter bandwidth set to: ", "+- 5 Hz");
    break;
  }

  // HTTP server config
  server.on("/api/v1/mpu6050", HTTP_GET, get_data);
  server.begin();
  serial_log("HTTP: listening", "");
}

void loop()
{
  server.handleClient();
  MDNS.update();
}
