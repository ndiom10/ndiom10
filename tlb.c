#include "tlb.h"
#include <stddef.h>

#define TLB_SETS 16
#define TLB_WAYS 4

typedef struct {
    int valid;
    size_t vpn;
    size_t ppn_base;
    unsigned long long last_used;
} tlb_line_t;

static tlb_line_t tlb[TLB_SETS][TLB_WAYS];
static unsigned long long use_clock = 1;

static size_t page_offset(size_t va) {
    return va & ((((size_t)1) << POBITS) - 1);
}

static size_t vpn_of(size_t va) {
    return va >> POBITS;
}

static size_t page_base(size_t va) {
    return va & ~((((size_t)1) << POBITS) - 1);
}

static int set_of(size_t vpn) {
    return (int)(vpn & (TLB_SETS - 1));
}

void tlb_clear() {
    for (int s = 0; s < TLB_SETS; s++) {
        for (int w = 0; w < TLB_WAYS; w++) {
            tlb[s][w].valid = 0;
            tlb[s][w].vpn = 0;
            tlb[s][w].ppn_base = 0;
            tlb[s][w].last_used = 0;
        }
    }
    use_clock = 1;
}

int tlb_peek(size_t va) {
    size_t vpn = vpn_of(va);
    int set = set_of(vpn);

    for (int w = 0; w < TLB_WAYS; w++) {
        if (tlb[set][w].valid && tlb[set][w].vpn == vpn) {
            int rank = 1;
            for (int k = 0; k < TLB_WAYS; k++) {
                if (tlb[set][k].valid && tlb[set][k].last_used > tlb[set][w].last_used) {
                    rank++;
                }
            }
            return rank;
        }
    }

    return 0;
}

size_t tlb_translate(size_t va) {
    size_t vpn = vpn_of(va);
    size_t offset = page_offset(va);
    int set = set_of(vpn);

    for (int w = 0; w < TLB_WAYS; w++) {
        if (tlb[set][w].valid && tlb[set][w].vpn == vpn) {
            tlb[set][w].last_used = use_clock++;
            return tlb[set][w].ppn_base | offset;
        }
    }

    size_t pa_base = translate(page_base(va));
    if (pa_base == (size_t)-1) {
        return (size_t)-1;
    }

    int victim = -1;
    for (int w = 0; w < TLB_WAYS; w++) {
        if (!tlb[set][w].valid) {
            victim = w;
            break;
        }
    }

    if (victim == -1) {
        victim = 0;
        for (int w = 1; w < TLB_WAYS; w++) {
            if (tlb[set][w].last_used < tlb[set][victim].last_used) {
                victim = w;
            }
        }
    }

    tlb[set][victim].valid = 1;
    tlb[set][victim].vpn = vpn;
    tlb[set][victim].ppn_base = pa_base;
    tlb[set][victim].last_used = use_clock++;

    return pa_base | offset;
}
