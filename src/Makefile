# FIXME: Use autoconf ?
# HAS_GTK_GUI = 1

DESTDIR =
SBINDIR = /usr/sbin
XCFLAGS = -I. -W  -g -D_FORTIFY_SOURCE=2 -Wno-sign-compare
LDF  = -Wl,--as-needed
LDF += -lncursesw
HOSTCC=cc

OBJS= latencytop.o text_display.o translate.o fsync.o
CCAN_OBJS= ccan/str/str.o ccan/intmap/intmap.o ccan/ilog/ilog.o ccan/list/list.o

ifdef HAS_GTK_GUI
  XCFLAGS += `pkg-config --cflags glib-2.0`
  XCFLAGS += `pkg-config --cflags gtk+-2.0` -DHAS_GTK_GUI
  LDF += `pkg-config --libs glib-2.0`
  LDF += `pkg-config --libs gtk+-2.0`
  OBJS += gtk_display.o
endif

#
# The w in -lncursesw is not a typo; it is the wide-character version
# of the ncurses library, needed for multi-byte character languages
# such as Japanese and Chinese etc.
#
# On Debian/Ubuntu distros, this can be found in the
# libncursesw5-dev package.
#

# We write explicity this "implicit rule"
%.o : %.c
	$(CC) -c $(CFLAGS) $(XCFLAGS) $< -o $@

latencytop: $(CCAN_OBJS) $(OBJS) latencytop.h Makefile
	$(CC) $(CFLAGS) $(CCAN_OBJS) $(OBJS) $(LDFLAGS) $(LDF) -o latencytop

# config.h is built by configurator which has no ccan dependencies
CONFIGURATOR := tools/configurator/configurator
$(CONFIGURATOR): $(CONFIGURATOR).c
	$(PRE)$(HOSTCC) $(CCAN_CFLAGS) $(DEP_CFLAGS) $< -o $@
config.h: $(CONFIGURATOR) Makefile
	$(PRE)$(CONFIGURATOR) $(HOSTCC) $(CCAN_CFLAGS) >$@.tmp && mv $@.tmp $@

$(OBJS) $(CCAN_OBJS): config.h

clean:
	$(RM) *~ latencytop DEADJOE $(OBJS) $(CCAN_OBJS) config.h

install: latencytop
	mkdir -p $(DESTDIR)/usr/share/latencytop
	mkdir -p $(DESTDIR)/$(SBINDIR)
	install -m 0644 latencytop.trans $(DESTDIR)/usr/share/latencytop/latencytop.trans
	install -m 0644 *.png $(DESTDIR)/usr/share/latencytop/
	install -m 0755 latencytop $(DESTDIR)$(SBINDIR)/
