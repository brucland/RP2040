 /*
 * A basic example to show multithreading in mbed OS. 
   -> Convert all i/o to MBED calls from Arduino
 * Two threads are created to toggle pins as fast as ;possible
 * with or without yielding (and no ISR).
   -- With yield: two pulse trains at 142 KHz, pulse duration 0.1 uSec
      looks like task context switching time ~3 uSec
   -- no yield: alternating bursts of pulse trains at 4 MHz about 5 mSec long
      appears that context switch is ~3 uSec
 * With one yielding thread and ISR
   -- two pulse trains of 107 KHz, entry into ISR ~5 uSec, exit ~4 uSec
 * With two yielding threads and ISR
   -- three pulse trains of ~80 KHz, entry into ISR ~5 uSec, exit ~4 uSec
   -- p19 ISR input is connected to p20 pulse output
 * with USB serial (and two fast threads and ISR) you MUST:
   -- check to make sure the board is correct (aviod compile error)
   -- open a PuTTY window with the correct COM port number and baud rate
   -- DO NOT choose the Pico+COM board entry
     (the arduino monitor does not work in this mode)
 */  
#include "rtos.h"  
#include "mbed.h"
#include "USBSerial.h" 
  
using namespace rtos; // we will be using rtos::ThisThread  
using namespace mbed;

// Declaring the Thread objects  
// NOTE:  threads hang until serial monitor is attached
Thread led20_fast_thread; 
Thread led21_fast_thread; 
Thread print_thread ; 

// Declare i/o -
// set up an ISR source for timing, ISR input from p20
InterruptIn int_p19(p19);
// thread logic output to scope
DigitalOut LED_p21(p21);
// another thread logic output to scope + ISR
DigitalOut LED_p20(p20);
// ISR toggle logic to scope 
DigitalOut LED_p18(p18);
// builtin LED 
DigitalOut LED_p25(p25);

// Make an interrupt routine
void ISR_function() {  
  LED_p18 = 1 ;
  LED_p18 = 0 ;
}

 // without  yield about 1 MHz
 // with yield, about 460 KHz
 // 1/0 pulse length 500 nSec/
 void led21_fast_function() {  
   while(1){
     LED_p21 = 1;
     ThisThread::sleep_for(1);
     LED_p21 = 0;
     //yield();
     ThisThread::sleep_for(1);
   }
 }
//
 void led20_fast_function() {  
   while(1){
     LED_p20 = 1 ;
     ThisThread::sleep_for(2);
     LED_p20 = 0 ;
     //yield();
     ThisThread::sleep_for(1);
   }
 }

// make a serial port
// see https://os.mbed.com/docs/mbed-os/v6.10/apis/usbserial.html
USBSerial serial_port ;

 void serial_function() {  
   int count=0;
   int test_in ;
   
   while(1){
     count++ ;
    // test input and print
    // In PuTTY setup, Terminal panel:
    // set Local echo and Local editing to 'FORCE ON'
     serial_port.scanf("%d", &test_in) ;
    //
     serial_port.printf("%d %d\n\r", count, test_in);
    //
     LED_p25 = 1 ;
     ThisThread::sleep_for(250);
     LED_p25 = 0 ;
   }
 }
 //  
int main() {   
  int_p19.fall(&ISR_function) ;
  led21_fast_thread.start(led21_fast_function);
  led20_fast_thread.start(led20_fast_function);
  print_thread.start(serial_function);
}  
