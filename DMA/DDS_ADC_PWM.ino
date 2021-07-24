 /*
 * A basic example to show multithreading in mbed OS. 
   AND multicore operation, with no MBED on core 1!
   -- check to make sure the board is correct (aviod compile error)
 * CORE 0  
 * Two threads:
 *   -- The usual heartbeat
     -- USB serial with frequency set input and ADC running
        sends frequency to core1 using FIO
        starts ADC-DMA-PWM for input from core1 PWM output
 * with USB serial you MUST:
   -- open a PuTTY window with the correct COM port number and baud rate
   -- DO NOT choose the Pico+COM board entry
     (the arduino monitor does not work in this mode)
  * CORE 1
    -- sets up sine table, DMA and PWM
    -- Waits in MAIN for FIFO input from core0
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
#include "pico/multicore.h"
#include "pico/time.h"
// multicore sync functions
// NOTE -- SDK uses spin-locks 0-15 -- DO NOT use 16 to 31 OK
// sycc.h also has get_cpuid function
#include "hardware/sync.h"
  
using namespace rtos; // we will be using rtos::ThisThread  
using namespace mbed;

// ===========================================
//
// core 0 will run the UI for changing frequency and sends value to FIFO
//   and blinks an LED
//   and set up the ADC to full speed and start ADC-DMA-PWM output
//
// core 1 wil set up the DDS and start DMA-PWM sine synth
//   and wait for FIFO frequency data, then compute DMA frequency
//
// === CORE 0 setup ==========================
// Declaring the Thread objects  (MBED)
// NOTE:  threads hang until serial monitor is attached
Thread led25_thread; 
Thread print_thread ; 

 // Thread function pin toggle (MBED)
 DigitalOut LED_p25(p25);
 // Thread functions DO NOT exit
 void led25_function() {  
   while(1){
     LED_p25 = 1;
     ThisThread::sleep_for(250);
     LED_p25 = 0;
     //yield();
     ThisThread::sleep_for(250);
   }
 }

// thread to make a serial port and use it
// see https://os.mbed.com/docs/mbed-os/v6.10/apis/usbserial.html
// Also try out ADC routines
// Thread functions DO NOT exit
USBSerial serial_port ; // (MBED object)
// serial thead
 void serial_function() {  
   int frequency = 1000 ;
   int ADC_sample_cycles = 96 ;
   int ADC_sample_freq = 500000 ;
    // === from C SDK section 4.1.1. hardware_adc
    // setup ADC
    adc_init();
    adc_gpio_init(26);
    // p26 is ADC input 0
    adc_select_input(0);
    // 1000 cycles o 48 MHz clock
    adc_set_clkdiv (ADC_sample_cycles);
    // free run
    adc_run(1);
    // result is in adc_hw->result
    // but we are going to forward that to the FIFO
    //adc_fifo_setup(bool en, bool dreq_en, uint16_t dreq_thresh, bool err_in_fifo, bool byte_shift)
    // fifo_enable, dreq_enable, thresh=1, no error, shift to 8-bits(for 8-bit PWM)
    adc_fifo_setup(1,1,1,0,1);
   //
   //initial frequency -> to core1 
    multicore_fifo_push_blocking(frequency);
    serial_port.printf("freq = %d \n\r", frequency) ;  
    //
    // === pwm set up slice 7 pin 14
    #define pin14 14
    gpio_set_function(pin14, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin14);
    // full speed clock => 256 counts in 2 uSec
    pwm_set_clkdiv(slice_num, 1.0f);
    pwm_set_clkdiv_mode(slice_num, PWM_DIV_FREE_RUNNING) ;
    // max count 256
    pwm_set_wrap(slice_num, 256);
    // initial for testing to half
    pwm_set_chan_level(slice_num, PWM_CHAN_A , 128) ;
    pwm_set_enabled(slice_num, 1);
    //
    // === TWO DMA channels to send data to PWM
  // ctrl chanel resets data channel read addr
  int ctrl_chan = dma_claim_unused_channel(true);
  // data channel move sine taable to PWM
  int data_chan = dma_claim_unused_channel(true);

  // the control channel to restaart data channel 
  // -- just does a NOP write, then chains to data channel
  dma_channel_config c = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_irq_quiet(&c, true);
    channel_config_set_enable(&c, true); 
    channel_config_set_chain_to(&c, data_chan) ;
    // 
    dma_channel_configure (ctrl_chan, &c, 
         &dma_hw->ch[data_chan].read_addr, // NOP write  in other channel
         &dma_hw->ch[data_chan].read_addr,  // NOP read_addr
        1, // transfer count
        0) ;  // trigger

   // The acdtual data channel
   dma_channel_config c2 = dma_channel_get_default_config(data_chan);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_16);
    channel_config_set_read_increment(&c2, false);
    channel_config_set_write_increment(&c2, false);
    channel_config_set_irq_quiet(&c2, true);
    channel_config_set_enable(&c2, true); 
    channel_config_set_chain_to(&c2, ctrl_chan) ;
    channel_config_set_dreq(&c2, DREQ_ADC);
    //
    dma_channel_configure(data_chan, &c2, 
        &pwm_hw->slice[slice_num].cc, // write_addr, duty cycle of pwm
        &adc_hw->fifo,  // read_addr, the table
        1, // one ADC value,
        1) ; // trigger
   // === 
   while(1){  
    // test input and print
    // In PuTTY setup, Terminal panel:
    // set Local echo and Local editing to 'FORCE ON'
    serial_port.scanf("%d %d", &frequency, &ADC_sample_freq) ;
    // formatted output
    // read adc_hw directly to avoid ac_read overhead
     serial_port.printf("Freq=%d, ADC_freq=%d\n\r", frequency, ADC_sample_freq) ;  //adc_read());
     // 
     //frequency => core1  
     multicore_fifo_push_blocking(frequency);

     // set ADC divider --  max 16 bits on divider -- ADC clock is 48 MHz.
     // Therfore the minimum sample freq is around 740 Hz.
     ADC_sample_cycles = (int)(48e6 / (float)ADC_sample_freq) ;
     adc_set_clkdiv (ADC_sample_cycles) ;

    // not really necessary
    //ThisThread::sleep_for(50);
   }
 }

// ==== CORE 1 setup ===========================
//  core1 launch
// USE ONLY C-sdk library, no MBED on core1
void core1_main(){
  // freq from core0 via FIFO
  int frequency  ;
  int ADC_sample_cycles ;

  // frequency computation see near the bottom of
  // https://people.ece.cornell.edu/land/courses/ece4760/RP2040/index_rp2040_Micropython.html
  float Fout, X0, dY, Fx ;
  // cpu clock ffreq 
  float Fclk = 125e6 ; 
  // length sine table
  float Ltable = 256 ; 
  float two16 = 65536 ;
  float two32 = 4294967296 ;
  
  // === Define sine table
  // DMA ch0 source for PWM signed short
  short DMA_sine[256] ;
  // now define a pointer to the table-pointer
  int ptr_table ;
  ptr_table = (int)DMA_sine;
  // build the sine table for DMA-to-pwm
  int i = 0 ;
  while (i<256) {
      DMA_sine[i] = (short) (127 * sin(2*3.1416*i/256) + 128) ;
      i++ ;
  }
  
  // === pwm set up slice 0 pin 0
  #define pin0 0
  gpio_set_function(pin0, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(pin0);
  // full speed clock => 256 counts in 2 uSec
  pwm_set_clkdiv(slice_num, 1.0f);
  pwm_set_clkdiv_mode(slice_num, PWM_DIV_FREE_RUNNING) ;
  // max count 256
  pwm_set_wrap(slice_num, 256);
  // initial for testing to half
  pwm_set_chan_level(slice_num, PWM_CHAN_A , 128) ;
  pwm_set_enabled(slice_num, 1);

  // === TWO DMA channels to send table to PWM
  // ctrl chanel resets data channel read addr
  int ctrl_chan = dma_claim_unused_channel(true);
  // data channel move sine taable to PWM
  int data_chan = dma_claim_unused_channel(true);

  // the control channel to reset read addrs of data channel
  dma_channel_config c = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_irq_quiet(&c, true);
    channel_config_set_enable(&c, true); 
    channel_config_set_chain_to(&c, data_chan) ;
    // 
    dma_channel_configure (ctrl_chan, &c, 
         &dma_hw->ch[data_chan].read_addr, // write addr in other channel
        &ptr_table,  // read_addr, pointer-to-ponter to table
        1, // transfer count
        0) ;  // trigger

   // The acdtual data channel
   dma_channel_config c2 = dma_channel_get_default_config(data_chan);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_16);
    channel_config_set_read_increment(&c2, true);
    channel_config_set_write_increment(&c2, false);
    channel_config_set_irq_quiet(&c2, true);
    channel_config_set_enable(&c2, true); 
    channel_config_set_chain_to(&c2, ctrl_chan) ;
    // timers sets the DDS frequency
    #define DREQ_TIMER0 0x3b
    channel_config_set_dreq(&c2, DREQ_TIMER0);
    //
    dma_channel_configure(data_chan, &c2, 
        &pwm_hw->slice[slice_num].cc, // write_addr, duty cycle of pwm
         DMA_sine,  // read_addr, the table
        256, // len(DMA_sine),
        1) ; // trigger

  //Set a non-zero default freq for testing
  //dma_hw->timer[0] = (0xffff | (0x8fff<<16) ) ;
  // ===
  while (1) {
    // grab the frequency from core0
    frequency = multicore_fifo_pop_blocking();
    // modify DMA timer for frequency set
    Fout = (float) frequency ;
    X0 = (Fout/Fclk) * Ltable * two16 ;
    Fx = Fclk*((int)X0)/(Ltable * two16) ;
    dY = (Fout-Fx) * Ltable * two32 /(Fclk * ((int)X0)) ;
    // hit the DMA pacing timer directly
    dma_hw->timer[0] = (0xffff-int(dY)) | (int(X0)<<16) ;   
  }
}

// ==== core 0 MAIN: Start everything ============
// program entry
int main() {   
  // start blink thread  -- MBED
  led25_thread.start(led25_function);
  // start print thread -- MBED
  print_thread.start(serial_function);
  // start core 1 -- C-SDK -- !!
  multicore_reset_core1();
  multicore_launch_core1(&core1_main);
}  
// end ////
///////////
