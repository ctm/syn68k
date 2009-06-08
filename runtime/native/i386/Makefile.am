CC_FOR_BUILD=@CC_FOR_BUILD@
CFLAGS_FOR_BUILD=@CFLAGS_FOR_BUILD@

LOCAL_INCLUDES =

runtimedir = $(top_srcdir)/runtime

LOCAL_INCLUDES += -I$(srcdir)/include \
  -I$(runtimedir)/include \
  -I$(runtimedir)/../include -I$(srcdir) -I. -I../../../include

# these objects must be compiled with LIB_CC
MISC_OBJS = host-native.o host-xlate.o i386-aux.o xlate-aux.o

# these objects must be compiled with CC
TEMPLATE_OBJS = main.o template.o process.o
ANALYZE_OBJS = analyze.o template.o
XLATE_OBJS = xlatemain.o xlatetable.o xlate.o

i386-isa.h:	src-stmp

host-native.o: host-native.c i386-isa.h
	$(CC) $(CFLAGS) -c $(LOCAL_INCLUDES) -o $(@F) $<
host-xlate.o: host-xlate.c
	$(CC) $(CFLAGS) -c $(LOCAL_INCLUDES) -o $(@F) $<
i386-aux.o: i386-aux.c
	$(CC) $(CFLAGS) -c $(LOCAL_INCLUDES) -o $(@F) $<
xlate-aux.o: xlate-aux.c
	$(CC) $(CFLAGS) -c $(LOCAL_INCLUDES) -o $(@F) $<

xlatemain.o:	xlatemain.c
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) -c $(LOCAL_INCLUDES) -o $(@F) $<
xlatetable.o:	xlatetable.c
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) -c $(LOCAL_INCLUDES) -o $(@F) $<
xlate.o:	xlate.c
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) -c $(LOCAL_INCLUDES) -o $(@F) $<

.c.o:
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) -c $(LOCAL_INCLUDES) -o $(@F) $<

subdir-stmp: $(MISC_OBJS) src-stmp obj-stmp
	echo $(MISC_OBJS) 		\
             `ls _*.o` > subdir-stmp

obj-stmp:
	k=1;							\
	for i in i386_stubs/*; do			\
	  $(CC) $(CFLAGS) $(LOCAL_INCLUDES) $$i -c -o _i386_$$k.o;\
	  k=`expr $$k + 1`;					\
	  echo done processing $$i into _i386_$$k.o;		\
	done
	touch obj-stmp

src-stmp: template
	mkdir -p i386_stubs
	./template "$(MAKE)"
	$(RM) analyze
	touch src-stmp

template: $(TEMPLATE_OBJS) analyze.c
	$(CC_FOR_BUILD) $(TEMPLATE_OBJS) -o template

host-xlate.c host-xlate.h: xlate
	./xlate

xlate:	$(XLATE_OBJS)
	$(CC_FOR_BUILD) $(XLATE_OBJS) -o xlate

analyze: $(ANALYZE_OBJS)
	$(CC_FOR_BUILD) $(CFLAGS_FOR_BUILD) $(ANALYZE_OBJS) -o analyze

analyze.o:	asmdata.h template.h


# Invoked only from ./template
nextobj:
#doesn't do anything
