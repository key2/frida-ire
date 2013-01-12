if ENABLE_LOCAL_BACKENDS
SERVER := $(top_builddir)/src/zed-server
else
SERVER := $(top_builddir)/../../frida-ire-universal/zed-server
endif

pyexec_LTLIBRARIES = \
        frida.la
noinst_LTLIBRARIES = \
	libpyfrida-data.la \
	libpyfrida.la

frida_la_SOURCES = \
        pyfrida.c
frida_la_LDFLAGS = \
	-export-symbols-regex "^initfrida$$" \
	-module -avoid-version -shared \
	$(PYTHON_LDFLAGS) \
	$(ZED_LDFLAGS)
frida_la_LIBADD = \
	$(builddir)/libpyfrida.la \
	$(builddir)/libpyfrida-data.la \
	$(top_builddir)/src/libzed-core.la \
	$(PYTHON_LIBS) \
	$(ZED_LIBS)

libpyfrida_la_SOURCES = \
	session-manager.vala
libpyfrida_la_CFLAGS = \
	-w
libpyfrida_la_VALAFLAGS = \
	--library=pyfrida \
	--header=pyfrida.h \
	--vapidir=$(builddir) \
	--vapidir=$(top_builddir)/src \
	--vapidir=$(top_builddir)/lib/interfaces \
	--pkg=gee-1.0 \
	--pkg=gio-2.0 \
	--pkg=zed-core \
	--pkg=zed-interfaces \
	--pkg=pyfrida-data

CLEANFILES = \
	$(builddir)/pyfrida-data.vapi \
	$(builddir)/pyfrida-data.h \
	$(builddir)/pyfrida-data.c \
	$(builddir)/pyfrida-data-blob.S
libpyfrida_data_la_SOURCES = \
	$(builddir)/pyfrida-data.c \
	$(builddir)/pyfrida-data-blob.S
pyfrida-data.c: $(SERVER)
	$(AM_V_GEN) \
		"$(top_builddir)/tools/resource-compiler" \
			--enable-asm \
			-c "$(srcdir)/pyfrida.resources" \
			-o "$(builddir)/pyfrida-data" \
			$(SERVER)

AM_CPPFLAGS = \
	-I $(top_srcdir)/lib/interfaces \
	-I $(top_srcdir)/src \
	$(PYTHON_CFLAGS) \
	$(ZED_CFLAGS)
