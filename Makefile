#
# Students' Makefile for the Malloc Lab
#
TEAM = bovik
VERSION = 1
HANDINDIR = /afs/cs.cmu.edu/academic/class/15213-f01/malloclab/handin

CC = gcc
CFLAGS = -Wall -O0 -g -m32

SUFFIX ?= implicit

OBJS = mdriver.o mm_$(SUFFIX).o memlib.o fsecs.o fcyc.o clock.o ftimer.o

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm_$(SUFFIX).o: mm_$(SUFFIX).c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h 

handin:
	cp mm_$(SUFFIX).c $(HANDINDIR)/$(TEAM)-$(VERSION)-mm_$(SUFFIX).c

clean:
	rm -f *~ *.o mdriver

imp:
	$(MAKE) SUFFIX=implicit
exp:
	$(MAKE) SUFFIX=explicit
seg:
	$(MAKE) SUFFIX=segreg
bud:
	$(MAKE) SUFFIX=buddy