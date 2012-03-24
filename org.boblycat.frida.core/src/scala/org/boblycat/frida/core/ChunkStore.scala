package org.boblycat.frida.core

import java.io._

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

object ChunkStore {
  var idCounter = 0
  private val chunkRegistry = new scala.collection.mutable.HashMap[ChunkId, (String, BinaryCodeChunk)]()

  def startup(config : FridaConfig) {
    val storeFile = new File(config.baseDir + "/chunkstore")
    if(storeFile.exists && storeFile.canRead) {
      val fis = new FileInputStream(storeFile)
      val ois = new ObjectInputStream(fis)
      val m = ois.readObject.asInstanceOf[scala.collection.mutable.HashMap[ChunkId, (String, BinaryCodeChunk)]]
      chunkRegistry ++= m
      ois.close()
   }
  }

  def shutdown(config : FridaConfig) {
    val storeFile = new File(config.baseDir + "/chunkstore")
    if(!storeFile.exists) {
      storeFile.createNewFile
    }
    if((storeFile.exists && storeFile.canWrite)) {
      val fos = new FileOutputStream(storeFile)
      val ous = new ObjectOutputStream(fos)
      ous.writeObject(chunkRegistry)
    }
  }

  def addChunk(name : String, chunk : BinaryCodeChunk) : ChunkId = {
    val id = nextId
    chunkRegistry += (id -> (name, chunk))
    println("registry now " + chunkRegistry.size)
    id
  }

  def getChunk(id : ChunkId) : Option[BinaryCodeChunk] = {
    chunkRegistry.get(id) match {
      case None => None
      case Some((name, chunk)) => Some(chunk)
    }
  }

  def listChunkNames() : Iterable[(String, ChunkId)] = {
    var rs = List[(String, ChunkId)]()
    for((id, (name, _)) <- chunkRegistry) {
      rs = (name, id) :: rs
    }
    println("list " + rs)
    rs
  }

  def nextId = {
    val x = new ChunkId(ChunkStore.idCounter)
    ChunkStore.idCounter += 1
    x
  }
}