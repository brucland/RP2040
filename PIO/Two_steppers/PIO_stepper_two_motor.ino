 /*
 * --- Stepper motor sequencer ---
 * Two PIO state machine ae started
 * -- The first takes two input words from a thread to set step  rate and pattern
 * -- The second is a counter which watches an input pin and increments on rising edge
 *    This has the effect of counting full 4 or 8-step cycles.
 *
 */  
 // MBED
#include "math.h"
#include "rtos.h"  
#include "mbed.h"
#include "USBSerial.h" 
// C-SDK
//#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/clocks.h"

// multicore sync functions
// NOTE -- SDK uses spin-locks 0-15 -- DO NOT use 16 to 31 OK
// sycc.h also has get_cpuid function
#include "hardware/sync.h"
//
// PIO program/setup include
#include "pio_stepper_two.h"
//  
using namespace rtos; // we will be using rtos::ThisThread  
using namespace mbed;

// convenient constants
#define spin_wait {}
#define sys_freq 125000000
// ===========================================
//
// core 0 will run the UI for PIO testing
//   and blinks an LED

// === CORE 0 setup ==========================
// Declaring the Thread objects  (MBED)
// NOTE:  threads hang until serial monitor is attached
Thread led25_thread; 
Thread print_thread ; 

 //  pio assembler links from included file
PIO pio = pio0;
// machine 0 -- stepper initialization
uint offset0 = pio_add_program(pio, &stepper_program);
// set up pio state machine
void pio_stepper0_start(PIO pio, uint sm, uint offset, uint pin) {
    stepper0_program_init(pio, sm, offset, pin);
    pio_sm_set_clkdiv (pio, sm, 1.0) ;
    // drain the  FIFOs
    pio_sm_clear_fifos (pio, sm);
    pio_sm_set_enabled(pio, sm, true);
}
// machine 1 -- counter initialization
uint offset1 = pio_add_program(pio, &step_counter_program);
// set up pio state machine
void pio_counter0_start(PIO pio, uint sm, uint offset, uint pin) {
    step_counter0_program_init(pio, sm, offset, pin);
    pio_sm_set_clkdiv (pio, sm, 1.0) ;
    // drain the  FIFOs
    pio_sm_clear_fifos (pio, sm);
    pio_sm_set_enabled(pio, sm, true);
}

// machine 2 -- stepper initialization
uint offset2 = pio_add_program(pio, &stepper_program);
// set up pio state machine
void pio_stepper1_start(PIO pio, uint sm, uint offset, uint pin) {
    stepper1_program_init(pio, sm, offset, pin);
    pio_sm_set_clkdiv (pio, sm, 1.0) ;
    // drain the  FIFOs
    pio_sm_clear_fifos (pio, sm);
    pio_sm_set_enabled(pio, sm, true);
}
// machine 3 -- counter initialization
uint offset3 = pio_add_program(pio, &step_counter1_program);
// set up pio state machine
void pio_counter1_start(PIO pio, uint sm, uint offset, uint pin) {
    step_counter1_program_init(pio, sm, offset, pin);
    pio_sm_set_clkdiv (pio, sm, 1.0) ;
    // drain the  FIFOs
    pio_sm_clear_fifos (pio, sm);
    pio_sm_set_enabled(pio, sm, true);
}

 // Thread function pin toggle (MBED)
 DigitalOut LED_p25(p25);
 // Thread functions DO NOT exit
 void led25_function() {  
   while(1){
     LED_p25 = 1;
     ThisThread::sleep_for(250);
     LED_p25 = 0;
     ThisThread::sleep_for(250);
   }
 }

// thread to make a serial port and use it
// see https://os.mbed.com/docs/mbed-os/v6.10/apis/usbserial.html
// 
USBSerial serial_port ; // (MBED object)
//
// signal to tell machine 0 that the cpu has new data for it
//DigitalOut pio_new_data_p4(p4) ; // MBED
//
// serial thead
 void serial_function() {  
    // max counter because state machine call only decriment a count
    int step_count = 1, step_count1 = 1 ;
    // steps/sec
    int step_rate = 500, step_rate1 = 500 ;
    // step patterns
    // the stop pattern just toggle one line to tick out
    // the number of requested steps
    #define step_stop 0x80808080  // 0x00000000
    // bits 1-2-3-4-1-2-3-4
    #define step_full_forward  0x12481248  //0001 0010 0100 1000 0001 0010 0100 1000_// 
    #define step_full_reverse  0x84218421 
    // bits 1-12-2-23-3-34-4-41
    #define step_half_forward  0x13264c89  //0001 0011 0010 0110 0100 1100 1000 1001  0x13264c89 
    #define step_half_reverse  0x98c46231 
    unsigned int steps[5] = 
       {step_stop, step_full_forward, step_full_reverse, step_half_forward, step_half_reverse} ;
    // set the initial pattern -- for production, this would be zero   
    int pattern = 0, pattern1 = 0;

    // the usual sart message
    serial_port.printf("Starting...\n\r") ;  
    
    // === start the PIO ===
    // stepper0 outputs will be on gpio 6 7 8 9
    // counter 0 signals on pin 5 (not used in C, just in PIO assembler)
    #define gpio6 6
    // stepper1 outputs will be on gpio 10 11 12 13
    // counter 1 signals on pin 14 (not used in C, just in PIO assembler)
    #define gpio10 10
    //
    #define machine0 0
    #define machine1 1
    #define machine2 2
    #define machine3 3

    // start sm's running
    pio_counter0_start(pio, machine1, offset1, 0);
    pio_stepper0_start(pio, machine0, offset0, gpio6);
    pio_counter1_start(pio, machine3, offset3, 0);
    pio_stepper1_start(pio, machine2, offset2, gpio10);
    //
    
    // state machine 1 data -- count init to desired step count
    pio_sm_put_blocking (pio, machine1, step_count);
    // give the state machine 0 initial data -- rate, pattern
    pio_sm_put_blocking (pio, machine0, sys_freq/step_rate);
    // put this handshak clear between data-send because PIO so fast
    pio_sm_put_blocking (pio, machine0, steps[pattern]);
    //serial_port.printf("%d\n\r", pio0_hw->irq);
    // clear the IRQ bit set in SM by writing ONE
    pio0_hw->irq = 0b01 ; //clear bit 0
    //serial_port.printf("%d\n\r", pio0_hw->irq);

    // state machine 3 data -- count init to desired step count
    pio_sm_put_blocking (pio, machine3, step_count);
    // give the state machine 2 initial data -- rate, pattern
    pio_sm_put_blocking (pio, machine2, sys_freq/step_rate);
    // put this handshak clear between data-send because PIO so fast
    pio_sm_put_blocking (pio, machine2, steps[pattern]);
    //serial_port.printf("%d\n\r", pio0_hw->irq);
    // clear the IRQ bit set in SM by writing ONE
    // machine 2 signals on bit 2
    pio0_hw->irq = 0b100 ; //clear bit 2

   // === 
   while(1){  
    // In PuTTY setup, Terminal panel:
    // set Local echo and Local editing to 'FORCE ON'
    serial_port.printf("step/sec, pattern, step count, step/sec1, pattern1, step count1\n\r") ; 
    serial_port.scanf("%d %d %d %d %d %d", &step_rate, &pattern, &step_count,
                     &step_rate1, &pattern1, &step_count1 ) ;
    if ((pattern < 0) || (pattern > 4)) pattern = 0; 
    if ((pattern1 < 0) || (pattern1 > 4)) pattern1 = 0; 
    // NOTE step count = 0 drops through counter regardless
    // of the step pattern. Used IFF pattern=0
    //if (pattern == 0) step_count = 0;
    //if (pattern1 == 0) step_count1 = 0;

    // now wait for ACK  on IRQ 0 from stepper machine
    // need this because mahine may tak several mSec to get here
    // depending on step rate
    // serial_port.printf("%d\n\r", pio0_hw->irq); // testing
    // machine 0 signals on bit 0
    while ((pio0_hw->irq & 0b01) == 0) spin_wait ;
    //
    // send data to counter machine
    pio_sm_put_blocking (pio, machine1, step_count);
    // send data to stepper machine
    pio_sm_put_blocking (pio, machine0, sys_freq/step_rate);
    pio_sm_put_blocking (pio, machine0, steps[pattern]);
    // clear the IRQ bit by writing ONE
    pio0_hw->irq = 0b01 ; //clear bit 0

    // machine 2 signals on bit 2
    //serial_port.printf("%d\n\r", pio0_hw->irq); // testing
    while ((pio0_hw->irq & 0b100) == 0) spin_wait ;
    //
    // send data to counter machine
    pio_sm_put_blocking (pio, machine3, step_count1);
    // send data to stepper machine
    pio_sm_put_blocking (pio, machine2, sys_freq/step_rate1);
    pio_sm_put_blocking (pio, machine2, steps[pattern1]);
    // clear the IRQ bit by writing ONE
    pio0_hw->irq = 0b100 ; //clear bit 0
    //
   }
 }

// ==== core 0 MAIN: Start everything ============
// program entry
int main() {   
  // start blink thread  -- MBED
  led25_thread.start(led25_function);
  // start print thread -- MBED
  print_thread.start(serial_function);
}  
// end ////
///////////