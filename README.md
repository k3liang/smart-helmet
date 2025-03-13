# Smart Helmet for Worker Safety

Every code file in this folder has been written by us, 
except for the `governor.c` and `governor.h` files, which we took from 
Assignment 2 to help us change CPU frequency.

Pay special attention to
- `sensor_interaction.c` and `sensor_interaction.h`
- `scheduler2.c` and `scheduler2.h`
- `eye_detector.py`
- `arduino_sensors.ino`  
These files are where a majority of our code is written in.

**To run:**
1. create a virtual environment (for the python packages)
2. `sudo apt install cmake`
3. `sudo apt install python3-opencv`
4. `sudo apt install python3-scipy`
5. `pip install dlib`
6. `make`
7. `sudo -E env PATH="$PATH" ./main_section`

**Main libraries used:**
- Arduino sensor reading/outputting (temperature/humidity, accelerometer, air quality, LCD)
    - `<LiquidCrystal.h>`
    - `"DHT.h"`
    - `"GY521.h"`
    - `"Adafruit_PM25AQI.h"`
- Fatigue Detection
    - `OpenCV`
    - `Python's C API`