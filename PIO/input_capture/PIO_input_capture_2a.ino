 /*
 * input time capture using PIO as a timer/edge detector
 * 51 clock ticks per cycle at 1 MHz input
 * works to at least 10  MHz, but no usable accuracy above ~2 MHz
 * -- PIO has 8 instructions which implements a counter and edge state machine
 * -- core0 readback dumps up to 8 time stamps from the FIFO at high frequency
 *    can run indefinitely at below a few KHz.
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
#include "pio_input_capture_2a.h"
//  
using namespace rtos; // we will be using rtos::ThisThread  
using namespace mbed;

// ===========================================
//
// core 0 will run the UI for PIO testing
//   and blinks an LED
// set up PWM for 50% duty cycle up to 1 MHz
// 8-slot FIFO acts as buffer for values at high speed

// === CORE 0 setup ==========================
// Declaring the Thread objects  (MBED)
// NOTE:  threads hang until serial monitor is attached
Thread led25_thread; 
Thread print_thread ; 

// locations to store edge time 
unsigned int PIO_time[8] ;

 //  pio assembler link
PIO pio = pio0;
uint offset = pio_add_program(pio, &capture_program);
// set up pio state machine
void pio_test_start(PIO pio, uint sm, uint offset, uint pin) {
    capture_program_init(pio, sm, offset, pin);
    pio_sm_set_clkdiv (pio, sm, 1.0) ;
    // drain the 4-slot input fifo
    // drain the transmit fifo
    pio_sm_clear_fifos (pio, sm);
   // pio_sm_drain_tx_fifo (pio, sm);
   // pio_sm_get (pio, sm);
   // pio_sm_get (pio, sm);
    //pio_sm_get (pio, sm);
   // pio_sm_get (pio, sm);
    // and turn on the state machine
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
// Also try out ADC routines
// Thread functions DO NOT exit
USBSerial serial_port ; // (MBED object)
//
// SigitalOut pio_cntl_p4(p4); // MBED
//
// serial thead
 void serial_function() {  
    // number of edge samples to grab
    int N=8 ;
    int PWM_cycles = 1200 ;
    int delta_time ;

    // the usual sart message
    serial_port.printf("Starting ...  PIO\r\n") ;  

    // === PWM channel ===
    // pwm set up slice 0 pin 0
    #define pin6 6
    gpio_set_function(pin6, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin6);
    // want 1 mSec sq wave
    pwm_set_clkdiv(slice_num, 1.0f);
    pwm_set_clkdiv_mode(slice_num, PWM_DIV_FREE_RUNNING) ;
    // max count 
    pwm_set_wrap(slice_num, 125); //62500 1KHz (with divider at 2)
    // initial to ~half
    pwm_set_chan_level(slice_num, PWM_CHAN_A , 60) ; // 31000 
    // enable later
    pwm_set_enabled(slice_num, 0);

   // === 
   while(1){  
    // test input and print
    // In PuTTY setup, Terminal panel:
    // set Local echo and Local editing to 'FORCE ON'
    serial_port.printf("PWM wrap cycles>") ; 
    serial_port.scanf("%d", &PWM_cycles) ;
    // max count 
    pwm_set_wrap(slice_num, PWM_cycles); //62500 1KHz 
    // initial to ~half
    pwm_set_chan_level(slice_num, PWM_CHAN_A , (int)PWM_cycles/2) ; // 31000 
    serial_port.printf("PWM_period=%6.2f uSec\n\r", (float)PWM_cycles/125 ) ;  

    // reset the time stamp array 
    for(int i=0; i<N; i++){
       PIO_time[i] = 0 ;  
    }

    // === start the PIO ===
    // output pin for testing sm state machine
    /// pin 3 should mimic the input waveform with ~20-30 nSec delay
    // pin 3 NOT used in this 2a version
    #define gpio3 3
    // start sm running
    pio_test_start(pio, 0, offset, gpio3);

    // === start the PWM ===
    // start PWM pulser
    pwm_set_counter (slice_num, 0);
    // and start it
    pwm_set_enabled(slice_num, 1);

    // sleep long eough for 8 samples at 1 KHz
    ThisThread::sleep_for(10);
    //busy_wait_us (50);

    // === stop the PWM ===
    // stop the pulses
    pwm_set_enabled(slice_num, 0);
    //pio_sm_set_enabled(pio, 0, false);

    for(int i=0; i<N; i++){
      //PIO_time[i] = pio_sm_get_blocking (pio, 0);
      PIO_time[i] = pio_sm_get (pio, 0);
    }
    
    // check system clock
    int system_clk = clock_get_hz (clk_sys) ;
    // read back the time stamps
    serial_port.printf("t=%x clk=%d\n\r",  PIO_time[0], system_clk) ;  
    for(int i=1; i<N; i++){
      delta_time = (signed int)PIO_time[i-1] - (signed int)PIO_time[i] ;
      serial_port.printf("t=%x  dt=%d uSec=%6.2f\n\r", 
                PIO_time[i], delta_time, (float)(delta_time+2)/62.5 ) ;  
    }
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