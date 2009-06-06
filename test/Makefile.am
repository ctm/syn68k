noinst_PROGRAMS = syngentest

syngentest_CPPFLAGS = -DMEMORY_OFFSET=8192

if M68K_HOSTCPU
  syngentest_SOURCES = main.c driver.c tests.c setup.c \
	testtrap.c callemulator.s run68k.s crc.c testrt.c testqsort.c
else
  syngentest_SOURCES = main.c driver.c tests.c setup.c \
	testtrap.c crc.c testrt.c testqsort.c
endif

nodist_syngentest_SOURCES = testbattery.c

testbattery.c: tests.c maketestbattery.pl
	rm -f testbattery.c
	$(srcdir)/maketestbattery.pl < $(srcdir)/tests.c > testbattery.c
	chmod a-w testbattery.c

syngentest_LDADD = ../runtime/libsyn68k.a

syngentest_LDFLAGS =

INCLUDES = -I$(srcdir)/include -I$(srcdir)/../include -I../include

CLEANFILES = testbattery.c

