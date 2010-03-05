package org.boblycat.frida.core

class FridaException(val msg : String) extends Exception {

	override def toString = msg
}
