#ifndef TLB_H
#define TLB_H

#include "config.h"
#include "mlpt.h"

void tlb_clear();
int tlb_peek(size_t va);
size_t tlb_translate(size_t va);

#endif
