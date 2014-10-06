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



volatile unsigned char white_led_PWM;		// 0-255
volatile unsigned char red_led_PWM;		// 0-255
volatile unsigned char motor_PWM;		// 0-255



typedef enum  {
	MODE_OFF,			// L4 shutdown. Waiting for either button press or charger.
	MODE_ON1,

} mode_type;

volatile mode_type mode;


typedef enum  {
	MOTOR_STEADY,
	MOTOR_WAVE_UP,
	MOTOR_WAVE_DOWN,
	MOTOR_THUMP
} motor_mode_type;

volatile motor_mode_type motor_mode;


volatile char motor_dir;		// 0=in a steady mode, -1=counting down, 1=counting up
volatile unsigned char motor_timer;		// 0=in a steady mode

#define BUTTON_LOCKOUT_TIME	((unsigned char)100)			// Need button to stay low this long before reset, to debounce

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

	unsigned char port = DEVICES_IN;		// Grab current port state

	step++;

	if (step==(PWM_STEPS-1)) {	// Did we overflow?
		step=0;		// Reset cycle
		port |= ( RED_LED_BIT | MOTOR_BIT | WHITE_LED_BIT ) ;		// Reset all devices on (note that devices with 0 PWM will not actually get turned on due to code below)


		// Periodic stuff to do once each PWM full cycle

		switch (motor_mode) {
		case MOTOR_STEADY: 		break;
		case MOTOR_WAVE_UP: 	if(motor_PWM++ == 255) motor_mode=MOTOR_WAVE_DOWN; break;
		case MOTOR_WAVE_DOWN: 	if(--motor_PWM ==   1) motor_mode=MOTOR_WAVE_UP; break;
		default: _never_executed();

		}


	}

	if (step>=white_led_PWM) {
		port &= ~WHITE_LED_BIT;
	}

	if (step>=red_led_PWM) {
		port &= ~RED_LED_BIT;
	}

	if (step>=motor_PWM) {
		port &= ~MOTOR_BIT;
	}

	if (button_lockout_countdown) {		// Are we in a debounce?

		if (port & BUTTON_BIT ) {	// button currently pressed?

			button_lockout_countdown = BUTTON_LOCKOUT_TIME;		// Reset counter;

		} else {

			button_lockout_countdown--;			// Otherwise count down...

		}


	}

	if ( DEVICES_IN & BUTTON_BIT ) {
		port |= WHITE_LED_BIT;
	} else {
		port &= ~WHITE_LED_BIT;
	}

	DEVICES_OUT = port;					// Update port output values

//    DEVICES_OUT ^= WHITE_LED_BIT;

}

/*
 * ===== Port 1 interrupt service routine====
 *
 * Handles:
 * 	button press
 *
 */

//

unsigned char motor_state;

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{

	DEVICES_IFG = 0; // Clear all pending flags

	if (button_lockout_countdown==0) {

		motor_state++;

		switch(motor_state) {

		case 1:	motor_PWM= 10; break;
		case 2:	motor_PWM= 50; break;
		case 3:	motor_PWM=100; break;
		case 4:	motor_PWM=200; break;
		case 5:	motor_PWM=255; break;
		case 6:	motor_PWM=  0; motor_state=0; break;

		default:
			_never_executed();
		}

		button_lockout_countdown = BUTTON_LOCKOUT_TIME;		// Start the countdowntimer for debounce

	}

	__bis_SR_register( LPM0_bits | GIE);       // w/ interrupt

}


void cmain(void)
{
	WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer

	// Speed up the clock...

	BCSCTL1 = XT2OFF | RSEL3 | RSEL2 | RSEL1 ; 		// XT2OFF, rsel = 14;
	DCOCTL = DCO2 |DCO1 | DCO0;						// DCO = 7;

	//Now running at 15.197mhz - fast enough for manual PWM bit banging

	// Setup Pins

	// Set all device pins to output mode

	DEVICES_DIR = WHITE_LED_BIT | RED_LED_BIT | MOTOR_BIT;

	// Set up Timer

	// The Timer on this part is very limiting so we need to be creative.
	// We will set the timer to generate an interrupt at 5Khz and we will use that to do manual PWM in the ISR

	TACTL = TASSEL_2 | MC_1;           // Timer_A Control Register

									  // TASSEL       = 10   Timer_A Source = SMCLK
									  // ID           = 00   Input divider /1
									  // MC           = 01   Up mode, count up to TACCR0 and roll over

	TACCR0 = 500;      					// Timer_A Compare Register 1
										// We will generate an interrupt each time we get up to this value

	TACCTL0 = CCIE;						// Enable interrupt on reaching TACCR0

	// Set up pin interrupts

	DEVICES_IE = BUTTON_BIT;					// Enable interrupts on the pins

	DEVICES_IFG = 0;							// Clear any pending flags

	red_led_PWM = 0;
	white_led_PWM = 0;

	motor_state=0;
	button_lockout_countdown = 0;

	// Sleep and enable the interrupts
	__bis_SR_register( LPM0_bits | GIE);       // w/ interrupt
}

