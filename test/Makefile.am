noinst_PROGRAMS = syngentest

syngentest_CPPFLAGS = -DMEMORY_OFFSET=8192

  syngentest_SOURCES = main.c driver.c tests.c setup.c \
	testtrap.c crc.c testrt.c testqsort.c \
\
        include/callemulator.h include/crc.h include/driver.h \
        include/run68k.h include/setup.h include/testbattery.h \
	include/testqsort.h include/testrt.h include/testtrap.h \
\
        maketestbattery.pl testall.sh

if M68K_HOSTCPU
  syngentest_SOURCES += callemulator.s run68k.s
endif

nodist_syngentest_SOURCES = testbattery.c

testbattery.c: tests.c maketestbattery.pl
	rm -f testbattery.c
	$(srcdir)/maketestbattery.pl < $(srcdir)/tests.c > testbattery.c
	chmod a-w testbattery.c

syngentest_LDADD = ../runtime/libsyn68k.a

syngentest_LDFLAGS = @EXECSTACK@

INCLUDES = -I$(srcdir)/include -I$(srcdir)/../include -I../include

CLEANFILES = testbattery.c

