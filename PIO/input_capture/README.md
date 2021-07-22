Background
----------
The RP2020 has no "input capture" peripherial that uses hardare to grab a time stamp for
an externl event (edge on i/o pin). Both the AVR and PIC32 that I have used can capture times
in hardware, and I find it useful.
The PIO can be used to implement a fast timer/counter,
detect i/o pin edges, and log the time stamps at full bus rate to a 8-slot hardware FIFO. The FIFO
can then be read by the M0 core at some much slower rate.

The code structure
------------------

