#include <USBSerial.h>
#define LED_BUILTIN PB9
void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
	SerialUSB.begin();
}

void loop()
{
	for (uint8_t i = 0; i < 2; i++) {
		digitalWrite(LED_BUILTIN, LOW);
		delay(50);
		digitalWrite(LED_BUILTIN, HIGH);
		delay(200);
	}
	delay(750);
}
