/*
\file main.c

\brief Main source file.

(c) 2020 Microchip Technology Inc. and its subsidiaries.

Subject to your compliance with these terms, you may use Microchip software and any
derivatives exclusively with Microchip products. It is your responsibility to comply with third party
license terms applicable to your use of third party software (including open source software) that
may accompany Microchip software.

THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
FOR A PARTICULAR PURPOSE.

IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
SOFTWARE.
*/

#include <atmel_start.h>
#include <util/delay.h>
#include <string.h>

/* Select Manchester encoding convention */
#define ENCODING_G_E_THOMAS
#ifndef ENCODING_G_E_THOMAS
#define ENCODING_IEEE
#endif

/*
 * Select baud rate
 *
 * Must be at least 16000 with the 20MHz clock source to support
 * the USART Fractional Baud Rate Generator. Lower baud rates are
 * supported if clock prescalers are enabled.
 */
#define BAUD_RATE 50000UL

/* Transfer data setup */
#define START_BYTE 0x55
#define TRANSMIT_BUFFER_SIZE 255
volatile uint8_t transmit_buffer[TRANSMIT_BUFFER_SIZE] = {START_BYTE};
volatile uint8_t transmit_buffer_length                = 0;
volatile uint8_t sending_in_progress                   = 0;

/* Defining an example packet */
#define TRANSMIT_EXAMPLE_SIZE 18
const uint8_t transmit_example[TRANSMIT_EXAMPLE_SIZE] = "Hello Manchester!";

/*
 * USART0 initialization
 *
 * Pins and interrupts have already been configured by ATMEL Start.
 * Master SPI mode must still be enabled and configured before we
 * can enable the USART. The Fractional Baud Rate Generator must
 * also be configured.
 */
void USART_init(void)
{
	/* Configure Master SPI mode */
	USART0.CTRLC = USART_CMODE_MSPI_gc | USART_UCPHA_bm;

	/* Calculate Baud register value */
	USART0.BAUD = (uint16_t)USART0_BAUD_RATE(BAUD_RATE) << 6;

	/* Enable TRX */
	USART0.CTRLB = USART_TXEN_bm;
}

/*
 * CCL initialization
 *
 * Already configured by ATMEL Start. Only need to account for
 * choice of a different Manchester encoding convention and
 * enable the peripheral and LUT0.
 */
void CCL_init(void)
{
/* Configure LUT truth table for correct encoding convention */
#ifdef ENCODING_G_E_THOMAS
	/* Corresponds to an XNOR gate for CCL inputs A and B */
	CCL.TRUTH0 = 9;
#else
	/* Corresponds to an XOR gate for CCL inputs A and B */
	CCL.TRUTH0 = 6;
#endif

	/* Enable CCL peripheral and LUT0 */
	CCL.LUT0CTRLA |= CCL_ENABLE_bm;
	CCL.CTRLA = CCL_ENABLE_bm;
}

/*
 * Send Manchester encoded data. If data is currently being transmitted
 * the new data will not be sent and the function returns zero.
 */
uint8_t send_encoded_data(const uint8_t *transmit_data, uint8_t num_bytes)
{
	if (!sending_in_progress) {
		for (uint8_t i = 0; i < num_bytes; i++) {
			transmit_buffer[i] = transmit_data[i];
		}
		transmit_buffer_length = num_bytes + 1;
		sending_in_progress    = 1;
		return 1;
	} else {
		return 0;
	}
};

int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	/* Additional initialization */
	USART_init();
	CCL_init();

	/* Main loop */
	while (1) {
		while (!send_encoded_data(transmit_example, TRANSMIT_EXAMPLE_SIZE))
			;

		/* Add a delay after a packet is sent before re-enabling the USART interrupt */
		if (!(USART0.CTRLA & USART_DREIE_bm)) {
			/* Ensure a sufficient timeout period between packets */
			for (uint8_t i = 2048000 / BAUD_RATE; i > 0; i--) {
				_delay_us(250);
			}
			USART0.CTRLA |= USART_DREIE_bm;
		}
	}
}

ISR(USART0_DRE_vect)
{
	static uint8_t transmit_buffer_index = 0;

	if (transmit_buffer_index < transmit_buffer_length) {
		/* Send data */
		USART0_TXDATAL = transmit_buffer[transmit_buffer_index];
		transmit_buffer_index++;
	} else {
		transmit_buffer_index = 0;

		/* Disable USART interrupt after the packet is sent */
		USART0.CTRLA        = (USART0.CTRLA & ~USART_DREIE_bm);
		sending_in_progress = 0;
	}
}
