 /*
 * A basic example to show multithreading in mbed OS. 
   AND multicore operation, with no MBED on core1!
 * CORE 0  
 * Two yielding threads, one timer ISR, and one input pin ISR
 * With two yielding threads and two ISR
   -- Timer ISR triggers p20 pulse 
   -- p19 ISR input is connected to p20 pulse output
      latency ~10 uSec, jitter 1 uSec 
      probably 5 uSec to get out of timer isr + 5 to get into p19 isr
 * with USB serial you MUST:
   -- check to make sure the board is correct (aviod compile error)
   -- open a PuTTY window with the correct COM port number and baud rate
   -- DO NOT choose the Pico+COM board entry
     (the arduino monitor does not work in this mode)
  * CORE 1
    -- GPIO ISR latency is <1 uSec and ~5 USec to return to main
 */  
 //#include <pico/stdlib.h>
#include "rtos.h"  
#include "mbed.h"
#include "USBSerial.h" 
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/time.h"
// core sync functions
// NOTE -- SDK uses spin-locks 0-15 -- DO NOT use
// can use spin-lock 16 to 31
// sycc.h also has get_cpuid function
#include "hardware/sync.h"
  
using namespace rtos; // we will be using rtos::ThisThread  
using namespace mbed;

// some few cycle waits
#define One_cycle asm ("nop");
#define Five_cycle_wait asm ("nop");asm ("nop");asm ("nop");asm ("nop");asm ("nop");
#define Ten_cycle_wait Five_cycle_wait;Five_cycle_wait

// === global -- both cores ==================
volatile int multicore_count ;
volatile int core_num ;
spin_lock_t * count_lock ;
// ===========================================

// === CORE 0 setup ==========================
// Declaring the Thread objects  
// NOTE:  threads hang until serial monitor is attached
Thread led21_thread; 
Thread print_thread ; 

// Set up timer ISR ticker to toggle pin 20
Ticker led20_ISR ;
// set up an ISR input from p20
InterruptIn int_p19(p19);

// Declare i/o -
// thread logic output to scope
DigitalOut LED_p21(p21);
// another thread logic output to scope + ISR
DigitalOut LED_p20(p20);
// ISR toggle logic to scope 
DigitalOut LED_p18(p18);

// Make an ISR to be Triggered by timer to toggle p20
 void led20_function() {  
     LED_p20 = 1 ;
     // stretch the pulse a little -- wait function has an overhead of 4 uSec!
     //wait_us(0) ;
     LED_p20 = 0 ;
 }

// Make an interrupt routine for pin19 ISR to toggle pin18
void ISR_function() {  
  LED_p18 = 1 ;
  //wait_us(0) ;
  LED_p18 = 0 ;
}

 // Thread function pin toggle
 // Thread functions DO NOT exit
 void led21_function() {  
   while(1){
     LED_p21 = 1;
     ThisThread::sleep_for(1);
     LED_p21 = 0;
     //yield();
     ThisThread::sleep_for(1);
   }
 }

// thread to make a serial port and use it
// see https://os.mbed.com/docs/mbed-os/v6.10/apis/usbserial.html
// Also try out ADC routines
// Thread functions DO NOT exit
USBSerial serial_port ;
 void serial_function() {  
   int count=0 ;
   int test_in ;
   int core1_count_local ;
   int save_irq ;
   
   // from C SDK section 4.1.1. hardware_adc
   // example code from manual
   adc_init();
   adc_gpio_init(26);
   // p26 is ADC input 0
   adc_select_input(0);
   // 1000 cycles o 48 MHz clock
   adc_set_clkdiv (1000);
   // free run
   adc_run(1);
   //
   while(1){
    // this thread counter
    count++ ;
    // get count from core1, with locking
    // 'unsafe' means that interrupts were not disabled
    save_irq = spin_lock_blocking(count_lock) ;
    core1_count_local = multicore_count ;
    spin_unlock(count_lock, save_irq);
     
    // test input and print
    // In PuTTY setup, Terminal panel:
    // set Local echo and Local editing to 'FORCE ON'
    // serial_port.scanf("%d", &test_in) ;
    // formatted output
    // read adc_hw directly to avoid ac_read overhead
     serial_port.printf("%d %d %d\n\r", count, adc_hw->result, core1_count_local) ;  //adc_read());
     // print once after we kow that core1 has started
     if (count==1) 
        serial_port.printf("core = %d\n\r", core_num) ;
    ThisThread::sleep_for(1000);
   }
 }

// ==== CORE 1 setup ===========================
// an ISR -- use pin 0 interrupt connected to pin 1 output
//  to toggle pin 2
void core1_gpio_isr(){
  gpio_put(2, 1) ;
  Ten_cycle_wait ; // pulses better on white board
  gpio_put(2, 0) ;
}

//Test core1 launch
// USE ONLY C-sdk library, no MBED on core1
// toggles at ~28 MHz using SDK gpio_put
void core1_main(){
  int save_irq ;
  // I don't know why the casting is necessary
  gpio_set_irq_enabled_with_callback(0, GPIO_IRQ_EDGE_RISE, 1, (gpio_irq_callback_t)core1_gpio_isr);
  // note leadingn UNDERSCORE is different that default SDK
  _gpio_init(0) ;
  _gpio_init(1) ;
  _gpio_init(2) ;
  //
  gpio_set_dir(0, GPIO_IN) ;
  gpio_set_dir(1, GPIO_OUT) ;
  gpio_set_dir(2, GPIO_OUT) ;
  // 
  gpio_put(1, 0) ;
  gpio_put(2, 0) ;

  // from sync.h
  // verify we are on core 1
  core_num = get_core_num();

  while (1) {
    gpio_put(1, 1) ;
    Ten_cycle_wait ;
    // Take the interrupt here ...
    // 1 uSec to get in 5 to get out
    // witout interrupt, loop runs at 4 million/sec
    // with interrupt runs at ~160 KHz
    gpio_put(1, 0) ;
    Ten_cycle_wait ;
    // with just the following statments get ~14 million loops/sec
    save_irq  = spin_lock_blocking(count_lock) ;
    multicore_count++ ;
    spin_unlock(count_lock, save_irq);
  }
}

// ==== core 0 MAIN: Start everything ============
// program entry
int main() {   
  // attach a function to the p19 instrrupt -- MBED
  int_p19.fall(&ISR_function) ;
  // attach a function to Ticker interrupt  -- MBED
  // 20 uSec is near to the minimum for this method (thread timing is poor at this speed)
  //  -- MBED
  led20_ISR.attach(&led20_function, std::chrono::microseconds(200));
  // start blink thread  -- MBED
  led21_thread.start(led21_function);
  // start print thread -- MBED
  print_thread.start(serial_function);
  // start core 1 -- C-SDK -- !!
  count_lock = spin_lock_init(31);
  multicore_reset_core1();
  multicore_launch_core1(&core1_main);
}  
// end ////
///////////
