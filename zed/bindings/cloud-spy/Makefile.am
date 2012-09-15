if ENABLE_LOCAL_BACKENDS
SERVER := $(top_builddir)/src/zed-server
else
SERVER := $(top_builddir)/../../frida-ire-universal/zed-server
endif

if OS_LINUX
deploy: all
	cp .libs/libcloud-spy.so.0.0.0 ../../../cloud-spy/chrome/plugin-linux.so
endif

if OS_MAC
deploy: all
	cp .libs/libcloud-spy.0.dylib ../../../cloud-spy/chrome/plugin-mac.plugin/Contents/MacOS/libnpcsplugin.dylib
endif

extra_ldflags =

if OS_LINUX
extra_ldflags += -Wl,--version-script -Wl,cloud-spy.version
else
extra_ldflags += -export-symbols-regex "^(NP_GetMIMEDescription|NP_GetValue|NP_GetEntryPoints|NP_Initialize|NP_Shutdown)$$"
endif

lib_LTLIBRARIES = libcloud-spy-data.la libcloud-spy-codegen.la libcloud-spy.la

libcloud_spy_codegen_la_SOURCES = \
	cloud-spy-api.vala \
	cloud-spy-root.vala \
	cloud-spy-codegen.c
libcloud_spy_codegen_la_CFLAGS = \
	-w
libcloud_spy_codegen_la_LDFLAGS = \
	-export-symbols-regex "^__hack_to_force_build_order__$$"
libcloud_spy_codegen_la_VALAFLAGS = \
	--library=cloud-spy \
	--header=cloud-spy.h \
	--vapidir=$(top_srcdir)/vapi \
	--vapidir=$(top_builddir)/src \
	--vapidir=$(top_builddir)/lib/interfaces \
	--vapidir=$(sourcedir) \
	--pkg=cloud-spy-object \
	--pkg=cloud-spy-data \
	--pkg=config \
	--pkg=gee-1.0 \
	--pkg=gio-2.0 \
	--pkg=json-glib-1.0 \
	--pkg=zed-core \
	--pkg=zed-interfaces \
	@ZED_VALAFLAGS@

libcloud_spy_la_SOURCES = \
	cloud-spy-plugin.h \
	cloud-spy-plugin.cpp \
	cloud-spy-object.h \
	cloud-spy-object-priv.h \
	cloud-spy-object.cpp \
	cloud-spy-promise.h \
	cloud-spy-promise.cpp \
	cloud-spy-byte-array.h \
	cloud-spy-byte-array.cpp \
	cloud-spy-api-glue.c \
	cloud-spy-root.c
libcloud_spy_la_CFLAGS = \
	-w
libcloud_spy_la_LDFLAGS = \
	$(ZED_LDFLAGS) \
	$(CLOUD_SPY_LDFLAGS) \
	$(extra_ldflags)
libcloud_spy_la_LIBADD = \
	$(top_builddir)/src/libzed-core.la \
	$(builddir)/libcloud-spy-data.la \
	$(ZED_LIBS) \
	$(CLOUD_SPY_LIBS)
libcloud_spy_la_LIBTOOLFLAGS = \
	--tag=disable-static

CLEANFILES = \
	$(builddir)/cloud-spy-data.vapi \
	$(builddir)/cloud-spy-data.h \
	$(builddir)/cloud-spy-data.c \
	$(builddir)/cloud-spy-data-blob.S
libcloud_spy_data_la_SOURCES = \
	$(builddir)/cloud-spy-data.c \
	$(builddir)/cloud-spy-data-blob.S
libcloud_spy_data_la_LDFLAGS = \
	-export-symbols-regex "^__hack_to_force_build_order__$$"
libcloud_spy_data_la_LIBTOOLFLAGS = \
	--tag=disable-shared
cloud-spy-data.c: $(SERVER)
	$(AM_V_GEN) \
		"$(top_builddir)/tools/resource-compiler" \
			--enable-asm \
			-c "$(srcdir)/cloud-spy.resources" \
			-o "$(builddir)/cloud-spy-data" \
			$(SERVER)

AM_CPPFLAGS = \
	-include config.h \
	-I $(top_builddir)/src \
	-I $(top_builddir)/lib/interfaces \
	$(ZED_CFLAGS) \
	$(CLOUD_SPY_CFLAGS)
