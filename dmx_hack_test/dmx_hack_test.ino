
#include <Wire.h>

// M5 Stack NCIR Sensor
// https://shop.m5stack.com/products/ncir-sensor-unit
// https://docs.m5stack.com/zh_CN/unit/ncir
// https://github.com/m5stack/M5Stack/blob/master/examples/Unit/NCIR_MLX90614/NCIR_MLX90614.ino

static uint16_t ncir_result;
static float temperature;


void setup() {
   pinMode(3, OUTPUT);
   pinMode(4, OUTPUT);

   Serial.begin(115200);
}

void loop() {

   // read and sensor and calculate temperature
   Wire.beginTransmission(0x5A); // Send Initial Signal and I2C Bus Address
   Wire.write(0x07);             // Send data only once and add one address automatically.
   Wire.endTransmission(false);  // Stop signal
   Wire.requestFrom(0x5A, 2);    // Get 2 consecutive data from 0x5A, and the data is stored
   ncir_result = Wire.read();
   ncir_result |= Wire.read() << 8;
   temperature = ncir_result * 0.02 - 273.15;
   Serial.print(millis());
   Serial.print('\t');
   Serial.print(ncir_result);
   Serial.print('\t');
   Serial.println(temperature);

   delay(500);

   /*
   digitalWrite(3, HIGH);
   delay(1000);
   digitalWrite(3, LOW);
   delay(1000);
   digitalWrite(4, HIGH);
   delay(1000);
   digitalWrite(4, LOW);
   delay(1000);
   */
}
