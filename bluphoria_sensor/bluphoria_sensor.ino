/* ------------------------------------------------------------------------- //

# Bluphoria Sensor

https://github.com/MILL-LX/bluphoria-sensor
Tiago Rorke - MILL Makers in Little Lisbon
June 2024

Hacking a Daslight DVC FUN DMX interface to allow zone selection ("colour states")
based on temperature readings from an NCIR thermometer.

Two optocouples are used to simulate button presses of the `+` and `-` buttons
on the interface.

Zones are selected based on temperature thresholds at regular intervals,
which can be collectively raised or lowered using the trimpot to adjust
to a given environment.

Uses a number of samples from the NCIR sensor to establish a stable reading,
before setting a colour state based on the temperature thresholds.

-1 is a "standby mode" colour state. After a time period, the arduino returns
the interface to this standby mode.

-2 is a "reset state" that kills the fans for a certain number of seconds to reset the mechanics.
This is a blocking function so the sensor will be unresponsive during this time.

After a certain peroid of time without any interaction, enters demo mode where it
randomly cycles between color states periodically.

The NCIR and trimpot readings are smoothed for added stability.


## Libraries

- Adafruit Library for MLX90614 NCIR temperature sensor - https://github.com/adafruit/Adafruit-MLX90614-Library
- Adafruit DotStar Library - https://github.com/adafruit/Adafruit_DotStar

## Hardware

- Adafruit Trinket M0
- M5 Stack NCIR module
- Trimpot
- 2x 6N139 Optocouples
- 2x 330R resistors


// ------------------------------------------------------------------------- */

#include <Adafruit_MLX90614.h>
#include <Adafruit_DotStar.h>

#define BUTTON_UP    4
#define BUTTON_DOWN  3
#define POT_ADJUST   A0

// RGB LED on Trinket M0
Adafruit_DotStar rgb_led(DOTSTAR_NUM, PIN_DOTSTAR_DATA, PIN_DOTSTAR_CLK, DOTSTAR_BRG);

// NCIR sensor
Adafruit_MLX90614 ncir_sensor = Adafruit_MLX90614();
float temperature;

// DMX control
int dmx_state = -1; // the initial mode of the dmx controller when it is turned on

// simulating button presses with optocouple
const int button_press_duration = 75;
const int button_press_interval = 200;

// colour states
const int num_states = 4;         // number of states excluding standby state (-1)
int state = -1;                   // initial state
int state_change_cooldown = 2000; // minimum time (ms) between changing states
long state_timer = 0;
const long standby_timeout = 480000; // 8 minutes timeout to return to standby state

// demo mode
bool demo_mode = false;
long demo_timer = 0;
const long demo_timein = 1500000;       // go to demo mode after 25 mins in standby mode
const long demo_change_timeout = 30000; // during demo mode, change colors every 30 seconds
const long demo_timeout = 900000;       // return to standby mode after 15 mins of demo mode

// reset fans
int reset_state = -2;
long reset_timer = 0;
const long reset_timeout = 5000; // reset state lasts for 5 seconds

// temperature calibration
const int num_pot_samples = 10;
int pot_adjust[num_pot_samples];
int pot_sample_index;
float temp_midpoint;
const float temp_adj_range = 4.0;         // total range of adjustment for the trimpot
const float temp_midpoint_default = 32.0; // the temperature midpoint when trimpot is at center position
const float temp_range = 8.0;             // total range of expected temperature readings
float temp_thresholds[num_states];        // thresholds for the different colour states

// hand detection
bool hand_detected = false;
bool stable_reading = false;
const int num_samples = 10; // number of samples to use to establish a stable reading
float samples[num_samples];
int sample_index = 0;
const float stable_temp_range = 1.0; // acceptable range in samples to constitute a stable reading


// ------------------------------------------------------------------------- //

void setup() {

   Serial.begin(115200);

   // init RGB LED
   rgb_led.begin();
   rgb_led.setPixelColor(0,0);
   rgb_led.setBrightness(255);
   rgb_led.show();

   // init optocouple control pins
   pinMode(BUTTON_UP, OUTPUT);
   pinMode(BUTTON_DOWN, OUTPUT);

   // init NCIR sensor
   if (!ncir_sensor.begin()) {
     Serial.println("Error connecting to NCIR sensor");
     while (1);
   };

   // synchronise the dmx controller
   delay(2500); // wait for the controller to start up
   updateDMX();

}


// ------------------------------------------------------------------------- //

void loop() {

   // update temperature settings
   pot_adjust[pot_sample_index] = analogRead(POT_ADJUST);
   pot_sample_index++;
   pot_sample_index %= num_pot_samples;
   int p = 0;
   for (int i=0; i<num_pot_samples; i++) {
      p += pot_adjust[i];
   }
   p /= num_pot_samples;
   temp_midpoint = mapfloat(
      p,
      1023, // in_min (inverted polarity)
      0,    // in_max
      temp_midpoint_default - (temp_range/2.0), // out_min
      temp_midpoint_default + (temp_range/2.0)  // out_max
      );
   updateThresholds();

   // update temperature readings
   samples[sample_index] = ncir_sensor.readObjectTempC();
   sample_index++;
   sample_index %= num_samples;
   // calculate average
   temperature = 0;
   for (int i=0; i<num_samples; i++) {
      temperature += samples[i];
   }
   temperature /= num_samples;

   // detect hand and check for stable reading
   if (temperature > temp_thresholds[0]) {
      hand_detected = true;
      if(demo_mode) { // leave demo mode as soon as a hand is detected
         exitDemo();
      }
   } else {
      hand_detected = false;
   }
   float sample_min = 999;
   float sample_max = 0;
   for (int i = 0; i < num_samples; i++) {
      sample_min = samples[i] < sample_min ? samples[i] : sample_min;
      sample_max = samples[i] > sample_max ? samples[i] : sample_max;
   }
   if (sample_max - sample_min <= stable_temp_range) {
      stable_reading = true;
   } else {
      stable_reading = false;
   }
   updateLED();

   // if not in demo mode
   if(!demo_mode) {

      // set colour state based on temperature reading
      if (hand_detected && stable_reading) {
         if (millis() - state_timer > state_change_cooldown) {
            for (int i=0; i<num_states; i++) {
               if (temperature > temp_thresholds[i]) {
                  state = i;
               }
            }
            state_timer = millis();
            updateLED();
            resetFans(state);
            updateDMX();
         }
      }

      // if not in standby, return to standby state if passed standby timeout duration
      if (state >= 0 && millis() - state_timer > standby_timeout) {
         state = -1;
         state_timer = millis();
         updateLED();
         updateDMX();

      // if in standby mode, check timer for entering demo mode
      } else if (state == -1 && millis() - state_timer > demo_timein) {
         enterDemo();
      }

      // print state, thresholds and readings for debugging
      for (int i=0; i < num_states; i++) {
         Serial.print(temp_thresholds[i]);
         Serial.print('\t');
      }
      Serial.print(temp_thresholds[0] + (float(state) + 0.5) * temp_range/float(num_states) );
      Serial.print('\t');
      Serial.print(temperature);
      Serial.println();

   } else {

      // demo mode
      if (millis() - demo_timer > demo_timeout) {
         exitDemo();
      } else if (millis() - state_timer > demo_change_timeout) {
         int pstate = state;
         while(state == pstate) {
            state = random(0,num_states);
         }
         Serial.print("demoing color state: ");
         Serial.println(state);
         state_timer = millis();
         resetFans(state);
         updateDMX();
      }
   }

   // regulate sample rate
   delay(100);

}


// ------------------------------------------------------------------------- //

// Enter demo mode
void enterDemo() {
   Serial.println("entering demo mode.");
   demo_mode = true;
   demo_timer = millis();
   state_timer = millis() - demo_change_timeout;
   // randomise color state selections
   randomSeed(analogRead(millis()));
}


// Exit demo mode
void exitDemo() {
   Serial.println("leaving demo mode.");
   demo_mode = false;
   state = -1;
   updateDMX();
}


// Reset state
void resetFans(int new_state) {
   Serial.println("entering reset state...");
   state = reset_state;
   updateDMX();
   reset_timer = millis();
   while(millis() - reset_timer < reset_timeout) {
      delay(10);
   }
   Serial.println("leaving reset state.");
   state_timer = millis();
   state = new_state;
}


// Calculate state temperature thresholds
void updateThresholds() {
   temp_thresholds[0] = temp_midpoint - temp_range/2.0;
   for (int i=1; i < num_states; i++) {
      temp_thresholds[i] = temp_thresholds[0] + i*temp_range/float(num_states);
   }
}


// synchronise the mode of DMX controller with the current colour state
void updateDMX() {
   while(dmx_state != state) {
      if (dmx_state > state) {
         modeDown();
      } else {
         modeUp();
      }
   }
}


// update the Trinket RGB LED
void updateLED() {
   if (!hand_detected) {
      // turn off LED if no hand is detected
      rgb_led.setPixelColor(0,0);
   } else if (hand_detected && !stable_reading) {
      // set the LED green if a hand is detected
      rgb_led.setPixelColor(0,255,0,0);
   } else {
      // if there is a hand and a new stable reading
      // set the colour based on the colour state
      float s = float(state) / float(num_states-1);
      int red = s*255;
      int blue = (1-s)*255;
      rgb_led.setPixelColor(0,0,red,blue);
   }
   rgb_led.show();
}


void modeUp() {
   digitalWrite(BUTTON_UP, HIGH);
   delay(button_press_duration);
   digitalWrite(BUTTON_UP, LOW);
   delay(button_press_interval);
   dmx_state++;
}


void modeDown() {
   digitalWrite(BUTTON_DOWN, HIGH);
   delay(button_press_duration);
   digitalWrite(BUTTON_DOWN, LOW);
   delay(button_press_interval);
   dmx_state--;
}


float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
   return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
