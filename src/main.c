/*
 * This file is part of the MSP430 USCI I2C slave example.
 *
 * Copyright (C) 2012 Stefan Wendler <sw@kaltpost.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * This firmware acts as an I2C slave on address 0x90(W)/0x91(R).
 * It receives a command / parameter pair from the I2C master and
 * sends out a response on a masters read request.
 *
 * The firmware knows two commands:
 *
 * CMD_SETLED: sets the build in LED (P1.0) of the launchpad depending
 * on the given parameter to HIGH (0x01) or LOW (0x00).
 *
 * CMD_GETKEY: copies the state of the build in push-button (P1.3) to
 * the response buffer and transmits it to the master on the next read
 * request.
 *
 * As a I2C master, the bus pirate could be used. To set the BP into
 * I2C mode use:
 *
 *    m4 3
 *
 * Bus-Piret I2C commands to
 *
 * - set LED HIGH (P1.0)
 *    [0x90 0x00 0x01
 *
 * - set LED LOW  (P1.0)
 *    [0x90 0x00 0x00
 *
 * - get BUTTON state (P1.3)
 *    [0x90 0x01 [0x91 r
 *
 * For a quick start to setup the I2C slave, the TI USCI I2C slave
 * code from slaa383 was used and slightly modified to work on
 * the MSP430G2553 and the MSP-GCC. For detail on leagal issues
 * regarding the TI_USCI_I2C_slave code see "ti-usci-i2c-slave-legal.txt".
 *
 * NOTE: 100k extrnal pull-ups are needed on SDA/SDC.
 */

#include <msp430.h>
#include <legacymsp430.h>

#include "TI_USCI_I2C_slave.h"

/* callback for start condition */
void start_cb();

/* callback for bytes received */
void receive_cb(unsigned char receive);

/* callback to transmit bytes */
void transmit_cb(unsigned char volatile *receive);

/* Commands */
#define CMD_SETLED  0x00
#define CMD_GETKEY  0x01
#define CMD_UNKNOWN 0xFF

/* Parameters */
#define PAR_UNKNOWN 0xFF

/* last command */
unsigned char cmd = CMD_UNKNOWN;

/* last parameter */
unsigned char par = PAR_UNKNOWN;

volatile int c, key, led;

void process_cmd(unsigned char cmd, unsigned char par)
{

    switch(cmd) {
    case CMD_SETLED:
        led = 0x80 | par;
        break;
    case CMD_GETKEY:
        c = 0;
        break;
    }
}

void start_cb()
{
    cmd = CMD_UNKNOWN;
    par = PAR_UNKNOWN;
}

void receive_cb(unsigned char receive)
{
    if(cmd == CMD_UNKNOWN) {

        cmd = receive;

        if(cmd == CMD_GETKEY) {
            process_cmd(cmd, PAR_UNKNOWN);
        }
    } else {
        par = receive;
        process_cmd(cmd, par);
    }
}

void transmit_cb(unsigned char volatile *byte)
{
    if (c ==0) {
        *byte = key & 0xff;
        ++c;
    } else {
        *byte = key >> 8;
        key = 0;
    }
}

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD;                      // Stop WDT
    P1DIR |= BIT0;                                 // Set P1.0 to output direction
    P1OUT &= ~BIT0;
	
    TI_USCI_I2C_slaveinit(start_cb, transmit_cb, receive_cb, 0x4a);
    BCSCTL1 = CALBC1_16MHZ;
    DCOCTL  = CALDCO_16MHZ;

    __delay_cycles(16000 * 500);    // 500ms wait

    __bis_SR_register(GIE);

    init_ps2_kbd();

    led = 0;

//    while(1) __asm__("nop");
    while(1) {
        if(key == 0) {
	    key = get_scancode();
        }
        if (led & 0x80) {
            set_kbd_leds(led & 0xf);
            led = 0;
        }
    }

    return 0;
}

