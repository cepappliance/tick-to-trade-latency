#
#  Copyright 2017 CEPappliance, Inc.

all:
	g++ -I. -DNDEBUG -m64 -mtune=native -Wall -Wundef -Wpointer-arith -fomit-frame-pointer -std=gnu++11 -O2 -c main.cc -o main.o
	g++ -std=gnu++11 -O2 -c FastDecoder.cc -o FastDecoder.o
	g++ -std=gnu++11 -O2 -c OrderBook.cc -o OrderBook.o
	g++ -m64 -mtune=native -O2 main.o FastDecoder.o OrderBook.o prebuilt/libonload_zf_static.a -o tcpdirect

clean:
	rm -f *.o
	rm tcpdirect
