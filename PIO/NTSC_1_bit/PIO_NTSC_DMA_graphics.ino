 /*
 *  NTSC sync and data using DMA transfer 
 * Useful raster is 256 wide by 204 pixels high
 *
 * using fake progreassive scan
 * See https://sagargv.blogspot.com/2014/07/ntsc-demystified-color-demo-with.html
 *
 * 263 scan lines.
 * one line is exactly 63.555 microseconds long
 * H-sync pulse is zero volts for 4.7 uSec, but not too critical
 * V-sync pulse is zero volts for 29.4 uSec, but not too critical
 * if line 1 is V-synch then content can start on line 21 or so and continue
 *   until line 260 
 * LInes 1-20 and 261-263 should hold video data at zero
 * about 55 uSec of each active scan line can contain pixels
 * a 5 Mpixel/sec rate gives about 55*5 = 275 pixels/line, so use 256 pixels
 *
 * PIO implementation:
 * One line time should require waits of <32 cycles so that loops are not required.
 * Choose sync machine clock rate so that 27 cycles give 63.555 uSec, which is
 * 15,734 Hz. So machine clock should be 27*15734=424828 Hz
 * Ths colck choice means that an H-sync pulse is exactly two cycles and 
 * V-sync is approx 13 cycles.
 * The clock divider is therfore 125e6/424828 = 294.2367
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
#include "pio_ntsc.h"
//  
using namespace rtos; // we will be using rtos::ThisThread  
using namespace mbed;

// ==========================================
// video formatiing variables
// The screen buffer
// 208 lines x 8 words/line
int DMA_image_buffer[1664] ;
// pointe to screen buffer
int ptr_table ;

//One bit masks for each bit of an INT
unsigned int pos[32] = {0x80000000,0x40000000,0x20000000,0x10000000,0x08000000,0x04000000,0x02000000,0x01000000,
                0x800000,0x400000,0x200000,0x100000,0x080000,0x040000,0x020000,0x010000,
                0x8000,0x4000,0x2000,0x1000,0x0800,0x0400,0x0200,0x0100,
                0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

#include "ascii_characters.h"
#define top 0
#define left 0
#define right 255
#define bottom 204
#define erase_screen memset(DMA_image_buffer,0,1664*4);

//== plot a point ==========================================
//plot one point
//at x,y with color 1=white 0=black 2=invert
#define word_per_line 8
// force rnage check if commented out
#define live_dangerously 

inline void video_pt(int x, int y, char c) {
	//each line has 18 bytes
	//calculate i based upon this and x,y
	// the word with the pixel in it
	// optional range check
  #ifndef live_dangerously
    if(x<left || y<top || x>right || y>bottom) return ;
  #endif
  //int i = (x/32) + y*8
	int i = (x >> 5) + y * word_per_line ;
	if (c==1)
	  DMA_image_buffer[i] = DMA_image_buffer[i] | pos[x & 0x1f];
        else if (c==0)
	  DMA_image_buffer[i] = DMA_image_buffer[i] & ~pos[x & 0x1f];
        else
	  DMA_image_buffer[i] = DMA_image_buffer[i] ^ pos[x & 0x1f];
}

//=============================================================================
//plot a line
//at x1,y1 to x2,y2 with color 1=white 0=black 2=invert
//NOTE: this function requires signed chars
//Code is from David Rodgers,
//"Procedural Elements of Computer Graphics",1985
void video_line(int x1, int y1, int x2, int y2, char c) {
	int e;
	signed int dx,dy,j, temp;
	signed char s1,s2, xchange;
        signed int x,y;

	x = x1;
	y = y1;

	//take absolute value
	if (x2 < x1) {
		dx = x1 - x2;
		s1 = -1;
	}

	else if (x2 == x1) {
		dx = 0;
		s1 = 0;
	}

	else {
		dx = x2 - x1;
		s1 = 1;
	}

	if (y2 < y1) {
		dy = y1 - y2;
		s2 = -1;
	}

	else if (y2 == y1) {
		dy = 0;
		s2 = 0;
	}

	else {
		dy = y2 - y1;
		s2 = 1;
	}

	xchange = 0;

	if (dy>dx) {
		temp = dx;
		dx = dy;
		dy = temp;
		xchange = 1;
	}

	e = ((int)dy<<1) - dx;

	for (j=0; j<=dx; j++) {
		video_pt(x,y,c);

		if (e>=0) {
			if (xchange==1) x = x + s1;
			else y = y + s2;
			e = e - ((int)dx<<1);
		}

		if (xchange==1) y = y + s2;
		else x = x + s1;

		e = e + ((int)dy<<1);
	}
}

//=============================================================
// put a string of big characters on the screen
void video_string(int x, int y, char *str) {
	char char_num;
  int i;
  int y_pos;
  int j;
	for (char_num=0; str[char_num]!=0; char_num++) {
    for (i=0;i<7;i++) {
        y_pos = y + i;
        // get the template for the next character
        j = ascii[str[char_num]][i] ;
        video_pt(x,   y_pos, (j & 0x80)==0x80);
        video_pt(x+1, y_pos, (j & 0x40)==0x40);
        video_pt(x+2, y_pos, (j & 0x20)==0x20);
        video_pt(x+3, y_pos, (j & 0x10)==0x10);
        video_pt(x+4, y_pos, (j & 0x08)==0x08);
    }
		x = x+6;
	}
}

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
uint offset0 = pio_add_program(pio, &ntsc_sync_program);
// set up pio state machine
void pio_ntsc_sync_start(PIO pio, uint sm, uint offset, uint pin) {
    ntsc_sync_program_init(pio, sm, offset, pin);
    pio_sm_set_clkdiv (pio, sm, 294.0 ) ; // 294.2367
    // drain the  FIFOs
    pio_sm_clear_fifos (pio, sm);
    pio_sm_set_enabled(pio, sm, true);
}

//  pio assembler link
uint offset1 = pio_add_program(pio, &ntsc_data_program);
// set up pio state machine
void pio_ntsc_data_start(PIO pio, uint sm, uint offset, uint pin) {
    ntsc_data_program_init(pio, sm, offset, pin);
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
//
// serial thead
 void serial_function() {  

    // the usual sart message
    serial_port.printf("Starting ...PIO\r\n") ;  

    // === NTSC generation ===========
    // === start the PIO's
    // NOTE BENE! START PIO machines  BEFORE DMA channels
    #define gpio16 16
    #define machine0 0
    #define machine1 1
    // start sm's running
    pio_ntsc_sync_start(pio, machine0, offset0, gpio16);
    pio_ntsc_data_start(pio, machine1, offset1, gpio16);
    // give the state machine initial data 

  // 208 active lines to sync machine
  #define num_active_lines 207
  pio_sm_put_blocking (pio, machine0, num_active_lines);

   // === DMA channels setup ===================== //
    // TWO DMA channels to send data to sstate machine 1
  // ctrl chanel resets data channel read addr
  int ctrl_chan = dma_claim_unused_channel(true);
  // data channel move sine taable to PWM
  int data_chan = dma_claim_unused_channel(true);
  //
  ptr_table = (int)DMA_image_buffer;
  //
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
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
    channel_config_set_read_increment(&c2, true);
    channel_config_set_write_increment(&c2, false);
    channel_config_set_irq_quiet(&c2, true);
    channel_config_set_enable(&c2, true); 
    channel_config_set_chain_to(&c2, ctrl_chan) ;
    channel_config_set_dreq(&c2, DREQ_PIO0_TX1);
   // #define DREQ_TIMER0 0x3b
    //dma_hw->timer[0] = (0xffff | (0x0fff<<16) ) ;
   // channel_config_set_dreq(&c2, DREQ_TIMER0);
    //
    dma_channel_configure(data_chan, &c2, 
        &pio->txf[1], // write_addr, sm1 TX fifo pio0_hw
         DMA_image_buffer,  // read_addr, the table
        1664, // len(DMA_image_buffer), 1664
        1) ; // trigger

   // === 
   // wait a frame or two to sync up buffers
   ThisThread::sleep_for(30);
   erase_screen ;
   char video_text[] = "Cornell ece4760 rp2040 NTSC video \0" ;

   video_line(left, bottom, right, bottom, 1);
   video_line(left, top, right, top, 1);
   video_line(left, top, left, bottom, 1);
   video_line(right, top, right, bottom, 1);

   int t_start = time_us_32();
    for(int i=0; i<100; i++){
      for(int j=0; j<100; j++){
        video_pt(i+(j&1), j, i&1);
      }
    }
    //float dt = (float)(time_us_32()-t_start);
    //serial_port.printf("time time/point %f %f\r\n", dt, dt/(float)(100*100)) ;  

    //  test lines
    // void video_line(int x1, int y1, int x2, int y2, char c)
    for(int i=0; i<200; i+=10){
      video_line(128, 100, right, i, 1);
    }
    for(int i=110; i<160; i+=4){
      video_line(10, i, 60, i, 1);
    }
    for(int i=70; i<120; i+=3){
      video_line(i, 110, i, 160, 1);
    }

    // test strings
    // void video_string(int x, int y, char *str)
    video_string(5, 195, video_text);

    //float dt = (float)(time_us_32()-t_start);
    //serial_port.printf("time time/point %f %f\r\n", dt, dt/(float)(100*100)) ;  

   int pos = 120;
   while(1){ 
     video_line(110, pos, 150, pos, 0);
     if(pos>190) pos = 120;
       else pos++ ;
     video_line(110, pos, 150, pos, 1);
    ThisThread::sleep_for(16);
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