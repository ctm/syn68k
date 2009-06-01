host_native=@HOST_NATIVE@

SYN68K_CFLAGS=@SYN68K_CFLAGS@
CLEANUP=@CLEANUP@
OPTIMIZE=@OPTIMIZE@

CFLAGS += -DRUNTIME

LOCAL_INCLUDES =

LOCAL_INCLUDES += -I$(srcdir)/include \
  -I$(srcdir)/../include -I$(srcdir) -I../include

LOCAL_CFLAGS = -DRUNTIME -Iinclude

all: libsyn68k.a

.c.o:
	$(CC) $(CFLAGS) -c $(LOCAL_INCLUDES) $< -o $@

OBJS =	block.o diagnostics.o hash.o rangetree.o translate.o alloc.o	\
	blockinfo.o trap.o destroyblock.o callback.o init.o interrupt.o	\
	profile.o dosinterrupts.o deathqueue.o checksum.o native.o	\
	backpatch.o recompile.o						\
	mapindex.o mapinfo.o syn68k.o

mapinfo.o:	$(host_native)/host-xlate.h

$(host_native)/host-xlate.h:
	$(MAKE) -C $(host_native) host-xlate.h

.PHONY: $(host_native)/subdir-stmp

$(host_native)/subdir-stmp:
	$(MAKE) -C $(host_native) subdir-stmp

libsyn68k.a: $(OBJS) $(host_native)/subdir-stmp
	$(RM) libsyn68k.a
	$(AR) cq $@ $(OBJS) 				\
                    $(addprefix $(host_native)/,		\
                    $(shell cat $(host_native)/subdir-stmp))
	$(RANLIB) $@

# Syn68k uses inline assembly that confuses various gcc stack optimizations.
# Under gcc 2.x -fno-defer-pop is sufficient to get gcc to do the right thing
# with our code.  Under gcc 3.x we need to also add -maccumulate-outgoing-args.
# Since we don't know what gcc we have until we try to execute CC, we
# do a test right here to figure out if the c-compiler understands
# -maccumulate-outgoing-args.  If it does, we use it.
#
# A better solution would be to put something into our asm to form a
# barrier that gcc won't try to do stack optimizations across.  I haven't yet
# scoured the gcc 3 documentation or source code to see if there's something
# obvious that can be used.

syn68k.o: syn68k.c
	outgoing=;\
	$(CC) -maccumulate-outgoing-args -c -x c /dev/null 2> /dev/null \
            && outgoing=-maccumulate-outgoing-args; \
	$(CC) -S $(SYN68K_CFLAGS) -Wall -static -fno-defer-pop -Wno-unused\
		$(LOCAL_INCLUDES) $$outgoing syn68k.c -o ./syn68k.s
	if [ x"$(CLEANUP)" != x ] ; then \
		$(PERL) $(srcdir)/$(CLEANUP) < syn68k.s > syn68k.s.new && \
                mv syn68k.s.new syn68k.s ; fi
	if [ x"$(OPTIMIZE)" != x ] ; then \
		$(PERL) $(srcdir)/$(OPTIMIZE) < syn68k.s > syn68k.s.new && \
                mv syn68k.s.new syn68k.s ; fi
	$(CC) -xassembler-with-cpp -c ./syn68k.s -o syn68k.o
	$(RM) ./syn68k.s

syn68k.c mapindex.c mapinfo.c profileinfo.gz:	../syngen/syngen syn68k_header.c 68k.scm 68k.defines.scm\
				../include/syn68k_private.h    \
				../runtime/include/hash.h      \
				../runtime/include/interrupt.h \
				../runtime/include/trap.h
	if [ "$(srcdir)" != "." ]; then \
	  cp -p $(srcdir)/68k.scm $(srcdir)/syn68k_header.c $(srcdir)/68k.defines.scm . ; \
	fi
	../syngen/syngen -v 68k.scm
	gzip -1f profileinfo
