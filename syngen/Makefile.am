CC=@CC_FOR_BUILD@
noinst_PROGRAMS = syngen

syngen_CPPFLAGS = -DSYNGEN

syngen_SOURCES = main.c token.c hash.c parse.c list.c reduce.c macro.c \
	boolean.c error.c defopcode.c bitstring.c generatecode.c \
	uniquestring.c 	byteorder.c \
\
        include/boolean.h include/error.h include/reduce.h include/parse.h \
        include/bitstring.h include/uniquestring.h include/hash.h \
	include/byteorder.h include/tokenlist.h include/common.h \
        include/macro.h include/generatecode.h include/list.h include/token.h \
        include/defopcode.h

INCLUDES = -I$(srcdir)/include -I$(srcdir)/../include -I../include

EXEEXT = 
