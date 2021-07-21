 /*
 *  Stepper motor sequencer
 *
 */  
 // MBED
#include "math.h"
#include "rtos.h"  
#include "mbed.h"
#include "USBSerial.h" 
// C-SDK
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
#include "pio_stepper_2.h"
//  
using namespace rtos; // we will be using rtos::ThisThread  
using namespace mbed;

// ===========================================
//
// core 0 will run the UI for PIO testing
//   and blinks an LED

// === CORE 0 setup ==========================
// Declaring the Thread objects  (MBED)
// NOTE:  threads hang until serial monitor is attached
Thread led25_thread; 
Thread print_thread ; 

 //  pio assembler link
PIO pio = pio0;
uint offset = pio_add_program(pio, &stepper_program);
// set up pio state machine
void pio_test_start(PIO pio, uint sm, uint offset, uint pin) {
    stepper_program_init(pio, sm, offset, pin);
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
// Thread functions DO NOT exit
USBSerial serial_port ; // (MBED object)
//
DigitalOut pio_new_data_p4(p4); // MBED
DigitalIn pio_ack(p11) ;
//
// serial thead
 void serial_function() {  
    // number of 125 MHz ticks in the stepper step
    int step_count = 1000000 ;
    // step patterns
    #define step_stop 0x00000000
    // bits 1-2-3-4-1-2-3-4
    #define step_full_forward  0x12481248 
    #define step_full_reverse  0x84218421 
    // bits 1-12-2-23-3-34-4-41
    #define step_half_forward  0x13264c89 
    #define step_half_reverse  0x98c46231 
    unsigned int steps[5] = 
       {step_stop, step_full_forward, step_full_reverse, step_half_forward, step_half_reverse} ;
    int pattern = 1;

    // the usual sart message
    serial_port.printf("Starting ...PIO\r\n") ;  

    // === start the PIO ===
    #define gpio6 6
    // start sm running
    pio_test_start(pio, 0, offset, gpio6);
    // give the state machine initial data 
    pio_sm_put_blocking (pio, 0, step_count);
    // put this between data-send because PIO so fast
    pio_new_data_p4 = 0 ;
    pio_sm_put_blocking (pio, 0, steps[pattern]);
    

   // === 
   while(1){  
    // test input and print
    // In PuTTY setup, Terminal panel:
    // set Local echo and Local editing to 'FORCE ON'
    serial_port.printf("step_count, pattern>") ; 
    serial_port.scanf("%d %d", &step_count, &pattern) ;
    if ((pattern < 0) || (pattern > 3)) pattern = 0; 
    // tell PIO there is new data
    pio_new_data_p4 = 1 ;
    // wait for ACK
    while(pio_ack.read() == 0) {};
    // sned the data
    pio_sm_put_blocking (pio, 0, step_count);
    pio_sm_put_blocking (pio, 0, steps[pattern]);
    // signal new data sent
    pio_new_data_p4 = 0 ;
    
    //ThisThread::sleep_for(10);
    //busy_wait_us (50);
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
