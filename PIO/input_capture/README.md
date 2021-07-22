Background
----------
The RP2020 has no "input capture" peripherial that uses hardare to grab a time stamp for
an externl event (edge on i/o pin). Both the AVR and PIC32 that I have used can capture times
in hardware, and I find it useful.
The PIO can be used to implement a fast timer/counter,
detect i/o pin edges, and log the time stamps at full bus rate to a 8-slot hardware FIFO. The FIFO
can then be read by the M0 core at some much slower rate. The implemented capture has a useful dynamic range 
from 10 MHz events down to a milliHertz. If the capture rate is slower than the thread execution rate, then
an indefinite number of captures is possible. If the capture rate is very high, then only the first 8 will be logged to
the FIFO, then the system will stall until the CPU reads the FIFO.

The code structure
------------------
There are two source files which are compiled using the Arduino 2.0beta IDE and a PIO assembler.
The C++ source file has a *.ino* extension
The assembler I have been using is a web version at  https://wokwi.com/tools/pioasm. PIOASM takes 
assembler source (of course) and also allows you to write some of the state machine C set up code in the same file. 
The output of the assembler is a C header file with an array representing the assembled PIO code, 
a couple of assembler-written C routines, and with the C code you specifed passed through to the header file.

There are therefore three files:   
1. the C++ source file (.ino),   
1. the PIO assembler source (.pio)   
1. the output of the PIO assembler (.h) which is included into the C++ source.  

The C program uses both  MBED and
C-SDK functions. MBED threads are used for multitasking and USB serial support. The PIO is initialized
and started using C-SDK low-level functions. The PIO itself runs a weird, stripped down assembly language, in which
each opcode may execute several related functions, but ALWAYS in one cycle (including conditional jumps).
There are four 32-bit registers: x, y, osr, and isr.  There are nine opcodes. Some of them used in this program:
* The *pull* opcode grabs a 32-bit value from the CPU output FIFO and loads it into the *osr* register.  
* The *mov* command does what you would expect, but can optionally logically invert or bit-reverse on move.
* The *set* opcode loads immediate data. In my program it sets i/o pin values directly.     
* The *in* opcode always operates on the isr register. It shifts in the number of bits you specify iteratively
into isr register. The C code sets the PIO configuration to autopush the isr into the CPU input FIFO to save one instruction.

As configured, the PIO state machine counts at 125 MHz, with an overhead of two cycles per timing event (easy to
compensate for). If the input is a 10 MHz square wave, there will be 12 or 13 counts recorded in each FIFO entry (barely
useful). If the input is a 1 KHz square wave there will be 125000 counts per event.
