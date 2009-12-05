SUBDIRS = native/i386

lib_LIBRARIES = libsyn68k.a
include_HEADERS = ../include/syn68k_public.h

host_native=@HOST_NATIVE@

SYN68K_CFLAGS=@SYN68K_CFLAGS@
CLEANUP=@assembly_cleanup_script@
OPTIMIZE=@assembly_optimization_script@

# The way we use labels in our assembler is out of spec.  The gcc info
# pages say that you're not allowed to jump between labels of asm.
# Historically, however, we chose not to put everything into a single
# function and then jump about within spec.  IIRC, that was either due
# to the compilation time and memory consumption to be excessive
# and/or due to us running into compiler bugs.  This was all back
# around 1995 though and we have more memory and better compilers.  As
# such, it may make sense to re-evaluate the decision to split all the
# opcodes into separate functions.  However, until we do that, we need
# to deal with the fact that some dead-code-elimination (DCE)
# optimizations will cause our trickery to fail.  As such, we include
# "-fno-dce" in the architectures where we know we're going to have trouble.

dce_fixup=@dce_fixup@

AM_CFLAGS = -DRUNTIME

DIST_SOURCES = 68k.defines.scm 68k.scm alloc.c backpatch.c block.c \
               blockinfo.c callback.c checksum.c deathqueue.c destroyblock.c \
	       diagnostics.c dosinterrupts.c fold.pl hash.c i486-cleanup.pl \
	       i486-optimize.pl init.c interrupt.c native.c opcode_dummy.c \
               profile.c rangetree.c recompile.c reg sched.pl syn68k_header.c \
	       translate.c trap.c x86_recog.pl \
\
               include/alloc.h \
	       include/backpatch.h include/block.h include/blockinfo.h \
	       include/callback.h include/ccfuncs.h include/checksum.h \
               include/deathqueue.h include/destroyblock.h \
               include/diagnostics.h include/hash.h include/interrupt.h \
	       include/mapping.h include/native.h include/profile.h \
	       include/rangetree.h include/recompile.h include/translate.h \
	       include/trap.h \
\
	       native/i386/analyze.c native/i386/host-native.c \
	       native/i386/host-native.h native/i386/i386-aux.c \
	       native/i386/i386-aux.h native/i386/main.c \
	       native/i386/process.c native/i386/process.h \
	       native/i386/template.c native/i386/template.h \
	       native/i386/xlate-aux.c native/i386/xlate-aux.h \
	       native/i386/xlate.c native/i386/xlate.h \
	       native/i386/xlatemain.c native/i386/xlatetable.c \
\
	       native/null/Makefile native/null/host-native.h \
               native/null/host-xlate.h

LOCAL_INCLUDES =

LOCAL_INCLUDES += -I$(srcdir)/include \
  -I$(srcdir)/../include -I$(srcdir) -I../include

LOCAL_CFLAGS = -DRUNTIME -Iinclude

all: libsyn68k.a

.c.o:
	$(CC) $(AM_CFLAGS) -c $(LOCAL_INCLUDES) $< -o $@

OBJS =	block.o diagnostics.o hash.o rangetree.o translate.o alloc.o	\
	blockinfo.o trap.o destroyblock.o callback.o init.o interrupt.o	\
	profile.o dosinterrupts.o deathqueue.o checksum.o native.o	\
	backpatch.o recompile.o						\
	mapindex.o mapinfo.o syn68k.o opcode_dummy.o

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
	$(CC) -S $(SYN68K_CFLAGS) $(dce_fixup) -Wall -static -fno-defer-pop -Wno-unused\
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

clean-local:
	-rm -rf native 68k.defines.scm 68k.scm mapinfo.c syn68k.c mapindex.c \
                profileinfo.gz syn68k_header.c bucket.c
