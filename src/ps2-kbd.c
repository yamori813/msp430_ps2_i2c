/*
 * ps2-kbd.c
 *
 *  Created on: 2013-07-24
 *      Author: Jacques Deschenes
 *      DESCRIPTION: interface clavier PS/2
 *  Port change: 2025-04-05 Hiroki Mori
 *					 P2.6  signal clock
 *					 P2.7  signal data
 *
 *------------------------------------------------
 *  References:
 *  	http://retired.beyondlogic.org/keyboard/keybrd.htm#1
 *  	http://www.computer-engineering.org/
 *  	http://en.wikipedia.org/wiki/PS/2_keyboard
 *  	http://wiki.osdev.org/PS2_Keyboard
 *------------------------------------------------
 */
#include <msp430.h>
#include "ps2-kbd.h"

// utilisation du port 1 pour
// l'interface clavier PS/2
#define KBD_CLK BIT6 // signal clock
#define KBD_DAT BIT7 // signal data



volatile unsigned char kbd_queue[32]; // file circulaire pour les codes recus du clavier.
volatile unsigned char head, tail; // tete et queue de la file
volatile unsigned char  in_byte, bit_cnt, parity, rx_flags, kbd_leds;

void wait_idle(){// attend msec d'inactivite, utilisation du WDT comme minuterie
				 // crystal 32Khz installe
	WDTCTL = WDT_ADLY_16; // delais 16 msec avec crystal 32Khz
	IFG1 &= ~WDTIFG; // RAZ indicateur interruption
	while (!(IFG1 & WDTIFG)) {
		if (!((P2IN & (KBD_CLK+KBD_DAT))==(KBD_CLK+KBD_DAT))){
			IFG1 &= ~WDTIFG;
			WDTCTL = WDT_ADLY_16;
		}
	}
	WDTCTL = WDTPW+WDTHOLD;
} // wait_idle()

int init_ps2_kbd(){ // initialisation du clavier
	volatile unsigned int c;
	P2DIR &= ~(KBD_CLK|KBD_DAT); // les 2 en entree.
	P2SEL &= ~(KBD_CLK|KBD_DAT);  // pas de peripherique
	P2SEL2 &= ~(KBD_CLK|KBD_DAT); // sur ces 2 E/S
	P2REN &= ~(KBD_CLK|KBD_DAT); // pas de pullup.
	P2IES |= KBD_CLK; // int. sur transition descendante.
	bit_cnt=0;
	head=0;
	tail=0;
	rx_flags=0;
//	wait_idle();
	P2IFG &= KBD_CLK;
	P2IE |= KBD_CLK;  // interruption sur le signal clock
	if (!kbd_send(KBD_RESET)){
		return 0;
	}
	while ((rx_flags & F_ERROR+F_RCVD)==0); // attend resultat BAT
	if (rx_flags & F_ERROR)
		return 0;
	c=get_scancode();
	if (c!=BAT_OK)
		return 0;
	return 1;
}// init_ps2_kbd()

inline void enable_keyboard_rx(){ // active l'interruption
	bit_cnt=0;
	P2IFG  &= KBD_CLK;
	P2IE |= KBD_CLK;
} // enable_keyboard()

inline void disable_keyboard_rx(){ // desactive l'interruption
	P2IE &= ~KBD_CLK;
}// disable_keyboard()


int kbd_send(char cmd){ /* envoie d'un caractere de commande au clavier */
	bit_cnt=0;
	parity=0;
	P2IE &= ~KBD_CLK; // desactive les interruptions sur KBD_CLK
	P2OUT &= ~KBD_CLK; // MCU prend le controle de la ligne KBD_CLK
	P2DIR |= KBD_CLK;   	//  mis a 0  KBD_CLK
	__delay_cycles(MCLK_FRQ * 150); 	// delais minimum 100usec marge 50usec
	P2OUT &= ~KBD_DAT;		// prend le controle de la ligne KBD_DAT
	P2DIR |= KBD_DAT;   	// met KBD_DAT a zero
	P2DIR &= ~(KBD_CLK); 	// libere la ligne clock
	while (!(P2IN & KBD_CLK)); // attend que le clavier mette la ligne clock a 1
	while (bit_cnt<8){      // envoie des 8 bits, moins significatif en premier.
		while (P2IN & KBD_CLK);   // attend clock a 0
		if (cmd&1){
			P2OUT |= KBD_DAT;
			parity++;
		}else{
			P2OUT &= ~KBD_DAT;
		}
		cmd >>= 1;
		while (!(P2IN & KBD_CLK)); // attend clock a 1
		bit_cnt++;				  // un bit de plus envoye.
	}
	while (P2IN & KBD_CLK);   // attend clock a 0
	if (!(parity & 1)){
		P2OUT |= KBD_DAT;
	}else{
		P2OUT &= ~KBD_DAT;
	}
	while (!(P2IN & KBD_CLK)); // attend clock a 1
	while (P2IN & KBD_CLK);   // attend clock a 0
	P2DIR &= ~KBD_DAT;  		// libere la ligne data
	while (!(P2IN & KBD_CLK)); // attend clock a 1
	while (P2IN & (KBD_DAT+KBD_CLK)); 	// attend que le clavier mette data et clock a 0
	while (!((P2IN & (KBD_DAT+KBD_CLK))==(KBD_DAT+KBD_CLK))); // attend que les 2 lignes reviennent a 1.
	bit_cnt=0;
	P2IFG &= ~KBD_CLK;
	P2IE |= KBD_CLK;
	while ((rx_flags & F_ERROR+F_RCVD)==0); // attend keyboard ACK
	if ((rx_flags & F_ERROR) || (get_scancode()!=KBD_ACK)){
		return 0;
	}else{
		return 1;
	}
}// kbd_send()

#define COMPLETED 1
#define RELEASE 2
#define EXTENDED 4
#define PRN_KEY 8
#define PAUSE_KEY 16
int get_scancode(){ // entier positif si touche enfoncee, entier negatif si touche relachee
	unsigned int i, flags;
	int code;
	code = 0;
	flags=0;
	while (!(flags & COMPLETED)){
		if (head!=tail){
			code = kbd_queue[head];
			head++;
			head &= 31;
			if (code==XTD_KEY){
				flags |= EXTENDED;
			}else if (code==KEY_REL){
				flags |= RELEASE;
			}else if (code==0xE1){ // PAUSE
				for (i=7;i;i--){     // elimine les 7 prochains caracteres
					while (head==tail);
					head++;
					head &= 31;
				}
				flags = COMPLETED+PAUSE_KEY;
			}else if ((flags&EXTENDED)&& (code==0x12)){ // touche PRINT SCREEN enfoncee
				for (i=2;i;i--){ // elimine les 2 codes suivants
					while (head==tail);
					head++;
				}
				flags = COMPLETED+PRN_KEY;
			}else if ((flags&EXTENDED)&& (code==0x7c)){ // touche PRINT SCREEN relachee
				for (i=4;i;i--){ // elimine les 4 codes suivants
					while (head==tail);
					head++;
				}
				flags = COMPLETED+PRN_KEY+RELEASE;
			}else{
				flags |=COMPLETED;
			}
			if (!(flags & COMPLETED)){
				while (head==tail); // attend touche suivante
			}
		}else{
			break;
		}
	}
	if (flags & PAUSE_KEY){
		code = PAUSE;
	}else if (flags & PRN_KEY){
		code = PRN;
	}
	if (flags & RELEASE){
		code |= REL_BIT; // negatif pour touche relachee
	}
	if (flags & EXTENDED){
		code |= XT_BIT; //
	}
	P2IE &= KBD_CLK;// section critique
	if (head==tail){
		rx_flags &= ~F_RCVD;
	}
	P2IE |= KBD_CLK; // fin section critique
	return code;
}// get_scancode()

extern t_scan2key translate[],alt_char[],xt_char[],shifted_key[];

int get_key(int scancode){
	int a,i;
	a=0;
	if (scancode & XT_BIT){
		i=0;
		while (xt_char[i].code){
			if (xt_char[i].code==scancode){
				a=xt_char[i].ascii;
				break;
			}
			i++;
		} // while (xt_char[i].code)
	}else if (rx_flags & F_SHIFT|F_CAPS){
		i=0;
		while (shifted_key[i].code){
			if (shifted_key[i].code==(scancode&0xff)){
				a=shifted_key[i].ascii;
				break;
			}
			i++;
		}// while (shifted_key.code)
		if (!a){
			i=0;
			while (translate[i].code){
				if (translate[i].code==(scancode&0xff)){
					a=translate[i].ascii;
					break;
				}
				i++;
			}// while (translate.code)
			if (a>='a' && a<='z'){
				a -=32;
			}
		} // if (!a)
	}else{
		i=0;
		while (translate[i].code){
			if (translate[i].code==(scancode&0xff)){
				a=translate[i].ascii;
				break;
			}
			i++;
		}// while (translate.code)
		if (a>='a' && a<='z'){
			a -=32;
		}
	}
	return a|(scancode&0xff00);
}// get_key()

int set_kbd_leds(unsigned int leds_state){
	if (!kbd_send(KBD_LED)){
		return 0;
	}
	if (!kbd_send(leds_state)){
		return 0;
	}
	return 1;
} // set_kbd_leds()

int debug = 0;

#pragma vector=PORT2_VECTOR
__interrupt void kbd_clk_isr(void){
	if (!(P2IFG & KBD_CLK)) return; // pas la bonne interruption
	P2IFG &= ~KBD_CLK;
	switch (bit_cnt){
	case 0:   // start bit
		if (P2IN & KBD_DAT)
			return; // ce n'est pas un start bit
		parity=0;
		bit_cnt++;
		break;
	case 9:   // paritee
		if (P2IN & KBD_DAT)
			parity++;
		if (!(parity & 1)){
			rx_flags |= F_ERROR;
			P2IE &= KBD_CLK; //desactivation interruptions
		}
		bit_cnt++;
		break;
	case 10:  // stop bit
		kbd_queue[tail]=in_byte;
		tail++;
		tail &=31;
		bit_cnt=0;
		rx_flags |= F_RCVD;
		if (debug) 
			P1OUT |= BIT0;
		else
			P1OUT &= ~BIT0;
		debug = debug ? 0 : 1;
		break;
	default:
		in_byte >>=1;
		if(P2IN & KBD_DAT){
			in_byte |=128;
			parity++;
		}
		bit_cnt++;
	}
} // kbd_clk_isr()
