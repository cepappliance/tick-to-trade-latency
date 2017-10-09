/**
 * Copyright 2017 CEPappliance, Inc.
 */

#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

class OrderBook {
    private:
//    const unsigned char sym [16] = {0x47,0x42,0x50,0x52,0x55,0x42,0x5f,0x54,0x4f,0xcd,0x0,0x0,0x0,0x0,0x0,0x0};
    const unsigned char sym [16] = {0x55,0x53,0x44,0x30,0x30,0x30,0x30,0x30,0x30,0x54,0x4f,0xc4,0x0,0x0,0x0,0x0};
    int* bid_price;
    int* ask_price;
    int* bid_size;
    int* ask_size;
    int* entry_size;
    int* entry_price;

    public:
    #define TYPICAL_PX_LEVEL_COUNT 256

    OrderBook ();

    int processEntry(const char* md_entry_id,
                      char md_entry_type,
                      char md_update_action,
                      int  md_price,
                      int  md_qty);
    int topBidPrice (int ix);
    int topAskPrice (int ix);
    int topBidSize  (int ix);
    int topAskSize  (int ix);
    const char* getSymbol () {return (const char*)sym;}
};
#endif
