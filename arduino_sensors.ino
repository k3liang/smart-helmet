// include the library code:
#include <LiquidCrystal.h>
#include "DHT.h"
#include "GY521.h"
#include "Adafruit_PM25AQI.h"
#include <SoftwareSerial.h>

#define DHTPIN 2     // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

const int rs = 7, en = 8, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

GY521 sensor(0x68);

// this is for the PM2.5 air quality sensor
SoftwareSerial pmSerial(4, 3);

Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

void printLCD() {
  String data = Serial.readStringUntil('\n');
  lcd.clear();
  lcd.print(data);
}

void printLCD2() {
  String data = Serial.readStringUntil('_');
  String data2 = Serial.readStringUntil('\n');
  lcd.clear();
  lcd.print(data);
  lcd.setCursor(0, 1);
  lcd.print(data2);
}

void readTempHumid() {
  // takes roughly 250ms
  float h = dht.readHumidity();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early
  if (isnan(h) || isnan(f)) {
    Serial.println(-1);
    return;
  }

  Serial.println(h);
  Serial.println(f);
}

void readaccel() {
  sensor.readAccel();
  float ax = sensor.getAccelX();
  float ay = sensor.getAccelY();
  float az = sensor.getAccelZ();
  
  Serial.println(ax,3);
  Serial.println(ay,3);
  Serial.println(az,3);
}

void readAQI() {
  PM25_AQI_Data data;
  
  if (! aqi.read(&data)) {
    //Serial.println("Could not read from AQI");
    Serial.println(-1);
    return;
  }
  Serial.println(data.particles_03um);
}

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  //Serial.begin(115200);
  Serial.begin(9600);

  delay(3000);

  dht.begin();

  // gy-521
  Wire.begin();

  delay(100);
  while (sensor.wakeup() == false)
  {
    delay(1000);
  }
  sensor.setAccelSensitivity(0);  //  2g
  sensor.setThrottle(false);
  
  //  set calibration values
  sensor.axe = 0.181;
  sensor.aye = 0.024;
  sensor.aze = -1.003;

  pmSerial.begin(9600);
  if (! aqi.begin_UART(&pmSerial)) { // connect to the sensor over software serial 
    while (1) delay(10);
  }

  // Print a message to the LCD.
  lcd.print("STARTED");
  Serial.println(1);
}

void loop() {

  if (Serial.available() > 0) {
    //String data = Serial.readStringUntil('\n');
    char data = (char) Serial.read();
    switch (data) {
      case 'T':
        readTempHumid();
        break;
      case 'A':
        readaccel();
        break;
      case 'Q':
        readAQI();
        break;
      case 'P':
        printLCD();
        break;
      case 'p':
        printLCD2();
        break;
    }
  }
}
