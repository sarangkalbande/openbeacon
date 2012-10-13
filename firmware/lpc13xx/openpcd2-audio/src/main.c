/***************************************************************
 *
 * OpenBeacon.org - main file for OpenPCD2 basic demo
 *
 * Copyright 2010 Milosch Meriac <meriac@openbeacon.de>
 *
 ***************************************************************

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
#include <openbeacon.h>
#include "storage.h"
#include "audio.h"
#include "lcd.h"
#include "font.h"

#define MIFARE_KEY_SIZE 6
const unsigned char mifare_key[MIFARE_KEY_SIZE] =
	{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
unsigned char test_signal = 0;

static void
rfid_hexdump (const void *buffer, int size)
{
	int i;
	const unsigned char *p = (unsigned char *) buffer;

	for (i = 0; i < size; i++)
	{
		if (i && ((i & 3) == 0))
			debug_printf (" ");
		debug_printf (" %02X", *p++);
	}
	debug_printf (" [size=%02i]\n", size);
}

void
CDC_GetCommand (unsigned char *command)
{
	if (!command[1])
		switch (command[0])
		{
			case '+':
				test_signal++;
				break;
			case '-':
				test_signal--;
				break;
			case 'B':
				/* increment U.FL test bus number */
				test_signal = (test_signal & ~0x7) + (1 << 3);
				break;
			case 'b':
				/* decrement U.FL test bus number */
				test_signal = (test_signal & ~0x7) - (1 << 3);
				break;
			case '0':
				test_signal = 0;
				break;
		}
}

static void
loop_rfid (void)
{
	int i, res, old_test_signal = -1, present;
	static unsigned char data[80], bus, signal;
	static unsigned char oid[4];

	/* fully initialized */
	GPIOSetValue (LED1_PORT, LED1_BIT, LED_ON);

	while(1);

	/* read firmware revision */
	debug_printf ("\nreading firmware version...\n");
	data[0] = PN532_CMD_GetFirmwareVersion;
	while ((res = rfid_execute (&data, 1, sizeof (data))) < 0)
	{
		debug_printf ("Reading Firmware Version Error [%i]\n", res);
		pmu_wait_ms (450);
		GPIOSetValue (LED1_PORT, LED1_BIT, LED_ON);
		pmu_wait_ms (10);
		GPIOSetValue (LED1_PORT, LED1_BIT, LED_OFF);
	}

	debug_printf ("PN532 Firmware Version: ");
	if (data[1] != 0x32)
		rfid_hexdump (&data[1], data[0]);
	else
		debug_printf ("v%i.%i\n", data[2], data[3]);

	/* show card response on U.FL */
	test_signal = (25 << 3) | 2;
	/* enable debug output */
	GPIOSetValue (LED1_PORT, LED1_BIT, LED_ON);
	i = present = 0;
	while (1)
	{
		/* wait 1s */
		pmu_wait_ms (10);

		if(present)
			present--;
		else
			if((i++%10) == 0)
				lcd_update_char (CHARACTER_START+((i/10)%26));

		/* detect cards in field */
		data[0] = PN532_CMD_InListPassiveTarget;
		data[1] = 0x01;															/* MaxTg - maximum cards    */
		data[2] = 0x00;															/* BrTy - 106 kbps type A   */
		if (((res = rfid_execute (&data, 3, sizeof (data))) >= 11)
			&& (data[1] == 0x01) && (data[2] == 0x01))
		{
			/* only for Mifare Classic cards */
			if (data[3] == 0 && data[4] == 4 && data[6] >= 4)
			{
				memcpy (oid, &data[7], sizeof (oid));

				data[0] = PN532_CMD_InDataExchange;
				data[1] = 0x01;													/* card 1 */
				data[2] = 0x60;													/* MIFARE authenticate A */
				data[3] = 0x01;													/* block 1 */
				/* MIFARE NFCID1 */
				memcpy (&data[10], oid, sizeof (oid));
				/* MIFARE default key 6*0xFF */
				memcpy (&data[4], mifare_key, MIFARE_KEY_SIZE);

				/* MIFARE Authenticate */
				res = rfid_execute (&data, 14, sizeof (data));

				if (res > 0)
				{
					rfid_hexdump (&data, res);

					data[0] = PN532_CMD_InDataExchange;
					data[1] = 0x01;												/* card 1 */
					data[2] = 0x30;												/* MIFARE read 16 bytes */
					data[3] = 0x01;												/* block 1 */

					/* MIFARE Read */
					res = rfid_execute (&data, 14, sizeof (data));

					debug_printf ("\nMIFARE_READ:");
					if (res == 18)
						rfid_hexdump (&data[2], 16);
					else
						debug_printf (" failed [%i]\n", res);
				}
				else
					debug_printf ("AUTH failed [%i]\n", res);

				debug_printf ("MIFARE_CARD_ID:");
				rfid_hexdump (oid, sizeof (oid));
			}
			else
			{
				debug_printf ("\nCARD_ID:");
				rfid_hexdump (&data[7], data[6]);
			}

			lcd_update_char ('X');
			present=5;

			/* blink LED to indicate card */
			GPIOSetValue (LED1_PORT, LED1_BIT, LED_ON);
			pmu_wait_ms (50);
			GPIOSetValue (LED1_PORT, LED1_BIT, LED_OFF);
		}
		else
		{
			GPIOSetValue (LED1_PORT, LED1_BIT, LED_ON);
			if (res != -8)
				debug_printf ("PN532 error res=%i\n", res);
		}

		/* turning field off */
		data[0] = PN532_CMD_RFConfiguration;
		data[1] = 0x01;															/* CfgItem = 0x01           */
		data[2] = 0x00;															/* RF Field = off           */
		rfid_execute (&data, 3, sizeof (data));

		if (test_signal != old_test_signal)
		{
			old_test_signal = test_signal;

			/* enable test singal output on U.FL sockets */
			signal = test_signal & 0x7;
			bus = (test_signal >> 3) & 0x1F;

			rfid_write_register (0x6328, 0xFC);
			/* select test bus signal */
			rfid_write_register (0x6321, signal);
			/* select test bus type */
			rfid_write_register (0x6322, bus);
			debug_printf ("UPDATED_DEBUG_OUTPUT\n");
		}
		/* display current test signal ID */
		debug_printf ("TEST_SIGNAL_ID: %02i.%i\n", bus, signal);
	}
}

int
main (void)
{
	/* Initialize GPIO (sets up clock) */
	GPIOInit ();

	/* switch PMU to high power mode */
	LPC_IOCON->PIO0_5 = 1 << 8;
	GPIOSetDir (0, 5, 1); //OUT
	GPIOSetValue (0, 5, 0);

	/* Set LED port pin to output */
	GPIOSetDir (LED1_PORT, LED1_BIT, 1);
	GPIOSetValue (LED1_PORT, LED1_BIT, LED_OFF);

	/* Init Power Management Routines */
	pmu_init ();

	/* Init SPI */
	spi_init ();

	/* CDC USB Initialization */
#if 0
	storage_init (0x1234);
#else
	storage_init (0);
	LPC_SYSCON->SYSPLLCTRL = 0x3 | (1<<5);
	SystemCoreClockUpdate ();
#endif

	/* UART setup */
	UARTInit (115200, 0);

	/* Init RFID */
	rfid_init ();

	/* Init LCD Display */
	lcd_init ();

	storage_status ();
//	storage_erase ();

	/* Init Audio */
	audio_init ();

	/* RUN RFID loop */
	loop_rfid ();

	return 0;
}