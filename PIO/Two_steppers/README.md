Background
---
Unipolar (5 or 6 wire) stepper motors require a 4-phase
sequence of pulses to rotate. Typically the job of producing the
pulse trains is put in a interrupt-service-routine on small controllers.
A single PIO i/o co-processor on RP2040 can produce the sequenced pulses for two motors, and count motor steps, to offload
the main cpu. The PIO unit waits for pulse-rate and sequence information from the
cpu, then produces a counted number of pulses at that rate. One of the PIO state machines counts steps and signals the stepper mahine to stop.
When signalled, the PIO finishes the curent sequence of either 4 full steps, or 8
half-steps, signals the cpu that it is ready, waits for new data, then starts the
new sequence. The new data consists of three 32-bit words. The first is the length of each stepper pulse
in machine cycles 125million/sec. The second is the desired sequence. A 32-bit word can contain 8
full-steps or 8 half steps. The third is the desired number of counts.


The code sructure
---
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
