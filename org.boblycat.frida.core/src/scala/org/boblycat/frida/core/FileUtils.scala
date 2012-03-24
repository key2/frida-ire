package org.boblycat.frida.core

import java.io.{File, RandomAccessFile}

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

object FileUtils {

  def loadBytes(fileName : String) : Array[Byte] = loadBytes(new File(fileName))
  def loadBytes(file: File): Array[Byte] = {
    var ra = new RandomAccessFile(file, "r")
    var r = new Array[Byte](ra.length.asInstanceOf[Int])
    ra.read(r)
    r
  }
}