bin_PROGRAMS = axgetput

axgetput_SOURCES = axgetput.c axgetput.h util.h proto_bin.h util.c proto_bin.c includes.h

install-exec-hook:
	(cd $(DESTDIR)$(bindir) && ln -sf axgetput bget && ln -sf axgetput bput)
