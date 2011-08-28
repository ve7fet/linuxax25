man_MANS = \
	ax25.4 \
	ax25_call.8 \
	ax25d.8 ax25d.conf.5 \
	axctl.8 \
	axgetput.1 \
	axparms.8 \
	axports.5 \
	axspawn.8 axspawn.conf.5 \
	baycom.9 \
	beacon.8 \
	bpqparms.8 \
	dmascc_cfg.1 \
	hdlcdrv.9 \
	kissattach.8 \
	kissnetd.8 \
	kissparms.8 \
	m6pack.8 \
	mheard.1 \
	mheardd.8 \
	mkiss.8 \
	net2kiss.8 \
	netrom.4 \
	netromd.8 \
	netrom_call.8 \
	nodesave.8 \
	nrattach.8 \
	nrbroadcast.5 \
	nrparms.8 \
	nrports.5 \
	nrsdrv.8 \
	rip98d.8 rip98d.conf.5 \
	rose.4 \
	rose_call.8 \
	rsattach.8 \
	rsdwnlnk.8 \
	rsparms.8 \
	rsports.5 \
	rsuplnk.8 \
	rxecho.8 rxecho.conf.5 \
	sethdlc.8 \
	smdiag.8 \
	smmixer.8 \
	soundmodem.9 \
	spattach.8 \
	tcp_call.8 \
	ttylinkd.8 ttylinkd.conf.5
 
EXTRA_DIST = $(man_MANS)

install-data-hook:
	(cd $(DESTDIR)$(mandir)/man1 && ln -sf axgetput.1 bget.1 && ln -sf axgetput.1 bput.1)
