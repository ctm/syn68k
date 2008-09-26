CC=@CC_FOR_BUILD@
noinst_PROGRAMS = syngen

syngen_CPPFLAGS = -DSYNGEN

syngen_SOURCES = main.c token.c hash.c parse.c list.c reduce.c macro.c \
	boolean.c error.c defopcode.c bitstring.c generatecode.c \
	uniquestring.c 	byteorder.c

INCLUDES = -I$(srcdir)/include -I$(srcdir)/../include -I../include

EXEEXT = 
