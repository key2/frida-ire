#ifndef __CLOUD_SPY_VARIANT_H__
#define __CLOUD_SPY_VARIANT_H__

#include "cloud-spy-plugin.h"

G_BEGIN_DECLS

NPClass * cloud_spy_variant_get_class (void) G_GNUC_CONST;

NPObject * cloud_spy_variant_new (NPP npp, GVariant * variant);

G_END_DECLS

#endif