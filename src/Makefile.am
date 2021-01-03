AM_LDFLAGS=$(LIBS)

bin_PROGRAMS = rtx-comp rtx-proc rtx-decomp random-path

rtx_comp_SOURCES = rtx_comp.cc rtx_compiler.cc trx_compiler.cc pattern.cc apertium_re.cc

rtx_proc_SOURCES = rtx_proc.cc rtx_processor.cc apertium_re.cc chunk.cc

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
