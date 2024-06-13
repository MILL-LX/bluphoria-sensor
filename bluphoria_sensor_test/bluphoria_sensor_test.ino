
#include <Adafruit_MLX90614.h>

Adafruit_MLX90614 ncir_sensor = Adafruit_MLX90614();

void setup() {
   pinMode(3, OUTPUT);
   pinMode(4, OUTPUT);

   Serial.begin(115200);

   if (!ncir_sensor.begin()) {
     Serial.println("Error connecting to NCIR sensor");
     while (1);
   };
}

void loop() {

   // read and sensor and calculate temperature
   //Serial.println(ncir_sensor.readObjectTempC());
   Serial.println(analogRead(A0));
   delay(100);

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
