Using the PIO 
------------
Introduction: 
-------------
The PIO subsystem contains eight completely separate, small i/o state machines for fast, cycle accurate, 
i/o protocol generation. Examples might be extra SPI channels, VGA driver, DVI driver, pulse density modulation, 
or stepper motor sequencer. There is a nine-instruction assembly language used to program each PIO state machine. 
The instructions are Turing-complete, but not meant for general computation (e.g. don't use them to add 32-bit integers).
Each state machine has transmit-receive FIFOs which can read/written by the M0 cores, or by the DMA system. 
Each state machine can also read/write any of the GPIO pins. YOu can toggle an i/o pin as fast as 62 MHz, but you will not 
see such a fast signal if you are using a solderless breadboard.  

- PIO processor (there are 8)
From the documentation:

 * Each PIO is programmable in the same sense as a processor:
 *   the four state machines independently
 * execute short, sequential programs, to manipulate GPIOs and transfer data. Unlike a general
 * purpose processor, PIO state machines are highly specialised for IO, with a focus on determinism,
 * precise timing, and close integration with fixed-function hardware. Each state machine is equipped
 * with:
 *  * Two 32-bit shift registers: either direction, any shift count
 *  * Two 32-bit scratch registers
 *  * 4x32 bit bus FIFO in each direction (TX/RX), reconfigurable as 8x32 in a single direction
 *  * Fractional clock divider (16 integer, 8 fractional bits)
 *  * Flexible GPIO mapping
 *  * DMA interface, sustained throughput up to 1 word per clock from system DMA
 *  * IRQ flag set/clear/status

-- The assembly language:
------------------------
See C-SDK manual, section 3.4. Program memory is only 32 insetructions long, but each instruction can have several 
simultaneous effects, including a variable delay after execution (for pulse length trimming), the ability to
set/clear a group of pins(side-set), and, of course, the main opcode function. Some instructions can also be set up 
to auto-pull or auto-push the i/o FIFOs at the same time they perform other functions. An extra SPI channel takes 
5 instructions, PWM takes 7, VGA takes ~30.

-- Merging PIO code with C: PIOASM runs as a separate step in the default C-SDK make-process. 
The C compile details are hidden by the Arduino-MBED interface (good), but aso hides the ability to build PIO code (bad).
The solution is to use a stand-alone version of PIOASM. The version I have been using is a web version at 
https://wokwi.com/tools/pioasm. PIOASM takes assembler source (of course) and also allows you to write some of the 
state machine C set up code in the same file. The output of the assembler is a C header file with an array 
representing the assembled PIO code, a couple of assembler-written C routines, and with the C code you specifed 
passed through to the header file. 
