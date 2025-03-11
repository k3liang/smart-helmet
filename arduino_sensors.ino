// include the library code:
#include <LiquidCrystal.h>

#include "DHT.h"

// use GY-521 library
#include "GY521.h"

#include "Adafruit_PM25AQI.h"
#include <SoftwareSerial.h>

#define DHTPIN 2     // Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 7, en = 8, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


GY521 sensor(0x68);



// If your PM2.5 is UART only, for UNO and others (without hardware serial) 
// we must use software serial...
// pin #2 is IN from sensor (TX pin on sensor), leave pin #3 disconnected
// comment these two lines if using hardware serial
SoftwareSerial pmSerial(4, 3);

Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

void printLCD() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  //lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  //lcd.print(millis() / 1000);

  String data = Serial.readStringUntil('\n');
  lcd.clear();
  lcd.print(data);
}

void readTempHumid() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  //Serial.print(F("Humidity: "));
  Serial.println(h);
  //Serial.print(F("%  Temperature: "));
  //Serial.print(t);
  //Serial.print(F("°C "));
  Serial.println(f);
  //Serial.print(F("°F  Heat index: "));
  //Serial.print(hic);
  //Serial.print(F("°C "));
  //Serial.print(hif);
  //Serial.println(F("°F"));
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
    
    //delay(500);  // try again in a bit!
    return;
  }
  Serial.println(data.particles_03um);
  //Serial.println("AQI reading success");

  //Serial.println(F("---------------------------------------"));
  //Serial.println(F("Concentration Units (standard)"));
  //Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_standard);
  //Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_standard);
  //Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_standard);
  //Serial.println(F("---------------------------------------"));
  //Serial.println(F("Concentration Units (environmental)"));
  //Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_env);
  //Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_env);
  //Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_env);
  //Serial.println(F("---------------------------------------"));
  //Serial.print(F("Particles > 0.3um / 0.1L air:")); Serial.println(data.particles_03um);
  
  /*
  Serial.print(F("Particles > 0.5um / 0.1L air:")); Serial.println(data.particles_05um);
  Serial.print(F("Particles > 1.0um / 0.1L air:")); Serial.println(data.particles_10um);
  Serial.print(F("Particles > 2.5um / 0.1L air:")); Serial.println(data.particles_25um);
  Serial.print(F("Particles > 5.0um / 0.1L air:")); Serial.println(data.particles_50um);
  Serial.print(F("Particles > 10 um / 0.1L air:")); Serial.println(data.particles_100um);
  Serial.println(F("---------------------------------------"));
  Serial.println(F("AQI"));
  Serial.print(F("PM2.5 AQI US: ")); Serial.print(data.aqi_pm25_us);
  Serial.print(F("\tPM10  AQI US: ")); Serial.println(data.aqi_pm100_us);
  */
  //  Serial.print(F("PM2.5 AQI China: ")); Serial.print(data.aqi_pm25_china);
  //  Serial.print(F("\tPM10  AQI China: ")); Serial.println(data.aqi_pm100_china);
  //Serial.println(F("---------------------------------------"));
  //Serial.println();
}

void setup() {
  // put your setup code here, to run once:

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  // dht
  Serial.begin(115200);
  //Serial.println(F("DHTxx test!"));

  delay(3000);

  dht.begin();

  // gy-521
  Wire.begin();

  delay(100);
  while (sensor.wakeup() == false)
  {
    //Serial.print(millis());
    //Serial.println("\tCould not connect to GY521: please check the GY521 address (0x68/0x69)");
    delay(1000);
  }
  sensor.setAccelSensitivity(0);  //  2g
  //sensor.setGyroSensitivity(0);   //  250 degrees/s

  sensor.setThrottle(false);
  //Serial.println("start...");
  
  //  set calibration values from calibration sketch.
  sensor.axe = 0.167;
  sensor.aye = 0.019;
  sensor.aze = -1.005;
  //sensor.gxe = 2.100;
  //sensor.gye = -0.738;
  //sensor.gze = 1.461;

  pmSerial.begin(9600);
  if (! aqi.begin_UART(&pmSerial)) { // connect to the sensor over software serial 
    //Serial.println("Could not find PM 2.5 sensor!");
    while (1) delay(10);
  }

  // Print a message to the LCD.
  lcd.print("STARTED");
  Serial.println(1);
}

void loop() {
  // put your main code here, to run repeatedly:

  //printLCD();

  // dht
  // Wait a few seconds between measurements.
  //delay(2000);
  //delay(100);

  //readTempHumid();

  //readaccel();

  //readAQI();

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
    }
  }
}
