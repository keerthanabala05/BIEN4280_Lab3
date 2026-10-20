/*
 * Author: Keerthana Balakrishnan
 * Date Created: October 6, 2026
 * Assignment: BIEN 4280 - Lab 3
 *
 * Description:
 * This program uses a Ticker interrupt and EventFlags to
 * alternate between humidity and temperature measurements
 * using the HS3000 sensor on the Arduino Nano 33 BLE Sense Rev2.
 * Two threads handle the measurements and control separate LEDs.
 * I2C communication is used to retrieve sensor data.
 */


#include "mbed.h"
#include "USBSerial.h"

using namespace rtos;
using namespace std::chrono;

USBSerial serial;

// protect USBSerial / printf
Mutex serialMutex;
Mutex i2cMutex;

// define threads
Thread humiditythread;
Thread temperaturethread;

// event flags
EventFlags sensorFlags;

// ticker
Ticker sensorTicker;

 // turns on  power for sensor- part 3
DigitalOut sensorPower(P0_22);

// enable pull-up i2c- part 3
DigitalOut i2cPullup(P1_0);

// initalize I2C (SDA, SCL)- part 3
I2C i2c(P0_14, P0_15);

// leds
DigitalOut humidityLED (P0_6); //BLUE
DigitalOut temperatureLED (P0_24); //RED

// sensor address, shifted to the left by 1
#define HS3000_sensor (0x44<<1)

// event flag values
#define HUMIDITY    0x01
#define TEMPERATURE 0x02

// Used by ISR to alternate between flags
volatile bool humidityNext = true;

// calculating humidity and temperature output

float calculate_humidity(uint16_t rawHumidity)
{
    return(rawHumidity / 16383.0f) * 100.0f;
}

float calculate_temperature(uint16_t rawTemperature)
{
    return(rawTemperature / 16383.0f ) * 165.0f - 40.0f;
}

// read temperature and humidity from semsor

bool read_sensor(float &humidity, float &temperature)
{
    char data[4];
    i2cMutex.lock();

    // measures and checks for ACK
    if (i2c.write(HS3000_sensor, nullptr, 0) != 0)
    {
        i2cMutex.unlock();
        return false;
    }

    // Wait for measurement
    ThisThread::sleep_for(40ms);

    // Read four bytes and check for ACK
    if (i2c.read(HS3000_sensor, data, 4) != 0)
    {
        i2cMutex.unlock();
        return false;
    }

    
    // Check if sensor data is fresh
    uint8_t status = ((uint8_t)data[0] >> 6) & 0x03;

    if (status != 0)
    {
        i2cMutex.unlock();
        return false;
    }

    //  first and second humidity bytes
    uint16_t firstByte = (uint8_t)data[0] & 0x3F;
    uint16_t secondByte = (uint8_t)data[1];

    // combine the bytes into raw humidity
    uint16_t rawHumidity = (firstByte << 8) | secondByte;

    // calculate humidity percentage
    humidity = calculate_humidity(rawHumidity);

    // raw temperature from last two bytes
    uint16_t tempByte1 = (uint8_t)data[2];
    uint16_t tempByte2 = (uint8_t)data[3];

    uint16_t rawTemperature =
        ((tempByte1 << 8) | tempByte2) >> 2;

    // calculate temperature in celsius
    temperature = calculate_temperature(rawTemperature);

    i2cMutex.unlock();
    return true;
}


// ISR to be called by ticker
void sensorISR()
{
    if (humidityNext)
    {
        sensorFlags.set(HUMIDITY);
    }
    else
    {
        sensorFlags.set(TEMPERATURE);
    }

    humidityNext = !humidityNext;
}

 
// humidity thread
void read_humidity()
{
    while(true)
    {
        // waits for event flag
        sensorFlags.wait_all(HUMIDITY);
        //turns on the LED
        humidityLED= 0;
       
        // variables to store sensor measurements
        float humidity;
        float temperature;

        // read sensor and also check if it work
        if (read_sensor(humidity, temperature))
        {
            serialMutex.lock();
            serial.printf("Humidity: %d %%RH\r\n", (int)humidity);
            serialMutex.unlock();
        }
        else
        {
            serialMutex.lock();
            serial.printf("Humidity read failed\r\n");
            serialMutex.unlock();
        }
        // keep LED on for a bit
        ThisThread::sleep_for(500ms);
        // Turn LED off
        humidityLED = 1;
    }
}

void read_temperature()
{
    while(true)
    {
        // waits for event flag
        sensorFlags.wait_all(TEMPERATURE);
        //turns on the LED
        temperatureLED= 0;
        
        // variables to store sensor measurements
        float humidity;
        float temperature;

        // read sensor and check if it worked
        if (read_sensor(humidity, temperature))
        {
            serialMutex.lock();
            serial.printf("Temperature: %d C\r\n", (int)temperature);
            serialMutex.unlock();
        }
        else
        {
            serialMutex.lock();
            serial.printf("Temperature read failed\r\n");
            serialMutex.unlock();
        }
        // keep LED on for a bit
        ThisThread::sleep_for(500ms);
        // Turn LED off
        temperatureLED = 1;
    }
}

// main
int main()
{
    // start LEDs off
    humidityLED=1;
    temperatureLED=1;

    // part 3
    sensorPower = 1;
    i2cPullup = 1;

    // waits for sensor to start
    ThisThread::sleep_for(100ms);
    // set frequency to 100 kHz
    i2c.frequency(100000);

    //start threads
    humiditythread.start(read_humidity);
    temperaturethread.start(read_temperature);

    //attach ticker
    sensorTicker.attach(&sensorISR, 1s);
    



    while (true) {
        ThisThread::sleep_for(1ms);
    }
}

