
#include <Adafruit_MLX90614.h>

#define BUTTON_UP    3
#define BUTTON_DOWN  4
#define POT_ADJUST   A0

// NCIR sensor
Adafruit_MLX90614 ncir_sensor = Adafruit_MLX90614();
float temperature;

// simulating button presses with optocouple
const int button_press_duration = 100;
const int button_press_interval = 300;

// temperature calibration
int pot_adjust;
float temp_midpoint;
const float temp_adj_range = 3.0;         // total range of adjustment for the trimpot
const float temp_midpoint_default = 32.0; // the temperature midpoint when trimpot is at center position
const float temp_range = 5.0;             // total range of expected temperature readings

// hand detection trigger ?
// adapt from i-miss-you ?

// colour states
const int num_states = 5;
int state = 3;



void setup() {

   Serial.begin(115200);

   // init optocouple control pins
   pinMode(BUTTON_UP, OUTPUT);
   pinMode(BUTTON_DOWN, OUTPUT);

   // init NCIR sensor
   if (!ncir_sensor.begin()) {
     Serial.println("Error connecting to NCIR sensor");
     while (1);
   };
}


void loop() {

   // update temperature reading
   temperature = ncir_sensor.readObjectTempC();

   // update temperature settings
   pot_adjust = analogRead(POT_ADJUST);
   temp_midpoint = mapfloat(
      pot_adjust,
      0,                                        // in_min
      1023,                                     // in_max
      temp_midpoint_default - (temp_range/2.0), // out_min
      temp_midpoint_default + (temp_range/2.0)  // out_max
      );

   Serial.print(temp_midpoint);
   Serial.print('\t');
   Serial.print(temperature);
   Serial.println();
   delay(100);

   // if hand detected
   // set colour state based on temperature reading

}


void modeUp(int n) {
   for (int i=0; i<n; i++) {
      digitalWrite(BUTTON_UP, HIGH);
      delay(button_press_duration);
      digitalWrite(BUTTON_UP, LOW);
      delay(button_press_interval);
   }
}


void modeDown(int n) {
   for (int i=0; i<n; i++) {
      digitalWrite(BUTTON_DOWN, HIGH);
      delay(button_press_duration);
      digitalWrite(BUTTON_DOWN, LOW);
      delay(button_press_interval);
   }
}


float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
   return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}