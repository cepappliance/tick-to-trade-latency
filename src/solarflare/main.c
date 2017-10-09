/**
 * Copyright 2017 CEPappliance, Inc.
 */

#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "OrderBook.h"
#include "FastDecoder.h"

#define FIX_PRICE_BASE 156
#define PX_LEVELS 9900000

static inline void time_cntr(uint64_t* pval) {
  uint64_t low, high;
  __asm__ __volatile__("rdtsc" : "=a" (low) , "=d" (high));
  *pval = (high << 32) | low;
}

void error (const char* errstr) {
    fprintf(stderr, "%s", errstr);
    exit(0);
}

void tableInit (char* table) {

    for (int i = 0; i < PX_LEVELS; i++) {
        double x = i*0.00001;
        sprintf(table+(i*8), "%08.5f",x);
//        table[i*8+7] = '0';
    }
}

inline void setNewPrice (char* data, int price, const char* table) {
    long long* tableptr = (long long*)table;
    long long* adr      = (long long*)(&data[FIX_PRICE_BASE]);
    adr[0] = tableptr[price];
}

int main (int argc, const char* argv[]) {

    int                rxportno    = 16001;
    int                txportno    = 6789;
    int                sockfd      = 0;
    int                udpfd       = 0;
    int                n           = 0;
    struct sockaddr_in fast_addr;
    struct sockaddr_in serv_addr;
    struct hostent*    server      = NULL;
    unsigned char      packet[1536];
    char               fixmsg[512];
    char*              table       = new char[PX_LEVELS*8];
    OrderBook*         ob          = new OrderBook();
    FastDecoder*       decoder     = new FastDecoder();

    tableInit (table);

    memset (fixmsg, 0, 512);
    strcpy (fixmsg, "8=FIX.4.4|9=00232|35=G|49=MD0000000000|56=000000000|34=20|52=20170325-09:00:22|1=MB0000000000|11=00000//000000|37=00000000000|38=1|40=2|41=00000//000000|44=57.297500|55=USD000UTSTOM|60=20170325-09:00:21|386=13|36=CETS|453=1|448=00000|447=D|452=3|10=122\n\0");

    if (argc < 3) {
       fprintf(stderr,"usage %s hostname udp_port\n", argv[0]);
       exit(0);
    }

    rxportno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening tcp socket");

    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) error("ERROR opening udp socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) error ("ERROR, no such host\n");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(txportno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        error ("ERROR, no such host\n");
    }

    memset((char *)&fast_addr, 0, sizeof(fast_addr));
    fast_addr.sin_family = AF_INET;
    fast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    fast_addr.sin_port = htons(rxportno);
    if (bind(udpfd, (struct sockaddr *)&fast_addr, sizeof(fast_addr)) < 0) {
        error("FAST bind failed");
    }

    printf ("Started! \n");

    timespec ts1;
    timespec ts2;
    int i = 0;
    int oldPx = 0;
    while (i < 100000) {
        n = read(udpfd,packet,1536);

        clock_gettime(CLOCK_REALTIME, &ts1);
#ifdef DEBUG
        printf ("New packet, len %d\n", n);
#endif
        int events = decoder->parsePacket (packet, n, ob);
        int bid = ob->topBidPrice(0);
        int ask = ob->topAskPrice(0);
        if (bid > 9900000) bid = 9900000;
        if (ask > 9900000) ask = 9900000;
#ifdef DEBUG
        if (events > 0) printf ("Orderbook events %d  {%d, %d}  \n", events, ask, bid);
#endif

        int newPx = bid+ask;

//        printf ("0  [%d @ %d], [%d @ %d]\n", ob->topAskSize(0)/10000, ob->topAskPrice(0), ob->topBidSize(0)/10000, ob->topBidPrice(0));
//        printf ("1  [%d @ %d], [%d @ %d]\n", ob->topAskSize(1)/10000, ob->topAskPrice(1), ob->topBidSize(1)/10000, ob->topBidPrice(1));
//        printf ("2  [%d @ %d], [%d @ %d]\n", ob->topAskSize(2)/10000, ob->topAskPrice(2), ob->topBidSize(2)/10000, ob->topBidPrice(2));
//            printf ("3  [%d @ %d], [%d @ %d]\n", ob->topAskSize(3)/10000, ob->topAskPrice(3), ob->topBidSize(3)/10000, ob->topBidPrice(3));

        if ((events > 0) && (newPx != oldPx)) { //
            setNewPrice (fixmsg, bid, (const char*)table);
            oldPx = newPx;

            clock_gettime(CLOCK_REALTIME, &ts2);


            n = write(sockfd, fixmsg, strlen(fixmsg));
            if (n < 0) error("ERROR writing to socket");

            printf ("ENTRIES: %d TIME: %ld  PX:%d SEQ:%d\n", events, (ts2.tv_nsec - ts1.tv_nsec), bid, decoder->getSeqNumber());
            i++;
        }
    }

    close(sockfd);
}


