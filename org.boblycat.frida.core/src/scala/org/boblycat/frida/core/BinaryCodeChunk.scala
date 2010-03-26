package org.boblycat.frida.core

/**
 * Created: Mar 19, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk @boblycat.org
 */

@serializable
class BinaryCodeChunk(val baseAddress : Long, val rawData : Array[Byte]) {

  def getBaseAddress = baseAddress

  def byteAt(index : Int): Byte = {
    return rawData(index)
  }

  def getBytes: Array[Byte] = rawData
  def length: Int = rawData.length
}