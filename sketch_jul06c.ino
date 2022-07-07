/*
  Rui Santos
  Complete project details at our blog: https://RandomNerdTutorials.com/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
// Based on this library example: https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino/blob/master/examples/SecureBatchWrite/SecureBatchWrite.ino

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_ADXL345_U.h>
/* Assign a unique ID to this sensor at the same time */
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
  #define WIFI_AUTH_OPEN ENC_TYPE_NONE
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// WiFi AP SSID
#define WIFI_SSID "Tien Vu"
// WiFi password
#define WIFI_PASSWORD "12345689"
// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "https://europe-west1-1.gcp.cloud2.influxdata.com"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN "vDct1YZ3mu-cipzWcI1sToT5JISGV3zBxJJUyPZrvJrBaelOTGyMWQ0tN8CX_2LRbwrIx9m0QaKnMRMMjLdTBw=="
// InfluxDB v2 organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_ORG "khsnbc958@gmail.com"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "Iot"
// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time:   "PST8PDT"
//  Eastern:        "EST5EDT"
//  Japanesse:      "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "<+07>-7"

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// InfluxDB client instance without preconfigured InfluxCloud certificate for insecure connection 
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// Data point
Point sensorReadings("measurements");

//BME280
Adafruit_BME280 bme; // I2C

float XAxis;
float YAxis;
float ZAxis;
int pin = 2;

void initADXL(void) 
{
#ifndef ESP8266
  while (!Serial); // for Leonardo/Micro/Zero
#endif
  Serial.begin(9600);
  Serial.println("Accelerometer Test"); Serial.println("");

  /* Initialise the sensor */
  if(!accel.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.println("Ooops, no ADXL345 detected ... Check your wiring!");
    while(1);
  }

  /* Set the range to whatever is appropriate for your project */
  accel.setRange(ADXL345_RANGE_16_G);
  // accel.setRange(ADXL345_RANGE_8_G);
  // accel.setRange(ADXL345_RANGE_4_G);
  // accel.setRange(ADXL345_RANGE_2_G);

  /* Display some basic information on this sensor */
}

void setup() {
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  //Init BME280 sensor
  initADXL();

  // Add tags
  sensorReadings.addTag("device", DEVICE);
  sensorReadings.addTag("location", "home");
  sensorReadings.addTag("sensor", "ADXL345");

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area:

//[https://www.pool.ntp.org/zone/](https://l.facebook.com/l.php?u=https%3A%2F%2Fwww.pool.ntp.org%2Fzone%2F%3Ffbclid%3DIwAR0Kcka58ivXReFZURBQfzo0rNJbb4gseL6QZLcDGH5tp4SALeKIyOFWST4&h=AT3VWH2FDrC-4bTjmpKN1NHTpQgad9naZ2gldcDp9XA2L0w2jy4ITYsLVo2NclwsN8wO-3DrhelxrznKFUDpFI7aRdj52hrbjwK1nPKiOAeCfEandP3Qc3n1yckYosnN3H7_cg)

// Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // LED: initialize GPIO 2 as an output.
  pinMode(pin, OUTPUT);
}

void loop() {
  sensors_event_t event; 
  accel.getEvent(&event);
  XAxis = event.acceleration.x;
  YAxis = event.acceleration.y;
  ZAxis = event.acceleration.z;

  // Add readings as fields to point
  sensorReadings.addField("X Axis", XAxis);
  sensorReadings.addField("Y Axis", YAxis);
  sensorReadings.addField("Z Axis", ZAxis);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(sensorReadings));

  // Write point into buffer
  client.writePoint(sensorReadings);

  // Clear fields for next usage. Tags remain the same.
  sensorReadings.clearFields();

  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  Serial.print("X: "); Serial.print(XAxis); Serial.print("  ");
  Serial.print("Y: "); Serial.print(YAxis); Serial.print("  ");
  Serial.print("Z: "); Serial.print(ZAxis); Serial.print("  ");Serial.println("m/s^2 ");
  // Wait 1s
  digitalWrite(pin, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);               // wait for a second
  digitalWrite(pin, LOW);    // turn the LED off by making the voltage LOW
}
