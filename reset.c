#include <avr/io.h>
#include <avr/interrupt.h>

/* Assuming BOOTSZ == 00 in the fuses (default) */
#if defined(__AVR_ATmega168__)
#define BOOTLOADER_AREA 0x1d00
#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32__)
#define BOOTLOADER_AREA 0x3800
#endif

int main(void) {
	MCUSR = 0;
	cli();
	((void (*)(void)) BOOTLOADER_AREA)();
	return 0;
}
