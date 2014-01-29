/*
 * Passthrough between UART and an nRF24.
 *
 * Licensed under AGPLv3.
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer1.h"
#include "uart.h"

#define CE_DDR		DDRC
#define CE_PORT		PORTC
#define CSN_DDR		DDRC
#define CSN_PORT	PORTC
#define CE_PIN		(1 << 1)
#define CSN_PIN		(1 << 0)

#include "spi.h"
#include "nrf24.h"

static void handle_input(char ch) {
	nrf24_idle_mode();

	nrf24_tx((uint8_t *) &ch, 1);
	sei(); /* TODO: be careful with new serial interrupts */
	nrf24_tx_result_wait();

	nrf24_rx_mode();
}

static uint8_t eeprom_read(uint16_t addr) {
	while (EECR & (1 << EEPE));

	EEAR = addr;
	EECR |= 1 << EERE;	/* Start eeprom read by writing EERE */

	return EEDR;
}

void setup(void) {
	uint8_t s = SREG;
	uint8_t m = MCUCR;
	uint8_t i, addrs[6];

	serial_init();
	timer_init();
	spi_init();
	nrf24_init();
	sei();

	/*
	 * Set our radio address and the remote end's radio address, read
	 * the addresses from EEPROM where they need to be saved first.
	 */
	for (i = 0; i < 6; i ++)
		addrs[i] = eeprom_read(i);

	nrf24_set_rx_addr(addrs + 0);
	nrf24_set_tx_addr(addrs + 3);

	/* Write something to say hello */
	serial_write_str("SREG:");
	serial_write_hex16(s);
	serial_write_str(", MCUCR:");
	serial_write_hex16(m);
	serial_write_str(", our addr: ");
	serial_write1(addrs[0]);
	serial_write1(addrs[1]);
	serial_write1(addrs[2]);
	serial_write_eol();

	nrf24_rx_mode();

	serial_set_handler(handle_input);
}

void loop(void) {
	cli(); /* Be careful with serial interrupts, don't want to a Tx now */
	if (nrf24_rx_fifo_data()) {
		uint8_t pkt_len, pkt_buf[33];

		nrf24_rx_read(pkt_buf, &pkt_len);
		sei();

		pkt_buf[pkt_len] = '\0';
		serial_write_str((char *) pkt_buf);
	} else
		sei();
}

int main(void) {
	setup();

	for (;;)
		loop();

	return 0;
}
