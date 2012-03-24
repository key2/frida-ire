package org.boblycat.frida.core

import java.io.File

/**
 *
 * Created: Mar 26, 2010
 *
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3 
 *
 *
 * @author: karltk@boblycat.org
 */

object FridaCore {
  val cfg = new FridaConfig(System.getProperty("user.home") + "/.frida")

  def startup {
    ensureConfigDir(cfg.baseDir)
    ChunkStore.startup(cfg)
  }

  def shutdown {
    ChunkStore.shutdown(cfg)
  }

  def ensureConfigDir(path : String) {
    val f = new File(path)
    if(!f.exists)
      f.mkdirs
      // FIXME bomb if fails
    else if(!f.isDirectory || !f.canRead)
      false // FIXME report error if we can't make config dir
  }
}