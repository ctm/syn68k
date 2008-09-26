CC=@CC_FOR_BUILD@
noinst_PROGRAMS = makebucket profile
EXEEXT = 

profile_SOURCES = bucket.c main.c readprofile.c amode.c frequency.c

makebucket_SOURCES = makebucket.c amode.c

AM_CFLAGS = -g -Wall -I$(srcdir)/include -I$(srcdir)/../include

all:
	echo "Doing nothing, we don't use this any more"

bucket.c: makebucket ../runtime/profileinfo.gz
	./makebucket ../runtime/profileinfo.gz > bucket.c
