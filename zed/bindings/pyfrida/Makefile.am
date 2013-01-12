pyexec_LTLIBRARIES = \
        frida.la

frida_la_SOURCES = \
        pyfrida.c
frida_la_LDFLAGS = \
	-export-symbols-regex "^initfrida$$" \
	-module -avoid-version -shared \
	$(ZED_LDFLAGS)
frida_la_LIBADD = \
	$(top_srcdir)/src/libzed-core.la \
	$(ZED_LIBS)

AM_CPPFLAGS = \
	-I $(top_srcdir)/lib/interfaces \
	-I $(top_srcdir)/src \
	$(PYTHON_CFLAGS) \
	$(ZED_CFLAGS)
