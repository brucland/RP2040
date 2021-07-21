
Background
---
Unipolar (5 or 6 wire) stepper motors require a 4-phase
sequence of pulses to rotate. Typically the job of producing the
pulse trains is put in a interrupt-service-routine on small controllers.
The PIO i/o co-processor on RP2040 can produce the sequenced pulses to offload
the main cpu. The PIO unit waits for pulse-rate and sequence information from the
cpu, then produces an indefinite number of pulses at that rate, until signalled by the
cpu. When signalled, the PIO finishes the curent sequence of either 4 full steps, or 8
half-steps, signals the cpu that it is ready, waits for new data, then starts the
new sequence.

The code sructure
---
There are two source files which are compiled using the Arduino 2.0beta IDE and a PIO assembler.
The C++ source file has a *.ino* extension
The assembler I have been using is a web version at  https://wokwi.com/tools/pioasm. PIOASM takes 
assembler source (of course) and also allows you to write some of the state machine C set up code in the same file. 
The output of the assembler is a C header file with an array representing the assembled PIO code, 
a couple of assembler-written C routines, and with the C code you specifed passed through to the header file.

There are therefore three files: (1) the C++ source file (.ino), (2) the PIO assembler source (.pio) 
(3) the output of the PIO assembler (.h) which is included into the C++ source. The C program uses MBED and
C-SDK to implemnet funcitons. MBED threads are used for multitasking and USB serial support. The PIO is initialized
and started using C-SDK low-level functions.
