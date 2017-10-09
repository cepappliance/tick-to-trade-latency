/**
 * Copyright 2017 CEPappliance, Inc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OrderBook.h"

OrderBook::OrderBook () {
    bid_price   = new int [TYPICAL_PX_LEVEL_COUNT];
    ask_price   = new int [TYPICAL_PX_LEVEL_COUNT];
    bid_size    = new int [TYPICAL_PX_LEVEL_COUNT];
    ask_size    = new int [TYPICAL_PX_LEVEL_COUNT];
    entry_price = new int [10000000];
    entry_size  = new int [10000000];
    int  i;

    for (i = 0; i < TYPICAL_PX_LEVEL_COUNT; i++) {
        bid_price [i] = 0;
        ask_price [i] = 0x7fffffff;
        bid_size  [i] = 0;
        ask_size  [i] = 0;
    }

    for (i = 0; i < 10000000; i ++) {
        entry_price [i] = 0;
        entry_size  [i] = 0;
    }
}

inline int lookup_bid_ix (int* prices, int price, int count) {
    int low = 0;
    int high = count - 2;

    while (low <= high) {
        int middle = (high + low) / 2;
        if (price > prices[middle]) {
            high = middle-1;
        } else if (price < prices[middle]) {
            low = middle+1;
        } else {
            low = middle;
            break;
        }
    }

    return low;
}

inline int lookup_ask_ix (int* prices, int price, int count) {
    int low = 0;
    int high = count - 2;

    while (low <= high) {
        int middle = (high + low) / 2;
        if (price < prices[middle]) {
            high = middle-1;
        } else if (price > prices[middle]) {
            low = middle+1;
        } else {
            low = middle;
            break;
        }
    }
    return low;
}

int OrderBook::processEntry(const char* md_entry_id, char md_entry_type, char md_update_action,
                             int md_price, int md_qty) {

    int rs = 0;

    int md_entry_id_int = (md_entry_id[0] & 0x0f) * 1000000 +
                          (md_entry_id[1] & 0x0f) * 100000 +
                          (md_entry_id[2] & 0x0f) * 10000 +
                          (md_entry_id[3] & 0x0f) * 1000 +
                          (md_entry_id[4] & 0x0f) * 100 +
                          (md_entry_id[5] & 0x0f) * 10 +
                          (md_entry_id[6] & 0x0f);


    int old_px   = entry_price [md_entry_id_int];
    int old_size = entry_size  [md_entry_id_int];

#ifdef DEBUG
    printf ("MD_ENTRY_ID:      %d \n",   md_entry_id_int);
    printf ("MD_ENTRY_TYPE:    %.2x \n", (unsigned char)md_entry_type);
    printf ("MD_UPDATE_ACTION: %.2x \n", (unsigned char)md_update_action);
    printf ("MD_ENTRY_PRICE:   %d \n",   md_price);
    printf ("MD_ENTRY_SIZE:    %d \n",   md_qty);
    printf ("OLD_SIZE:         %d \n",   old_size);
    printf ("-----------------------------------------\n");
#endif

    if (md_update_action == 0) {
        if (old_size != 0) {
            fprintf (stderr, " Error: Trying to add existing entry");
            exit (0);
        } else {
            rs = 1;
            entry_price[md_entry_id_int] = md_price;
            entry_size [md_entry_id_int] = md_qty;

            if (md_entry_type == '0') {
                int ix = lookup_bid_ix (bid_price, md_price, TYPICAL_PX_LEVEL_COUNT);
                if (bid_price[ix] == md_price) {
                    bid_size [ix] = bid_size [ix] + md_qty;
                } else {
                    memmove(&bid_price[ix+1], &bid_price[ix], (TYPICAL_PX_LEVEL_COUNT-1-ix)*sizeof(int));
                    memmove(&bid_size [ix+1], &bid_size [ix], (TYPICAL_PX_LEVEL_COUNT-1-ix)*sizeof(int));
                    bid_price[ix] = md_price;
                    bid_size [ix]  = md_qty;
                }
            } else if (md_entry_type == '1') {
                int ix = lookup_ask_ix (ask_price, md_price, TYPICAL_PX_LEVEL_COUNT);
                if (ask_price[ix] == md_price) {
                    ask_size [ix] = ask_size [ix] + md_qty;
                } else {
                    memmove(&ask_price[ix+1], &ask_price[ix], (TYPICAL_PX_LEVEL_COUNT-1-ix)*sizeof(int));
                    memmove(&ask_size [ix+1], &ask_size [ix], (TYPICAL_PX_LEVEL_COUNT-1-ix)*sizeof(int));
                    ask_price[ix] = md_price;
                    ask_size [ix] = md_qty;
                }
            }
        }
    } else if (md_update_action == 2) {

        if (old_size > 0) {
            rs = 1;
            entry_price[md_entry_id_int] = 0;
            entry_size [md_entry_id_int] = 0;
            if (md_entry_type == '0') {
                int ix = lookup_bid_ix (bid_price, old_px, TYPICAL_PX_LEVEL_COUNT);
                if (bid_price[ix] == old_px) {
                    bid_size [ix] = bid_size [ix] - old_size;
                    if (bid_size[ix] == 0) {
                        memmove(&bid_price[ix], &bid_price[ix+1], (TYPICAL_PX_LEVEL_COUNT-1-ix)*sizeof(int));
                        memmove(&bid_size [ix], &bid_size [ix+1], (TYPICAL_PX_LEVEL_COUNT-1-ix)*sizeof(int));
                        bid_price[TYPICAL_PX_LEVEL_COUNT-1] = 0;
                    }
                }
            } else if (md_entry_type == '1') {
                int ix = lookup_ask_ix (ask_price, old_px, TYPICAL_PX_LEVEL_COUNT);
                if (ask_price[ix] == old_px) {
                    ask_size [ix] = ask_size [ix] - old_size;
                    if (ask_size[ix] == 0) {
                        memmove(&ask_price[ix], &ask_price[ix+1], (TYPICAL_PX_LEVEL_COUNT-1-ix)*sizeof(int));
                        memmove(&ask_size [ix], &ask_size [ix+1], (TYPICAL_PX_LEVEL_COUNT-1-ix)*sizeof(int));
                        ask_price[TYPICAL_PX_LEVEL_COUNT-1] = 0x7fffffff;
                    }
                }
            }
        }
    } else if (md_update_action == 1) {
        if (old_size != 0) {
            rs = 1;
            entry_price[md_entry_id_int] = md_price;
            entry_size [md_entry_id_int] = md_qty;

            if (md_entry_type == '0') {
                int ix = lookup_bid_ix (bid_price, md_price, TYPICAL_PX_LEVEL_COUNT);
                if (bid_price[ix] == md_price) {
                    bid_size [ix] = bid_size [ix] + md_qty - old_size;
                }
            } else if (md_entry_type == '1') {
                int ix = lookup_ask_ix (ask_price, md_price, TYPICAL_PX_LEVEL_COUNT);
                if (ask_price[ix] == md_price) {
                    ask_size [ix] = ask_size [ix] + md_qty - old_size;
                }
            }
        }
    }

    return rs;
}

int OrderBook::topBidPrice (int ix) {
    return bid_price[ix];
}

int OrderBook::topAskPrice (int ix) {
    return ask_price[ix];
}

int OrderBook::topBidSize (int ix) {
    return bid_size[ix];
}

int OrderBook::topAskSize (int ix) {
    return ask_size[ix];
}
