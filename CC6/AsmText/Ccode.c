#include <msp430.h>


// All devices on the same port for bit efficiency...
#define DEVICES_DIR	P1DIR
#define DEVICES_OUT	P1OUT
#define DEVICES_IN	P1IN
#define DEVICES_IFG	P1IFG			// Interrupt flag reg
#define DEVICES_IES	P1IES			// Interrupt edge select, 0=interrupt on low to high
#define DEVICES_IE	P1IE			// INterrupt enable on pin

// Outputs
#define RED_LED_BIT 	BIT4
#define WHITE_LED_BIT 	BIT5
#define MOTOR_BIT 		BIT1

//Inputs
#define CHARGE_BIT 		BIT0		// LOW indicates charge in progress
#define PG_BIT			BIT2		// LOW indicates Power Good (charer is seeing power)
#define EOG_BIT			BIT7		// End of charge (battery full)

#define LOWBATT_BIT		BIT3		// Low battery signal
#define BUTTON_BIT		BIT6		// HIGH on button press

#define UPDATE_FREQ_HZ		500						// PWM cycle freqency
#define PWM_STEPS			256						// Number of PWM steps per cycle. 0=off, PWM_STEPS-1=on
#define SMCLK_HZ 			1000000					// Device boots to SMCLK=1Mhz

#define UPDATE_COUNT 		(SMCLK_HZ/(UPDATE_FREQ_HZ*PWM_STEPS))	// Value for Timer counter to generate above update freq

//volatile unsigned char white_led_PWM;		// 0-255
//volatile unsigned red_led_PWM;		// 0-255
volatile unsigned char motor_PWM;		// 0-255


typedef enum  {
	MOTOR_STEADY,
	MOTOR_WAVE_UP,
	MOTOR_WAVE_DOWN,
	MOTOR_THUMP_ON,
	MOTOR_THUMP_OFF
} motor_mode_type;

volatile motor_mode_type motor_mode;

//volatile char motor_dir;		// 0=in a steady mode, -1=counting down, 1=counting up

volatile unsigned char motor_thump_on_time;		// How long to stay on durring a Thumper cycle
volatile unsigned char motor_thump_off_time;		// How long to stay off  durring a Thumper cycle

#define BUTTON_LOCKOUT_TIME	((unsigned char)200)			// Need button to stay low this long before reset, to debounce

unsigned char motor_state;

volatile unsigned char button_lockout_countdown;		// countdown while button low

/*
 *  ======== Timer_A2 Interrupt Service Routine ========
 *  Called at UPDATE_FREQ_HZ based on Timer_A2 running in UP mode
 *  Does ghetto PWM
 */
#pragma vector=TIMERA0_VECTOR
__interrupt void TIMERA0_ISR_HOOK(void)
{

	static unsigned char step;			// Cycle this though to keep track of pwm values

	step++;

	if (!step) {	// Did we overflow?


		// Periodic stuff to do once each PWM full cycle

		static unsigned motor_timer;		// used in thump mode to time the on/off change

		switch (motor_mode) {


			case MOTOR_STEADY:
				break;

			case MOTOR_WAVE_UP:
				motor_PWM++;
				if( motor_PWM >= 250) {
					motor_mode=MOTOR_WAVE_DOWN;
				}
				break;

			case MOTOR_WAVE_DOWN:
				motor_PWM--;
				if( motor_PWM <=  10) {
					motor_mode=MOTOR_WAVE_UP;
				}
				break;

			case MOTOR_THUMP_ON:
				motor_timer++;
				if(motor_timer>=motor_thump_on_time) {		// This is safe for any initial value of motor timer

					motor_PWM = 0;

					motor_mode= MOTOR_THUMP_OFF;

				}
				break;

			case MOTOR_THUMP_OFF:
				motor_timer++;
				if(motor_timer>=motor_thump_off_time) {		// This is safe for any initial value of motor timer

					motor_timer = 0;

					motor_PWM = 200;

					motor_mode= MOTOR_THUMP_ON;

				}
				break;


			default: _never_executed();

		}


	}

//	if (step>=white_led_PWM) {
//		port &= ~WHITE_LED_BIT;
//	}

//	if (step>=red_led_PWM) {
//	}


	// I know this is all contorted, but nessisary to minimized generated code size...

	unsigned char port = 0;

	// Simulate PWM by turning on bit when step is less than current PWM setting

	if (step<motor_PWM) {
		port |= MOTOR_BIT;
	}

	// Make the LED's show bottom two binary digits of motor state

	DEVICES_OUT = port | ((motor_state & 3) << 4) ;


	if (button_lockout_countdown) {			// Are we in a debounce?

		button_lockout_countdown--;			// count down...

		if (DEVICES_IN & BUTTON_BIT ) {			// button still currently pressed?

			button_lockout_countdown = BUTTON_LOCKOUT_TIME;		// Reset counter;

		}
	}

}

/*
 * ===== Port 1 interrupt service routine====
 *
 * Handles:
 * 	button press
 *
 */

//


#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{

	DEVICES_IFG = 0; // Clear all pending flags

	if (button_lockout_countdown==0) {

		// A switch would have been clearer here, but we just dont have room.

		/*
		motor_state++;

		switch(motor_state) {

		case 1:	motor_PWM= 10; DEVICES_OUT |= RED_LED_BIT; break;
		case 2:	motor_PWM= 50; break;
		case 3:	motor_PWM=100; break;
		case 4:	motor_PWM=200; break;
		case 5:	motor_PWM=255; break;
		case 6:	motor_mode=MOTOR_THUMP_ON; motor_thump_on_time=10; motor_thump_off_time=40; break;
		case 7:	motor_mode=MOTOR_THUMP_ON; motor_thump_on_time=5; motor_thump_off_time=5; break;
		case 8:	motor_mode=MOTOR_WAVE_DOWN;  break;
		case 9:	motor_mode=MOTOR_STEADY; motor_PWM=  0; motor_state=0; DEVICES_OUT &= ~RED_LED_BIT; break;//off

		default:
			_never_executed();
		}

		*/

		motor_state++;

		if (motor_state==1) {		// Off?

			motor_PWM=4;		//

		} else {


			if ( motor_state < 21 ) {		// 1-10 = 10%-100%

				motor_PWM += 4;

				if (motor_state > 11 ) {

					motor_PWM += 16;

				}


			} else {						// Turn off

				if (motor_state==21) {

					motor_mode=MOTOR_THUMP_ON; motor_thump_on_time=10; motor_thump_off_time=40;

				} else if (motor_state==22) {

					motor_thump_on_time=5; motor_thump_off_time=5;

				} else if (motor_state==23) {

					motor_mode= MOTOR_WAVE_DOWN;


				} else {

					motor_mode=MOTOR_STEADY;
					motor_PWM=0;
					motor_state=0;

				}
			}


		}

		button_lockout_countdown = BUTTON_LOCKOUT_TIME;		// Start the countdowntimer for debounce


	}

}


/*
 *
 * This is the entry point from the ASM start up. It is *not* like main - we do not have any
 * assumptions when we get here. No variables are in a known state and nothting is set up
 * except for the start pointer. Do not Return from this procedure becuase there is no
 * place to return from.
 *
 */

void cstart(void)
{
	WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer

	// Speed up the clock...

	BCSCTL1 = XT2OFF | RSEL3 | RSEL2 | RSEL1 ; 		// XT2OFF, rsel = 14;
	DCOCTL = DCO2 |DCO1 | DCO0;						// DCO = 7;

	//Now running at 15.197mhz - fast enough for manual PWM bit banging

	// Setup Pins

	// Set all device pins to output mode

//	DEVICES_OUT = 0;
	DEVICES_DIR = WHITE_LED_BIT | RED_LED_BIT | MOTOR_BIT;

	// Set up Timer

	// The Timer on this part is very limiting so we need to be creative.
	// We will set the timer0 to generate an interrupt at 5Khz and we will use that to do manual PWM in the ISR

	TACTL = TASSEL_2 | MC_1;           // Timer_A Control Register

									  // TASSEL       = 10   Timer_A Source = SMCLK
									  // ID           = 00   Input divider /1
									  // MC           = 01   Up mode, count up to TACCR0 and roll over

	TACCR0 = 500;      					// Timer_A Compare Register 1
										// We will generate an interrupt each time we get up to this value

	TACCTL0 = CCIE;						// Enable interrupt on reaching TACCR0

	// Set up pin interrupts

	DEVICES_IES = 0;							// Low to high transition interrupt on all pins

	DEVICES_IE = BUTTON_BIT;					// Enable interrupts on the button pin

	DEVICES_IFG = 0;							// Clear any pending flags

	motor_PWM=0;

	motor_state=0;
	motor_mode=MOTOR_STEADY;
	button_lockout_countdown = 0;

	// Sleep and enable the interrupts
	__bis_SR_register( LPM1_bits | GIE);       // w/ interrupt

}



