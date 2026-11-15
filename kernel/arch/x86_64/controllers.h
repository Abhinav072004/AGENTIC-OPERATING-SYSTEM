#ifndef AGENTICOS_X86_CONTROLLERS_H
#define AGENTICOS_X86_CONTROLLERS_H
#include <stdbool.h>
#include <stdint.h>

#define PIC_VECTOR_BASE 32u
#define PIT_INPUT_HZ 1193182u
struct port_io {
    void *context;
    uint8_t (*read)(void *context, uint16_t port);
    void (*write)(void *context, uint16_t port, uint8_t value);
};
struct pic8259 { struct port_io io; uint16_t mask; bool ready; };
enum pic_delivery { PIC_INVALID, PIC_REAL, PIC_SPURIOUS_MASTER, PIC_SPURIOUS_SLAVE };
bool pic8259_init(struct pic8259 *pic, const struct port_io *io);
bool pic8259_mask(struct pic8259 *pic, uint16_t mask);
enum pic_delivery pic8259_delivery(struct pic8259 *pic, unsigned int irq);
bool pic8259_eoi(struct pic8259 *pic, unsigned int irq, enum pic_delivery delivery);
bool pit8254_program(const struct port_io *io, uint32_t hz, uint32_t *divisor);
#endif
