#define FLASH_TOOL_MODE
#define MAX_PKT_SIZE 32

static void flasher_setup(void);
static void flasher_rx_handle(void);
static void flasher_tx_handle(void);

#include "passthrough.c"

static void flasher_setup(void) {
}

static uint32_t prev_txrx_ts = 0;

static void flasher_rx_handle(void) {
	prev_txrx_ts = timer_read();
}

static void flasher_tx_handle(void) {
	static uint8_t first_tx = 1;

	/*
	 * If more than a second has passed since previous communication
	 * the bootloader will have left the flash mode by now so this is
	 * probably a new boot and a new flashing attempt.
	 */
	if ((uint32_t) (timer_read() - prev_txrx_ts) > F_CPU)
		first_tx = 1;

	/*
	 * Before any actual STK500v2 communication begins we need to
	 * attempt to reset the board to start the bootloader, and send it
	 * our radio address to return the ACK packets to.
	 */
	if (first_tx) {
		uint8_t ch, pkt[4];

		/*
		 * Our protocol requires any program running on the board
		 * to reset it if it receives a 0xff byte.
		 */
		ch = 0xff;
		nrf24_tx(&ch, 1);
		nrf24_tx_result_wait();

		/* Give the board time to reboot and enter the bootloader */
		my_delay(100);

		/* Finally send our address + pkt size as a separate packet */
		pkt[0] = eeprom_read(0);
		pkt[1] = eeprom_read(1);
		pkt[2] = eeprom_read(2);
		pkt[3] = MAX_PKT_SIZE;
		/* TODO: set the no-ACK bit, the remote board can't ACK yet */
		nrf24_tx(pkt, 4);
		nrf24_tx_result_wait();

		first_tx = 0;
	}

	prev_txrx_ts = timer_read();

	/* Don't flood the remote bootloader with our comms */
	my_delay(4);
}
