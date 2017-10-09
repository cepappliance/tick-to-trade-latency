/**
 * Copyright 2017 CEPappliance, Inc.
 */

#ifndef FAST_DECODER_H
#define FAST_DECODER_H
#include "OrderBook.h"

#define NO_MD_ENTRIES    4
#define MD_ENTRY_PMAP    5
#define MD_UPDATE_ACTION 6
#define MD_ENTRY_TYPE    7
#define MD_ENTRY_ID      8
#define MD_SYMBOL        9
#define MD_ENTRY_PX_1    14
#define MD_ENTRY_PX_2    15
#define MD_ENTRY_SIZE_1  16
#define MD_ENTRY_SIZE_2  17

class FastDecoder {
    private:
    int*         m_exp_table;
    unsigned int m_seq_number;
    const char   m_pmap_mask[21] = {0,0,0,0,0, 0,1,1,0,1,0,1,1,1,1,1,1,1,1,1,1};
    char         md_entry_id[24];
    char         buf[256];
    char         buf2[256][64];

    public:
    FastDecoder ();
    int parsePacket (const unsigned char* packet, const int n, OrderBook* ob);
    unsigned int getSeqNumber () {return m_seq_number;};
};
#endif
