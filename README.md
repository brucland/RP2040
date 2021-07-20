# RP2040
Building an understanding of the Pi RP2040 by iterative programming and head-scratching.
---
I am a retired lecturer from Electrical and Computer Engineering at Cornell University where I taught hardware design courses
for over 20 years. I have taught using Atmel AVR, Microchip PIC32, and Altera/Intel Cyclone 2, 4 and 5 FPGAs.
The courses are now being taught by Van Hunter Adams. 
Last years web pages are at 
https://people.ece.cornell.edu/land/courses/ece4760/index.html
and
https://people.ece.cornell.edu/land/courses/ece5760/index.html.
For the last five years I also taught a technical writing course.
https://people.ece.cornell.edu/land/courses/ece4920/index.html
Now Hunter and I are looking for the next architecture to use for the microcontroller course.

We have been experimenting with the C-SDK, MBED/Arduino, and MIcroPython. After messing with 
MicroPython for a few weeks, I switched to the MBED/Arduino environment which allows free intermixing
of MBED and C-SDK constructs. The resulting programs have NO Arduino syntax (except for the ino extension)
but use the high-level MBED RTOS, mixed with C-SDK, on core0, and only the C-SDK on core1.
