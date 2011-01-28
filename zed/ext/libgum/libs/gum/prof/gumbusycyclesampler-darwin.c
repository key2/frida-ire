/*
 * Copyright (C) 2011 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gumbusycyclesampler.h"

#include <mach/mach.h>

static void gum_busy_cycle_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
static GumSample gum_busy_cycle_sampler_sample (GumSampler * sampler);

G_DEFINE_TYPE_EXTENDED (GumBusyCycleSampler,
                        gum_busy_cycle_sampler,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SAMPLER,
                                               gum_busy_cycle_sampler_iface_init));

static void
gum_busy_cycle_sampler_class_init (GumBusyCycleSamplerClass * klass)
{
}

static void
gum_busy_cycle_sampler_iface_init (gpointer g_iface,
                                   gpointer iface_data)
{
  GumSamplerIface * iface = (GumSamplerIface *) g_iface;

  (void) iface_data;

  iface->sample = gum_busy_cycle_sampler_sample;
}

static void
gum_busy_cycle_sampler_init (GumBusyCycleSampler * self)
{
}

GumSampler *
gum_busy_cycle_sampler_new (void)
{
  GumBusyCycleSampler * sampler;

  sampler = g_object_new (GUM_TYPE_BUSY_CYCLE_SAMPLER, NULL);

  return GUM_SAMPLER (sampler);
}

gboolean
gum_busy_cycle_sampler_is_available (GumBusyCycleSampler * self)
{
  return TRUE;
}

static GumSample
gum_busy_cycle_sampler_sample (GumSampler * sampler)
{
  thread_basic_info_data_t info;
  mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
  kern_return_t kr;

  kr = thread_info (mach_thread_self (), THREAD_BASIC_INFO,
      (thread_info_t) &info, &info_count);
  g_assert_cmpint (kr, ==, KERN_SUCCESS);

  /*
   * We could convert this to actual cycles, but doing so would be a waste
   * of time, because GumSample is an abstract unit anyway.
   */
  return ((GumSample) info.user_time.seconds * G_USEC_PER_SEC) +
      info.user_time.microseconds;
}
