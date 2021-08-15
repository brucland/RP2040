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

For a robot or plotter you need at least two motors. Using space-optimized PIO programs, I fit two drivers and step counters into one PIO. You could put two more motors on the other PIO. Two motors use a total of 10 pins. 4 for each motor, and one each for count-SM to stepper-SM signaling. With no load the motors draw around 0.5 amp total when running in half-step mode (better torque, more current). Using the 28BYJ-48 stepper and its ULN2003 driver board, GPIO 6 to 9 are connected to motor controller in1 to in4 on the first motor. GPIO 10 to 13 are connected to motor controller in1 to in4 on the second motor. GPIO 5 and 14 are used for signalling between state machines and should not have an external connection.  

The full step sequences turn on one phase at a time in one of two patterns for forward and reverse. In terms of the motor input lines the sequences are 1-2-3-4-1-2-3-4 or 4-3-2-1-4-3-2-1. These are encoded in a single 32-bit word as 0x12481248 and 0x84218421. The half step sequences turn on one or two consecutive pins in an overlapping pattern. The sequences are 1-12-2-23-3-34-4-41 and 41-4-34-3-23-2-12-1, encoded as 0x13264c89 and 0x98c46231. The 'stop' pattern, 0x80808080, just toggles the pin that the counter sate machine uses. This allows one (or both) motors to stop while still being timed by the counter state machine. The program is initialized to stop pattern.

The code sructure
---
There are two source files which are compiled using the Arduino 2.0beta IDE and a PIO assembler.
The revised source files are a bit ponderous because of the state machine initialization routines. All that main does is to start all of the state machines, ask the user for parameters, then do nothing until the sequence of steps is finished.
The C++ source file has a *.ino* extension.
The assembler I have been using is a web version at  https://wokwi.com/tools/pioasm. PIOASM takes 
assembler source (of course) and also allows you to write some of the state machine C set up code in the same file. 
The output of the assembler is a C header file with an array representing the assembled PIO code, 
a couple of assembler-written C routines, and with the C code you specifed passed through to the header file.

There are therefore three files:   
1. the C++ source file (.ino),   
1. the PIO assembler source (.pio)   
1. the output of the PIO assembler (.h) which is included into the C++ source.  
