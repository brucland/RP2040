 /*
 * A basic example to show multithreading in mbed OS. 
   -> Convert all i/o to MBED calls from Arduino
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
 */  
#include "rtos.h"  
#include "mbed.h"
#include "USBSerial.h" 
  
using namespace rtos; // we will be using rtos::ThisThread  
using namespace mbed;

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
// builtin LED 
DigitalOut LED_p25(p25);

// Make an ISR to be Triggered by timer to toggle p20
 void led20_function() {  
     LED_p20 = 1 ;
     // stretch the pulse a little
     wait_us(2) ;
     LED_p20 = 0 ;
 }

// Make an interrupt routine for pin19 ISR to toggle pin18
void ISR_function() {  
  LED_p18 = 1 ;
  wait_us(2) ;
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
// Thread functions DO NOT exit
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
    // formatted output
     serial_port.printf("%d %d\n\r", count, test_in);
    // blink for testing
     LED_p25 = 1 ;
     ThisThread::sleep_for(250);
     LED_p25 = 0 ;
   }
 }

 //  Now start everything
int main() {   
  // attach a function to the p19 instrrupt
  int_p19.fall(&ISR_function) ;
  // attach a function to Ticker interrupt
  // 20 uSec is near to the minimum for this method (thread timing is poor at this speed)
  led20_ISR.attach(&led20_function, std::chrono::microseconds(200));
  // start blink thread
  led21_thread.start(led21_function);
  // start print thread
  print_thread.start(serial_function);
}  
