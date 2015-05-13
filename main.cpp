// Main.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "arduino.h"
#include "Wire.h"
#include <iostream>

// Grove drivers
#include <TH02_dev.h>
#include <rgb_lcd.h>

// timer classes
#include <ctime>
//#include <cstdio>

using namespace std;

int _tmain(int argc, _TCHAR* argv[])
{
    return RunArduinoSketch();
}

// define what sensors are where and initial values
const uint8_t led = D7;				// LED
const uint8_t speakerpin = D3;		// speaker
const uint8_t pot = A0;				// potentiometer

const uint8_t sound = A1;
float sound_reading = 0;

const uint8_t light_sensor = A3;	// light sensor
float lightSensor_reading = 0;
float light_resistance = 0;

// button modules - both could be an int, but a char is smaller.
const char button = D2;			// button can be physical or touch
char buttonState = 0;
char lastButtonState = 0;

const char touchBtn = D4;
char touchBtnState = 0;
char lastTouchBtnState = 0;

// relay module
char relayPin = D8;
char relayState = LOW;

// LM358 tempature (kit) module variables
const uint8_t pinTemp = A2;		// thermistor (kit) is pluged into, this is an analog device
const uint16_t B = 3975;		// used to calculate the B-Value of the thermistor
uint32_t val = 0;
float resistance = 0;
float temperature = 0;
uint16_t poll_interval = 1;	// default poll time (seconds) to read temp

// I2C temp sensor variables
float temper = 0;
float humidity = 0;

// lcd def
rgb_lcd lcd;
int lcdColor[] = { 0, 0, 255 };
int lcdState = 2;
bool lcdFlip = false;

// set the display time switch variables
float diff = 0;
bool flip = true;
clock_t viewTimer;			// toggle LCD timer

// set the pot range resolution is 0 - 1024 (10 bit resolution = 2^10)
uint8_t scaler = 102;		// scaler = (1024/10) where 10 is the target max on the Pot

void setup()
{
	// define the direction of the i/o sensors (non I2C devices)
	pinMode(led, OUTPUT);       // Configure the pin for OUTPUT so you can turn on the LED.
	pinMode(speakerpin, OUTPUT);
	pinMode(pot, INPUT);		// going to read from these devices configure for INPUT.
	pinMode(button, INPUT);
	pinMode(pinTemp, INPUT);	// thermistor
	pinMode(sound, INPUT);
	pinMode(touchBtn, INPUT);

	// relay sensor
	pinMode(relayPin, OUTPUT);

	// light sensor
	pinMode(light_sensor, INPUT);

	// lcd config
	lcd.clear();
	lcd.begin(16, 2);			// define number of (colums, rows) the LCD has
	lcd.setColor(3);			// default to blue
	lcd.setCursor(0, 0);		// default cursor position syntax is (<Row>, <Column>) so lcd.setCursor(0, 0);
	lcd.print("Starting...");

	Serial.begin(9600);
	
	// delay to let TH02 voltage to stablize
	delay(150);
	TH02.begin();
}

void beepTone(int tone, int duration)
{
	for (long i = 0; i < duration * 1000L; i+= tone * 2)
	{
		digitalWrite(speakerpin, HIGH);
		delayMicroseconds(tone);
		digitalWrite(speakerpin, LOW);
		delayMicroseconds(tone);
	}
}

void beepDuration(int duration)
{
	digitalWrite(led, HIGH);
	for (long i = 0; i < duration * 1000L; i++)
	{
		digitalWrite(speakerpin, HIGH);
	}
	digitalWrite(led, LOW);
	digitalWrite(speakerpin, LOW);
}

void beepOn()
{
	digitalWrite(speakerpin, HIGH);
}

void beepOff()
{
	digitalWrite(speakerpin, LOW);
}

void ChangeLcdColor()
{
	lcdColor[lcdState] = 0;
	if (lcdState == 2)
	{
		lcdState = 0;
		lcdColor[lcdState] = 255;
	}
	else
	{
		lcdState++;
		lcdColor[lcdState] = 255;
	}

	lcd.setRGB(lcdColor[0], lcdColor[1], lcdColor[2]);
}

// read the pot, return the calc scaled value
int ReadPot()
{
	int potValue = analogRead(pot);

	// returning a uint8_t, range must be less than 255
	return abs(potValue / scaler);
}

void toggleRelay()
{
	// get the relay state
	relayState = digitalRead(relayPin);
	if (relayState)
	{
		digitalWrite(relayPin, LOW);
	}
	else
	{
		digitalWrite(relayPin, HIGH);
	}
}

void toggleLcd()
{
	if (lcdFlip)
	{
		// off
		lcd.setRGB(0, 0, 0);
		lcd.noDisplay();
	}
	else
	{
		// on
		lcd.setRGB(lcdColor[0], lcdColor[1], lcdColor[2]);
		lcd.display();
	}

	lcdFlip = !lcdFlip;
}

// display the temp data
void DisplayData(float time)
{
	// flip switches views
	diff = (std::clock() - time) / (double)CLOCKS_PER_SEC;
	if (diff > 2)
	{
		if (flip)
		{
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Thrm:");
			lcd.setCursor(0, 1);
			lcd.print("TH02:");

			lcd.setCursor(6, 0);
			lcd.print(temperature);
			lcd.setCursor(6, 1);
			lcd.print(temper);
		}
		else
		{
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Humidity:");
			lcd.setCursor(0, 1);
			lcd.print(humidity);
		}

		// now toggle the switch and reset the timer
		flip = !flip;
		viewTimer = clock();
	}
}

// (-- MAIN --) the loop routine runs over and over again forever:
void loop()
{
	// reset devices
	digitalWrite(led, LOW);
	beepOff();
	digitalWrite(relayPin, LOW);

	// set timers
	clock_t pollTimer;		// sensor polling timer for temp

	// polling timer
	double poll_Diff = 0;
	pollTimer = clock();	// starts the timer by storing the time.Now

	// display timer
	double view_Diff = 0;
	viewTimer = clock();	// start the display view time

	while (true)
	{
		// is the button pressed?
		buttonState = digitalRead(button);
		touchBtnState = digitalRead(touchBtn);

		if (touchBtnState == HIGH && lastTouchBtnState == LOW)
		{
			lastTouchBtnState = HIGH;
		}

		if (touchBtnState == LOW && lastTouchBtnState == HIGH)
		{
			//toggleRelay();
			toggleLcd();
			beepDuration(5);
			lastTouchBtnState = LOW;
		}

		// what to do when the button is pressed (this should be a switch statement to remove the redundant if checks) 
		if (buttonState == HIGH && lastButtonState == LOW)
		{
			// button is pressed
			lastButtonState = HIGH;
		}
		if (buttonState == LOW && lastButtonState == HIGH)
		{
			// button was pressed and released (now take some action)
			// change the lcd color and set lastButtonState to LOW
			ChangeLcdColor();
			beepDuration(5);
			lastButtonState = LOW;
		}

		// read the scaled pot
		int poll_interval = ReadPot();

		// calculate the difference in time. calculated by (what time is it now - what time was it when the clock started)/1000
		poll_Diff = (std::clock() - pollTimer) / (double)CLOCKS_PER_SEC;
		if (poll_Diff > poll_interval)
		{
			// measure the temp via LM358 kit sensor
			val = analogRead(pinTemp);
			resistance = (float)(1023 - val) * 10000 / val;
			temperature = 1 / (log(resistance / 10000) / B + 1 / 298.15) - 273.15;
			//Serial.println(temperature);
			//Log((int)temperature + L"\n");
			std::cout << "--->Therm temp is: " << temperature << " taken every: " << poll_interval << " seconds." << std::endl;

			// measure the temp via I2C device
			temper = TH02.ReadTemperature();
			humidity = TH02.ReadHumidity();

			std::cout << "---> TH02 temp is: " << temper << endl;
			std::cout << "---> TH02 humidity is: " << humidity << endl;

			// print the temps to the LCD - don't need to do this here
			//DisplayData();

			/*
			if (temper > 22)
			{
				// turn on the relay, check to see if it's already on...
				digitalWrite(relayPin, HIGH);
			}
			else
			{
				// turn off the relay
				digitalWrite(relayPin, LOW);
			}
			*/

			// read the light sensor
			lightSensor_reading = analogRead(light_sensor);
			light_resistance = (float)(1023 - lightSensor_reading) * 10 / lightSensor_reading;
			std::cout << "Light sensor reading is: " << lightSensor_reading << endl;
			std::cout << "Light resistance is: " << light_resistance << endl;

			sound_reading = analogRead(sound);

			
			// reset the timer
			pollTimer = clock();
		}

		// display routine
		DisplayData(viewTimer);
	}
}
