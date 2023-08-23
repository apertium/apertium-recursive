pkgconfigdir = $(libdir)/pkgconfig
dist_pkgconfig_DATA = apertium-recursive.pc

SUBDIRS = src locales

EXTRA_DIST=autogen.sh

test: all
	(cd tests || exit && ./build_tests.py && chmod +x ./run_tests.py && ./run_tests.py)

export LTTB_I18N_DATA=$(datadir)/lttoolbox/lttoolbox.dat
export LOCALES_DIR=$(datadir)/$(PACKAGE_NAME)
export APRC_I18N_DATA=$(LOCALES_DIR)/aprc.dat
