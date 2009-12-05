CC=@CC_FOR_BUILD@
noinst_PROGRAMS = makebucket profile
EXEEXT = 

BUILT_SOURCES = bucket.c

profile_SOURCES = main.c readprofile.c amode.c frequency.c \
                  include/amode.h include/bucket.h \
                  include/frequency.h include/readprofile.h

nodist_profile_SOURCES = bucket.c

makebucket_SOURCES = makebucket.c amode.c \
                     include/amode.h include/bucket.h


AM_CFLAGS = -g -Wall -I$(srcdir)/include -I$(srcdir)/../include

all:
	echo "Doing nothing, we don't use this any more"

bucket.c: makebucket ../runtime/profileinfo.gz
	./makebucket ../runtime/profileinfo.gz > bucket.c

clean-local:
	-rm -f bucket.c
