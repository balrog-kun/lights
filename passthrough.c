/*
 * Passthrough between UART and an nRF24.
 *
 * Licensed under AGPLv3.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

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

#ifndef MAX_PKT_SIZE
# define MAX_PKT_SIZE	32
#endif

#define FIFO_MASK	255
static struct ring_buffer_s {
	uint8_t data[FIFO_MASK + 1];
	uint8_t start, len;
} tx_fifo;

static void handle_input(char ch) {
	tx_fifo.data[(tx_fifo.start + tx_fifo.len ++) & FIFO_MASK] = ch;
}

#define min(a, b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a < _b ? _a : _b; })

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
	static uint8_t tx_cnt; /* Consecutive Tx packets counter */

	/*
	 * Note: all nrf24 calls are serialised in this function so as
	 * to avoid any concurrency issues.
	 */

	if (nrf24_rx_fifo_data()) {
		uint8_t pkt_len, pkt_buf[32], i;
#ifdef SEQN
		static uint8_t seqn = 0xff;
#endif

#ifdef FLASH_TOOL_MODE
		flasher_rx_handle();
#endif

		nrf24_rx_read(pkt_buf, &pkt_len);

#ifdef SEQN
		if (pkt_buf[0] != seqn) {
			seqn = pkt_buf[0];
			for (i = 1; i < pkt_len; i ++)
				serial_write1(pkt_buf[i]);
		}
#else
		for (i = 0; i < pkt_len; i ++)
			serial_write1(pkt_buf[i]);
#endif

		tx_cnt = 0;
	}

	if (tx_fifo.len) { /* .len access should be atomic */
		uint8_t pkt_len, pkt_buf[MAX_PKT_SIZE], split;
#ifdef SEQN
		static uint8_t seqn = 0x00;
		uint8_t count = 128;
#define		MAX_PLD_SIZE	(MAX_PKT_SIZE - 1)

		pkt_buf[0] = seqn ++;
#else
		uint8_t count = 2;
#define		MAX_PLD_SIZE	MAX_PKT_SIZE
#endif

#ifdef FLASH_TOOL_MODE
		flasher_tx_handle();
#endif

		tx_cnt ++;

		cli();
		pkt_len = min(tx_fifo.len, MAX_PLD_SIZE);
		sei();

		/* HACK */
		if (tx_cnt == 2 && MAX_PLD_SIZE > 2 && pkt_len == MAX_PLD_SIZE)
			pkt_len = MAX_PLD_SIZE - 2;
		else if (MAX_PLD_SIZE > 2 && tx_cnt == 3)
			pkt_len = 1;

		split = min(pkt_len,
				(uint16_t) (~tx_fifo.start & FIFO_MASK) + 1);

#define START (MAX_PKT_SIZE - MAX_PLD_SIZE)
		memcpy(pkt_buf + START, tx_fifo.data +
				(tx_fifo.start & FIFO_MASK), split);
		memcpy(pkt_buf + START + split, tx_fifo.data, pkt_len - split);
		/*
		 * Or we could just do pkt_buf = tx_fifo.data + ...;
		 * pkt_len = split;
		 */

		cli();
		tx_fifo.len -= pkt_len;
		tx_fifo.start += pkt_len;
		sei();

		while (-- count) {
			/* Don't flood the remote end with the comms */
			my_delay(4);

			nrf24_tx(pkt_buf, pkt_len + START);
			if (!nrf24_tx_result_wait())
				break;
		}
	}
}

int main(void) {
	setup();

#ifdef FLASH_TOOL_MODE
	flasher_setup();
#endif

	for (;;)
		loop();

	return 0;
}
