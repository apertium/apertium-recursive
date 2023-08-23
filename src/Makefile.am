AM_LDFLAGS=$(LIBS)
AM_CPPFLAGS = -DAPRC_I18N_DATA='"$(APRC_I18N_DATA)"' -DLTTB_I18N_DATA='"$(LTTB_I18N_DATA)"'


bin_PROGRAMS = rtx-comp rtx-proc rtx-decomp random-path

rtx_comp_SOURCES = rtx_comp.cc rtx_compiler.cc trx_compiler.cc pattern.cc

rtx_proc_SOURCES = rtx_proc.cc rtx_processor.cc chunk.cc

rtx_decomp_SOURCES = rtx_decomp.cc

random_path_SOURCES = randpath.cc

bin_SCRIPTS = apertium-validate-trx

apertium_recursivedir = $(prefix)/share/apertium-recursive
apertium_recursive_DATA = trx.dtd

apertium-validate-trx: Makefile.am validate-header.sh
	@echo "Creating apertium-validate-trx script"
	@echo "#!/usr/bin/env bash" > $@
	@cat $(abs_srcdir)/validate-header.sh >> $@
	@echo "xmllint --dtdvalid \"$(apertium_recursivedir)\"/trx.dtd --noout \"\$$FILE1\"" >>$@
	@chmod a+x $@

EXTRA_DIST = trx.dtd
CLEANFILES = $(bin_SCRIPTS)
