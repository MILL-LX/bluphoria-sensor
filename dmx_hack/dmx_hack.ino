
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
int dmx_state = 5; // the initial mode of the dmx controller when it is turned on

// current DMX mode colours
// 5 RED
// 4 YELLOW
// 3 PINK
// 2 PURPLE
// 1 GREEN
// 0 BLUE

// simulating button presses with optocouple
const int button_press_duration = 75;
const int button_press_interval = 200;

// colour states
const int num_states = 6;
int state = 3;                    // initial state
int state_change_cooldown = 2000; // minimum time (ms) between changing states
long state_timer = 0;

// temperature calibration
int pot_adjust;
float temp_midpoint;
const float temp_adj_range = 3.0;         // total range of adjustment for the trimpot
const float temp_midpoint_default = 32.0; // the temperature midpoint when trimpot is at center position
const float temp_range = 10.0;            // total range of expected temperature readings
float temp_thresholds[num_states];   // thresholds for the different colour states

// hand detection
bool hand_detected = false;
bool stable_reading = false;
const int num_samples = 10;          // number of samples to use to establish a stable reading
float samples[num_samples];
int sample_index = 0;
const float stable_temp_range = 1.0; // acceptable range in samples to constitute a stable reading


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


void loop() {

   // update temperature settings
   pot_adjust = analogRead(POT_ADJUST);
   temp_midpoint = mapfloat(
      pot_adjust,
      0,                                        // in_min
      1023,                                     // in_max
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
         updateDMX();
      }
   }

   // print thresholds and readings for debugging
   for (int i=0; i < num_states; i++) {
      Serial.print(temp_thresholds[i]);
      Serial.print('\t');
   }
   Serial.print(temperature);
   Serial.println();


   // regulate sample rate
   delay(100);

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