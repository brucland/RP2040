// -------------------------------------------------- //
// This file is autogenerated by pioasm; do not edit! //
// -------------------------------------------------- //

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// ------- //
// stepper //
// ------- //

#define stepper_wrap_target 3
#define stepper_wrap 8

static const uint16_t stepper_program_instructions[] = {
    0xc010, //  0: irq    nowait 0 rel               
    0x60c0, //  1: out    isr, 32                    
    0x6040, //  2: out    y, 32                      
            //     .wrap_target
    0x00e6, //  3: jmp    !osre, 6                   
    0x00c0, //  4: jmp    pin, 0                     
    0xa0e2, //  5: mov    osr, y                     
    0x6004, //  6: out    pins, 4                    
    0xa026, //  7: mov    x, isr                     
    0x0048, //  8: jmp    x--, 8                     
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program stepper_program = {
    .instructions = stepper_program_instructions,
    .length = 9,
    .origin = -1,
};

static inline pio_sm_config stepper_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + stepper_wrap_target, offset + stepper_wrap);
    return c;
}
#endif

// ------------ //
// step_counter //
// ------------ //

#define step_counter_wrap_target 0
#define step_counter_wrap 4

static const uint16_t step_counter_program_instructions[] = {
            //     .wrap_target
    0x7820, //  0: out    x, 32           side 1     
    0x1020, //  1: jmp    !x, 0           side 0     
    0x2009, //  2: wait   0 gpio, 9                  
    0x2089, //  3: wait   1 gpio, 9                  
    0x0042, //  4: jmp    x--, 2                     
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program step_counter_program = {
    .instructions = step_counter_program_instructions,
    .length = 5,
    .origin = -1,
};

static inline pio_sm_config step_counter_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + step_counter_wrap_target, offset + step_counter_wrap);
    sm_config_set_sideset(&c, 2, true, false);
    return c;
}
#endif

// ------------- //
// step_counter1 //
// ------------- //

#define step_counter1_wrap_target 0
#define step_counter1_wrap 4

static const uint16_t step_counter1_program_instructions[] = {
            //     .wrap_target
    0x7820, //  0: out    x, 32           side 1     
    0x1020, //  1: jmp    !x, 0           side 0     
    0x200d, //  2: wait   0 gpio, 13                 
    0x208d, //  3: wait   1 gpio, 13                 
    0x0042, //  4: jmp    x--, 2                     
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program step_counter1_program = {
    .instructions = step_counter1_program_instructions,
    .length = 5,
    .origin = -1,
};

static inline pio_sm_config step_counter1_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + step_counter1_wrap_target, offset + step_counter1_wrap);
    sm_config_set_sideset(&c, 2, true, false);
    return c;
}

// ======================================================
// === stepper driver 0 config ===
// ======================================================
void stepper0_program_init(PIO pio, uint sm, uint offset, uint pin) {
   pio_gpio_init(pio, pin);
   pio_gpio_init(pio, pin+1);
   pio_gpio_init(pio, pin+2);
   pio_gpio_init(pio, pin+3);
   //
   // the step outputs
   pio_sm_set_out_pins (pio, sm, pin, 4) ;
   pio_sm_set_consecutive_pindirs(pio, sm, pin, 4, true); 
   //
   pio_sm_config c = stepper_program_get_default_config(offset);
   //
   // Using 'out' 
   sm_config_set_out_pins(&c, pin, 4);
   //
   //JMP pin is specified separately as GPIO #, GPIO 5
   // driven by signal from other sm
   sm_config_set_jmp_pin (&c, 5) ;
   //
   // the out FIFO, shift right,  autopull, threshold 31
   sm_config_set_out_shift (&c, true, true, 31) ;
   //
   pio_sm_init(pio, sm, offset, &c);
}
// ======================================================
// === stepper driver 1 config ===
// ======================================================
void stepper1_program_init(PIO pio, uint sm, uint offset, uint pin) {
   pio_gpio_init(pio, pin);
   pio_gpio_init(pio, pin+1);
   pio_gpio_init(pio, pin+2);
   pio_gpio_init(pio, pin+3);
   //
   // the step outputs
   pio_sm_set_out_pins (pio, sm, pin, 4) ;
   pio_sm_set_consecutive_pindirs(pio, sm, pin, 4, true); 
   //
   pio_sm_config c = stepper_program_get_default_config(offset);
   //
   // Using 'out' 
   sm_config_set_out_pins(&c, pin, 4);
   //
   //JMP pin is specified separately as GPIO #, GPIO 14
   // driven by signal from other sm
   sm_config_set_jmp_pin (&c, 14) ;
   //
   // the out FIFO, shift right,  autopull, threshold 31
   sm_config_set_out_shift (&c, true, true, 31) ;
   //
   pio_sm_init(pio, sm, offset, &c);
}
// ======================================================
// === stepper counter 0 config ===
// ======================================================
void step_counter0_program_init(PIO pio, uint sm, uint offset, uint pin) {
   pio_gpio_init(pio, 5); // signal pin 
   pio_sm_set_consecutive_pindirs(pio, sm, 5, 1, true); // signal done output 
   //
   pio_sm_config c = step_counter_program_get_default_config(offset);
   //
   // one pin, optional side, not pindir
   sm_config_set_sideset (&c, 2, true, false);
   sm_config_set_sideset_pins(&c, 5); // signal stepper machine
   //bool shift_right, bool autopull, uint pull_threshold)
   sm_config_set_out_shift (&c, true, true, 31) ;
   //
   pio_sm_init(pio, sm, offset, &c);
}
// ======================================================
// === stepper counter 1 config ===
// ======================================================
void step_counter1_program_init(PIO pio, uint sm, uint offset, uint pin) {
   pio_gpio_init(pio, 14); // signal pin 
   pio_sm_set_consecutive_pindirs(pio, sm, 14, 1, true); // signal done output 
   //
   pio_sm_config c = step_counter1_program_get_default_config(offset);
   //
   // one pin, optional side, not pindir
   sm_config_set_sideset (&c, 2, true, false);
   sm_config_set_sideset_pins(&c, 14); // signal stepper machine
   //bool shift_right, bool autopull, uint pull_threshold)
   sm_config_set_out_shift (&c, true, true, 31) ;
   //
   pio_sm_init(pio, sm, offset, &c);
}
// ======================================================
//

#endif
