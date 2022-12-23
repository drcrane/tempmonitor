#include <msp430.h>
#include <stdint.h>
#include "utility.h"

/**
 * Temperature Monitor and Controller
 * Monitor Temperature on two LMT86 Temperature Sensors
 * If the difference between the two sensors is greater than a set value
 * Turn on the output
 * Once the difference is below a set value turn off the output
 * Display the two different values on the attached OLED display
 *
 *         +----------+
 *    VCC -|          |- VSS
 *        -|          |- XIN  SWRTC
 *    RXD -|          |- XOUT SWRTC
 *    TXD -|          |- TEST
 *     A3 -|          |- /RST
 *     A4 -|          |- SDA
 *        -|          |- SCL
 * A3_CTL -|          |- DISP_PWR
 * A4_CTL -|          |- SW_0
 *   OP_0 -|          |- SW_1
 *         +----------+
 *
 * A3 & A4         = LMT86 OUT (PIN2)
 * A3_CTL & A4_CTL = LMT86 VDD (PIN1)
 * OP_0            = OUTPUT SWITCH
 * SDA & SCL       = OLED DISPLAY INTERFACE
 * DISP_PWR        = OLED DISPLAY POWER
 * SW_0 & SW_1     = MANUAL INPUT SWITCHES
 */

volatile uint16_t adc_val[2];

int16_t temp_local, temp_remote, temp_diff, fan_state, fan_state_ctr;

static volatile int recv_byte;

void hwuart_sendb(char b) {
	while(!(IFG2 & UCA0TXIFG));
	UCA0TXBUF = b;
}

void hwuart_sendstr(char * ptr) {
	while (*ptr != 0) {
		while(!(IFG2 & UCA0TXIFG));
		UCA0TXBUF = *ptr++;
	}
}

static volatile uint16_t tick;

static volatile uint16_t input_clr;
static volatile uint16_t input_set;

static volatile uint16_t reading_complete;

uint16_t adc_to_millivolts(uint16_t adc_value);
int16_t adc_to_decidegc(uint16_t adc_value);

void transmitvalue() {
	char buf[8];
	Utility_intToAPadded(buf, temp_local, 10, 1);
	hwuart_sendstr(buf);
	hwuart_sendb(' ');
	Utility_intToAPadded(buf, temp_remote, 10, 1);
	hwuart_sendstr(buf);
	hwuart_sendb(' ');
	Utility_intToAPadded(buf, temp_diff, 10, 1);
	hwuart_sendstr(buf);
	if (fan_state) {
		hwuart_sendstr(" ON ");
	} else {
		hwuart_sendstr(" OFF ");
	}
	Utility_intToAPadded(buf, fan_state_ctr, 10, 1);
	hwuart_sendstr(buf);
	hwuart_sendb('\r');
	hwuart_sendb('\n');
}

/*
 * Empirical Measurements
 * 1.880 = 760
 * 1.900 = 768
 */

uint16_t adc_to_millivolts(uint16_t adc_value) {
	uint16_t millivolts;
	// Reference Voltage 2500mV (ADC = 10bit)
	uint32_t tmp = adc_value;
	tmp = tmp * 24438;
	tmp = tmp / 10000;
	millivolts = tmp & 0xffff;
	return millivolts;
}

/*
 * m = 0.226277
 * c = 194.537
 */
int16_t adc_to_decidegc(uint16_t adc_value) {
	int16_t decidegc;
	int32_t tmp = adc_value;
	tmp = tmp * -2263;
	tmp = tmp + 1945370;
	tmp = tmp / 1000;
	decidegc = tmp;
	return decidegc;
}

int main(void) {
	WDTCTL = WDTPW | WDTHOLD;

	// Configure device to operate from internal clock at 1MHz
	BCSCTL1 = CALBC1_1MHZ;
	DCOCTL = CALDCO_1MHZ;
	
	// Configure UART 9600 @ 1MHz
	P1SEL |= BIT1 | BIT2 ; // P1.1 = RXD, P1.2=TXD
	P1SEL2 |= BIT1 | BIT2 ; // P1.1 = RXD, P1.2=TXD
	UCA0CTL1 |= UCSWRST;
	UCA0CTL1 |= UCSSEL_2; // Use SMCLK
	UCA0BR0 = 104; // Set baud rate to 9600 with 1MHz clock (Data Sheet 15.3.13)
	UCA0BR1 = 0; // Set baud rate to 9600 with 1MHz clock
	UCA0MCTL = UCBRS_1; // Modulation UCBRSx = 1
	UCA0CTL1 &= ~UCSWRST; // Initialize USCI state machine
	IE2 |= UCA0RXIE; // Enable USCI_A0 RX interrupt

	P2DIR |= BIT0 | BIT1 | BIT2;
	P2OUT |= BIT0 | BIT1;

	// Initialise ADC10 (ADC 10 bit) CHANNEL 3
	//ADC10CTL0 = SREF_1 | ADC10SHT_3 | ADC10SR | REF2_5V | REFON | ADC10ON | ADC10IE;
	//ADC10CTL1 = ADC10DIV_3 | INCH_3 | CONSEQ_2;
	//ADC10AE0 = BIT3;

	// Initialise ADC10 (ADC 10 bit) CHANNEL 3 AND 4
	ADC10CTL0 = SREF_1 | ADC10SHT_3 | ADC10SR | MSC | REF2_5V | REFON | ADC10ON | ADC10IE;
	ADC10CTL1 = ADC10DIV_3 | INCH_4 | CONSEQ_1;
	ADC10DTC1 = 0x02;
	ADC10AE0 = BIT3 | BIT4;

	// Delay to allow REF to settle
	__bis_SR_register(GIE);
	/*
	TA0CCR0 = 30;
	TA0CCTL0 |= CCIE;
	TA0CTL = TASSEL_2 | MC_1;
	__bis_SR_register(CPUOFF);
	TA0CCTL0 &= ~CCIE;
	*/

	ADC10CTL0 |= ENC;
	TA0CCR0 = 0xffff;
	TA0CTL = TASSEL_2 | MC_2;
	TA0CCTL0 |= CCIE;

	// Configure TA1 for Software Real Time Clock
	P2SEL |= (BIT6 | BIT7);
	P2DIR |= BIT7;
	BCSCTL3 |= XCAP_3;

	TA1CTL = TASSEL_1 | ID_0 | MC_1 | TACLR;
	TA1CCR0 = 3277;
	TA1CCTL0 |= CCIE;

	//NextADCValue = 0xCAFE;

	temp_local = 200;
	temp_remote = 200;
	fan_state = 0;
	fan_state_ctr = 0;

	P1DIR = 0x01;
	P1OUT = 0x01;

	P2IFG &= ~(BIT3 | BIT4);
	P2IE = (BIT3 | BIT4);

	while (1) {
		int c = recv_byte;
		if (c == 'A') {
			P1OUT |= 0x01;
			P2OUT |= 0x03;
			UCA0TXBUF = '1';
		}
		if (c == 'B') {
			P1OUT &= ~(0x01);
			P2OUT &= ~(0x03);
			UCA0TXBUF = '0';
		}
		if (c == 'C') {
			transmitvalue();
		}
		if (c == 'D') {
			fan_state = 1;
		}
		if (c == 'E') {
			fan_state = 0;
		}
		if (input_set) {
			if (input_set & BIT3) {
				fan_state = 0;
			}
			if (input_set & BIT4) {
				fan_state = 1;
			}
			fan_state_ctr = 0;
			input_clr = input_set;
			input_set = 0;
		}
		recv_byte = 0;
		__bis_SR_register(CPUOFF);
		if (reading_complete) {
			temp_local = adc_to_decidegc(adc_val[1]);
			temp_remote = adc_to_decidegc(adc_val[0]);
			temp_diff = temp_remote - temp_local;
			if (temp_diff < 0) {
				temp_diff = -(temp_diff);
			}
			if (temp_diff > 40) {
				if (fan_state_ctr < 5) {
					fan_state_ctr ++;
				} else {
					fan_state = 1;
					fan_state_ctr = 0;
				}
			}
			if (temp_diff < 25) {
				if (fan_state_ctr < 5) {
					fan_state_ctr ++;
				} else {
					fan_state = 0;
					fan_state_ctr = 0;
				}
			}
			reading_complete = 0;
			if (fan_state) {
				P2OUT |= 4;
			} else {
				P2OUT &= ~(4);
			}
		}
		if (tick && reading_complete == 0) {
			tick = 0;
			// start new reading
			ADC10SA = (uint16_t)adc_val;
			ADC10CTL0 |= (ENC | ADC10SC);
		}
	}

	return 0;
}

void __attribute__ ((interrupt(PORT2_VECTOR))) Port2_ISR(void) {
	uint16_t flg;
	flg = P2IFG & P2IE;
	if (flg & BIT3) {
		P2IFG &= ~(BIT3);
		P2IE &= ~(BIT3);
	}
	if (flg & BIT4) {
		P2IFG &= ~(BIT4);
		P2IE &= ~(BIT4);
	}
	input_set = flg;
	__bic_SR_register_on_exit(CPUOFF);
}

void __attribute__ ((interrupt(ADC10_VECTOR))) ADC10_ISR (void) {
	reading_complete = 1;
}

void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) TA0_ISR (void) {
	TA0CTL &= ~(TAIFG);
	if (input_clr) {
		P2IE |= (input_clr);
		input_clr = 0;
	}
	__bic_SR_register_on_exit(CPUOFF);
}

void __attribute__ ((interrupt(TIMER1_A0_VECTOR))) TA1_ISR (void) {
	TA1CTL &= ~(TAIFG);
	tick = 1;
	__bic_SR_register_on_exit(CPUOFF);
}

void __attribute__ ((interrupt(USCIAB0RX_VECTOR))) USCIAB0RX_ISR(void) {
	recv_byte = UCA0RXBUF;
	__bic_SR_register_on_exit(CPUOFF);
}
