/*
** Copyright 2005-2017  Solarflare Communications Inc.
**                      7505 Irvine Center Drive, Irvine, CA 92618, USA
** Copyright 2002-2005  Level 5 Networks Inc.
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of version 2 of the GNU General Public License as
** published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

/*
 * TCPDirect sample application demonstrating low-latency UDP sends and
 * receives.
 */
#include <zf/zf.h>
#include "zf_utils.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>
#include <inttypes.h>
#include <netinet/tcp.h>
#include "OrderBook.h"
#include "FastDecoder.h"


#define FIX_PRICE_BASE 156
#define PX_LEVELS 990000


static void usage_msg(FILE* f)
{
  fprintf(f, "usage:\n");
  fprintf(f, "  tcpdirect <ponger:port> <pinger:port>\n");
  fprintf(f, "\n");
  fprintf(f, "options:\n");
  fprintf(f, "  -i number of iterations\n");
  fprintf(f, "\n");
}


static void usage_err(void)
{
  usage_msg(stderr);
  exit(1);
}


struct cfg {
  int itercount;
};


static struct cfg cfg = {
  .itercount = 1000000,
};

void tableInit (char* table) {

  for (int i = 0; i < PX_LEVELS; i++) {
    double x = i*0.0001;
    sprintf(table+(i*8), "%07.4f",x);
    table[i*8+7] = '0';
  }
}

inline void setNewPrice (char* data, int price, const char* table) {
  long long* tableptr = (long long*)table;
  long long* adr      = (long long*)(&data[FIX_PRICE_BASE]);
  adr[0] = tableptr[price];
}




static void ping_pongs(struct zf_stack* stack, struct zfur* ur, struct zft* tt)
{
  char               fixmsg[512];
  char*              table       = new char[PX_LEVELS*8];
  OrderBook*         ob          = new OrderBook();
  FastDecoder*       decoder     = new FastDecoder();
  int len;

  int recvs_left = cfg.itercount;
  struct {
    struct zfur_msg msg;
    struct iovec iov[2];
  } msg;
  const int max_iov = sizeof(msg.iov) / sizeof(msg.iov[0]);

  tableInit (table);

  memset (fixmsg, 0, 512);

  strcpy (fixmsg, "8=FIX.4.4|9=00232|35=G|49=MD0000000000|56=000000000|34=20|52=20170325-09:00:22|1=MB0000000000|11=00000//000000|37=00000000000|38=1|40=2|41=00000//000000|44=57.297500|55=USD000UTSTOM|60=20170325-09:00:21|386=13|36=CETS|453=1|448=00000|447=D|452=3|10=122\n\0");
  len = strlen(fixmsg);

  do {
    /* Poll the stack until something happens. */
    while( zf_reactor_perform(stack) == 0 )
      ;
    msg.msg.iovcnt = max_iov;
    zfur_zc_recv(ur, &msg.msg, 0);
    if( msg.msg.iovcnt == 0 )
      continue;

    int events = decoder->parsePacket ((const unsigned char*)msg.msg.iov[0].iov_base, msg.msg.iov[0].iov_len, ob);

    if (events > 0) {
      setNewPrice (fixmsg, (ob->topAskPrice(0) + ob->topAskPrice(0))/2, (const char*)table);

      ZF_TEST(zft_send_single(tt, fixmsg, len, 0) == len);
    }

    /* The current implementation of TCPDirect always returns a single
     * buffer for each datagram.  Future implementations may return
     * multiple buffers for large (jumbo) or fragmented datagrams.
     */
    ZF_TEST(msg.msg.iovcnt == 1);
    /* As we're doing a ping-pong we shouldn't ever see any more datagrams
     * queued!
     */
    ZF_TEST(msg.msg.dgrams_left == 0);
    zfur_zc_recv_done(ur, &msg.msg);
    --recvs_left;
  } while( recvs_left );

}


static void ponger(struct zf_stack* stack, struct zfur* ur, struct zft* tt)
{
  ping_pongs(stack, ur, tt);
}



int main(int argc, char* argv[])
{
  int c;
  while( (c = getopt(argc, argv, "i:")) != -1 )
    switch( c ) {
    case 'i':
      cfg.itercount = atoi(optarg);
      break;
    case '?':
      exit(1);
    default:
      ZF_TEST(0);
    }

  argc -= optind;
  argv += optind;
  if( argc != 2 )
    usage_err();

  struct addrinfo *ai_local, *ai_remote;
  if( getaddrinfo_hostport(argv[0], NULL, &ai_local) != 0 ) {
    fprintf(stderr, "ERROR: failed to lookup address '%s'\n", argv[0]);
    exit(2);
  }
  if( getaddrinfo_hostport(argv[1], NULL, &ai_remote) != 0 ) {
    fprintf(stderr, "ERROR: failed to lookup address '%s'\n", argv[1]);
    exit(2);
  }

  /* Initialise the TCPDirect library and allocate a stack. */
  ZF_TRY(zf_init());

  struct zf_attr* attr;
  ZF_TRY(zf_attr_alloc(&attr));

  struct zf_stack* stack;
  ZF_TRY(zf_stack_alloc(attr, &stack));

  /* Allocate zockets and bind them to the given addresses.  TCPDirect has
   * separate objects for sending and receiving UDP datagrams.
   */
  struct zfur* ur;
  ZF_TRY(zfur_alloc(&ur, stack, attr));
  ZF_TRY(zfur_addr_bind(ur, ai_local->ai_addr, ai_local->ai_addrlen,
                        ai_remote->ai_addr, ai_remote->ai_addrlen, 0));

  struct zft_handle* tcp_handle;
  struct zft* tt;

  ZF_TRY(zft_alloc(stack, attr, &tcp_handle));
  ZF_TRY(zft_connect(tcp_handle, ai_remote->ai_addr, ai_remote->ai_addrlen, &tt));

  while( zft_state(tt) == TCP_SYN_SENT )
    zf_reactor_perform(stack);

  ZF_TEST( zft_state(tt) == TCP_ESTABLISHED );

  ponger(stack, ur, tt);

  return 0;
}
