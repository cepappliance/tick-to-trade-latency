/**
 * Copyright 2017 CEPappliance, Inc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "FastDecoder.h"
#include "OrderBook.h"

FastDecoder::FastDecoder () {
    m_exp_table = new int[256];
    memset (m_exp_table, 0, 256*sizeof(int));
    m_exp_table[0x7b] = 1;
    m_exp_table[0x7c] = 10;
    m_exp_table[0x7d] = 100;
    m_exp_table[0x7e] = 1000;
    m_exp_table[0x7f] = 10000;
    m_exp_table[0x01] = 100000;
    m_exp_table[0x02] = 1000000;
    m_exp_table[0x03] = 10000000;
    m_exp_table[0x04] = 100000000;
    m_exp_table[0x05] = 1000000000;
    m_seq_number = 0;
}

int FastDecoder::parsePacket (const unsigned char* packet, const int n, OrderBook* ob) {
    int           i     = 4;
    int           j     = 0;
    int           k     = 0;
    int           pmap  = 0;
    int           rs    = 0;
    int           apply = 0;
    char          exp   = 0;
    unsigned int  seq_number = packet[0] + (packet[1] << 8) + (packet [2] << 16);

    int           n_md_entries     = 0;
    int           md_price         = 0;
    int           md_qty           = 0;
    char          md_entry_type    = 0;
    char          md_update_action = 0;

    if (m_seq_number != 0) {
        if ((m_seq_number + 1) != seq_number) {
            fprintf (stderr, "Bad sequence number");
            exit (0);
        }
    }
    m_seq_number = seq_number;

#ifdef DEBUG
    printf ("Sequence number %d \n", m_seq_number);
#endif
    while (i < n) {
        buf2[j][k] = packet[i];
        if ((packet[i] & 0x80) != 0) {
#ifdef DEBUG
    printf ("data %s \n", buf2[j]);
#endif
            k = 0;
            j++;

            long long * buf_cast = (long long*) buf2[j];
            buf_cast[0] = 0;
            buf_cast[1] = 0;

        } else {
            k++;
        }
        i ++;
    }

    n_md_entries = (buf2[4][0] & 0xff) - 128;
    long long * buf_cast;
    int ix = 5;
    long long instr0 = 0;
    long long instr1 = 0;

    for (i = 0; i < n_md_entries; i ++) {

        if ((buf2[ix][0] & 0x80) != 0) {
            pmap = (((unsigned char)buf2[ix][0] & 0x7f) << 7);
        } else {
            pmap = (((unsigned char)buf2[ix][0] & 0x7f) << 7) + (((unsigned char)buf2[ix][1]) & 0x7f);
        }
#ifdef DEBUG
    printf ("pmap %.2x, %.2x \n", (unsigned char)buf2[ix][0], (unsigned char)buf2[ix][1]);
#endif
        ix++;
        if ((pmap & 0x2000) != 0) {
            md_update_action = (buf2[ix][0] & 0x7f) - 1;
            ix ++;
        }
        pmap = pmap << 1;
        if ((pmap & 0x2000) != 0) {
            md_entry_type = buf2[ix][0] & 0x7f;
            ix ++;
        }
        pmap = pmap << 1;

        buf_cast = (long long*) buf2[ix];

        *((long long*)md_entry_id)     = buf_cast[0];
        *(((long long*)md_entry_id)+1) = buf_cast[1];
        *(((long long*)md_entry_id)+2) = buf_cast[2];
        ix ++;


        if ((pmap & 0x2000) != 0) {
            buf_cast = (long long*) buf2[ix];
            instr0 = buf_cast[0];
            instr1 = buf_cast[1];

#ifdef DEBUG
            printf ("Instrument %s \n", buf2[ix]);
            printf ("Expected Instrument %s \n", ob->getSymbol());
#endif
            ix ++;
        }
        pmap = pmap << 1;

        const long long* ob_sym_cast = (const long long*)ob->getSymbol();
        if ((ob_sym_cast[0] == instr0) && (ob_sym_cast[1] == instr1)) {
            apply = 1;
        }

        // rpt seq
        ix ++;

//        ix += ((pmap & 0x2000) >> 13) + ((pmap & 0x1000) >> 12) + ((pmap & 0x800) >> 11);
//        pmap <= pmap << 3;

        if ((pmap & 0x2000) != 0) {
            // entry date
            ix ++;
        }
        pmap = pmap << 1;

        if ((pmap & 0x2000) != 0) {
            //entry time
            ix ++;
        }
        pmap = pmap << 1;

        if ((pmap & 0x2000) != 0) {
            // orig time
            ix ++;
        }
        pmap = pmap << 1;


        if ((pmap & 0x2000) != 0) {
            exp = buf2[ix][0] & 0x7f;
            if (exp != 0) {
                ix ++;
                md_price = buf2[ix][0] & 0x7f;
                int last = buf2[ix][0] & 0x80;
                if (last == 0) {
                    md_price = (md_price << 7) + (buf2[ix][1] & 0x7f);
                    last = buf2[ix][1] & 0x80;
                }
                if (last == 0) {
                    md_price = (md_price << 7) + (buf2[ix][2] & 0x7f);
                    last = buf2[ix][2] & 0x80;
                }
                if (last == 0) {
                    md_price = (md_price << 7) + (buf2[ix][3] & 0x7f);
                    last = buf2[ix][3] & 0x80;
                }
                md_price = md_price * m_exp_table[exp];
            }
            ix ++;
        }
        pmap = pmap << 1;

        if ((pmap & 0x2000) != 0) {
            exp = buf2[ix][0] & 0x7f;
            if (exp != 0) {
                ix ++;
                md_qty = buf2[ix][0] & 0x7f;
                int last = buf2[ix][0] & 0x80;
                if (last == 0) {
                    md_qty = (md_qty << 7) + (buf2[ix][1] & 0x7f);
                    last = buf2[ix][1] & 0x80;
                }
                if (last == 0) {
                    md_qty = (md_qty << 7) + (buf2[ix][2] & 0x7f);
                    last = buf2[ix][2] & 0x80;
                }
                if (last == 0) {
                    md_qty = (md_qty << 7) + (buf2[ix][3] & 0x7f);
                    last = buf2[ix][3] & 0x80;
                }
                md_qty = md_qty * m_exp_table[exp];
            }
            ix ++;
        }
        pmap = pmap << 1;

        if ((pmap & 0x2000) != 0) {
            // order status
            ix ++;
        }
        pmap = pmap << 1;

        if ((pmap & 0x2000) != 0) {
            // trading session status
            ix ++;
        }
        pmap = pmap << 1;

        if ((pmap & 0x2000) != 0) {
            // Trading session sub id
            ix ++;
        }
        if (apply) {

            int obrs = ob->processEntry(md_entry_id, md_entry_type, md_update_action, md_price, md_qty);
            if (obrs == 1) {
                rs = 1;
            }
        }
        apply = 0;
    }

    if (rs > 0) return n_md_entries;
    else return 0;
}
