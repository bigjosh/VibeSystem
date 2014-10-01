/******************************************************************************
 * Vibe System
 * Version 0.00.0001
 * Processor: MSP-MSP430G2553.h
 * Author: Mark Umina
 * 
 * Initial revision, released for field testing
 *
 * Supported -
 * - 3-level vibration
 * - Minor button debounce
 * - Charging
 * 
 * Unsupported -
 * - LED functionality:
 *   - Charge indication
 *   - Battery indication
 * - Charging motor protection
 * - Commenting
 * - Clean up
 *
 * North Systems LLC. 
 ******************************************************************************/

#include  "msp430g2553.h"

#define     LED1                 BIT4
#define     LED2                 BIT5
#define     LED_DIR              P1DIR
#define     LED_OUT              P1OUT

#define     BUTTON               BIT6
#define     BUTTON_OUT           P1OUT
#define     BUTTON_DIR           P1DIR
#define     BUTTON_IN            P1IN
#define     BUTTON_IN            P1IN
#define     BUTTON_IE            P1IE
#define     BUTTON_IES           P1IES
#define     BUTTON_IFG           P1IFG
#define     BUTTON_REN           P1REN
#define     BUTTON_SEL           P1SEL

#define     MOTOR                BIT1
#define     MOTOR_OUT            P1OUT
#define     MOTOR_DIR            P1DIR
#define     MOTOR_IN             P1IN
#define     MOTOR_IE             P1IE
#define     MOTOR_IES            P1IES
#define     MOTOR_IFG            P1IFG
#define     MOTOR_REN            P1REN
#define     MOTOR_SEL            P1SEL

#define     CHARGING             BIT0
#define     CHARGING_OUT         P1OUT
#define     CHARGING_DIR         P1DIR
#define     CHARGING_IN          P1IN
#define     CHARGING_IE          P1IE
#define     CHARGING_IES         P1IES
#define     CHARGING_IFG         P1IFG
#define     CHARGING_REN         P1REN
#define     CHARGING_SEL         P1SEL

#define     TIMER_PWM_MODE        0
#define     TIMER_UART_MODE       1
#define     TIMER_PWM_PERIOD      2000
#define     TIMER_PWM_OFFSET      20

#define     false                 0
#define     true                  1

#define     OFF                   0
#define     LO                    1
#define     MED                   2
#define     HI                    3

#define     DELAY_COUNT           2100

enum {eOFF, eLO, eMED, eHI} eMotorState;

void initialize_leds(void);
void initialize_button(void);
void initialize_motor(void);
void initialize_charging(void);
void pre_application_mode(void);
void InitializeClocks(void);
void set_pwm(int val1, int val2);

eMotor = OFF;

void main(void)
{
  // Stop WDT
  WDTCTL = WDTPW + WDTHOLD;
  //InitializeClocks();
  initialize_button();
  initialize_leds();
  pre_application_mode();
  initialize_charging();
  __enable_interrupt();

  /* Main Application Loop */
  while(1)
  {
    // Is button still down?
    for(unsigned int delay=0; delay < DELAY_COUNT; delay++);    
   
    if(BUTTON_IN & BUTTON)
    {
      // Button press detected
      if (e_vibstate == eOFF)
      {                  
        vibstate = LO;
        set_pwm(40, 40);        // Tunable...
        LED_OUT |= LED1 + LED2;
      }
      else if (e_vibstate == eLO)
      {
        vibstate = MED;
        set_pwm(6, 1);
      }
      else if (vibstate == MED)
      {
        vibstate = HI;
        set_pwm(27, 1);
      }
      else if (vibstate == HI)
      {
        vibstate = OFF;
        LED_OUT &= ~LED1;
        LED_OUT &= ~LED2;
      }
    }
    
    // LPM0 with interrupts enabled
    __bis_SR_register(LPM3_bits + GIE);
  };  
}

void pre_application_mode(void)
{
  LED_DIR |= LED1 + LED2;
  LED_OUT |= LED1 + LED1;       // To enable the LED toggling effect

  BCSCTL1 |= DIVA_1;            // ACLK/2
  BCSCTL3 |= LFXT1S_2;          // ACLK = VLO

  initialize_motor();
  
  //__bis_SR_register(LPM3_bits + GIE); // LPM with interrupts
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void ta1_isr(void)
{
  TACCTL1 &= ~CCIFG;
  TAR = 0;
  
  if(OFF == vibstate)
  {
    MOTOR_OUT &= ~MOTOR;
  }
  else if(HI == vibstate)
  {
    MOTOR_OUT |= MOTOR;
  }
  else 
  {
    MOTOR_OUT ^= MOTOR;
  }
}

void initialize_clocks(void)
{
  BCSCTL1 = CALBC1_1MHZ;  // Set range
  DCOCTL = CALDCO_1MHZ;
  BCSCTL2 &= ~(DIVS_3);   // SMCLK = DCO = 1MHz
}

void initialize_button(void)
{
  BUTTON_DIR &= ~BUTTON;
  BUTTON_OUT |= BUTTON;
  BUTTON_REN |= BUTTON;
  BUTTON_IES &= ~BUTTON;
  BUTTON_IFG |= BUTTON;
  BUTTON_IE |= BUTTON;
}

void initialize_motor(void)
{
  MOTOR_DIR |= MOTOR;
  MOTOR_OUT &= ~MOTOR;
  MOTOR_REN &= ~MOTOR;
  MOTOR_IES |= MOTOR;
  MOTOR_IFG &= ~MOTOR;
  MOTOR_IE &= ~MOTOR;
}

void initialize_charging(void)
{
  CHARGING_DIR &= ~CHARGING;
  CHARGING_OUT |= CHARGING;
  CHARGING_REN |= CHARGING;
  CHARGING_IES &= ~CHARGING;
  CHARGING_IFG |= CHARGING;
  CHARGING_IE |= CHARGING;
}

void initialize_leds(void)
{
  LED_DIR |= LED1 + LED2;
  LED_OUT |= LED1 + LED2;
}

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{   
  BUTTON_IFG = 0;
  BUTTON_IE &= ~BUTTON;
  BUTTON_IE |= BUTTON;
  
  // Return to active mode
  __bic_SR_register_on_exit(CPUOFF + GIE);
}

#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR(void)
{
    IE1 &= ~WDTIE;                   // Disable interrupt
    IFG1 &= ~WDTIFG;                 // Clear interrupt flag
    WDTCTL = WDTPW + WDTHOLD;        // Put WDT back in hold state
    BUTTON_IE |= BUTTON;             // Debounce complete
}

void set_pwm(int val1, int val2)
{
    TACTL = TASSEL_1 | MC_1;          // TACLK = SMCLK, Up mode.
    TACCTL1 = CCIE + OUTMOD_3;        // TACCTL1 Capture Compare
    TACCR0 = val1;
    TACCR1 = val2;  
}
