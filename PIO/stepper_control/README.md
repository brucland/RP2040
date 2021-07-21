Unipolar (5 or 6 wire) stepper motors require a 4-phase
sequence of pulses to rotate. Typically the job of producing the
pulse trains is put in a interrupt-service-routine on small controllers.
The PIO i/o co-processor on RP2040 can produce the sequenced pulses to offload
the main cpu. The PIO unit waits for pulse-rate and sequence information from the
cpu, then produces an indefinite number of pulses at that rate, until signalled by the
cpu. When signalled, the PIO finishes the curent sequence of either 4 full steps, or 8
half-steps, signals the cpu that it is ready, waits for new data, then starts the
new sequence. 
