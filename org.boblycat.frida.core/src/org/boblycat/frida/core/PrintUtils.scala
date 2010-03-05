package org.boblycat.frida.core

object PrintUtils {

	private def toBinaryString(b : Int, bits : Int) : String = {
		var v = 1 << (bits - 1)
		var s = ""
		for(i <- 0 until bits) {
			if((b.toInt & v) == v) { s += "1" } else { s += "0" }
			v >>= 1
		}
		s
	}
	
	def toBinaryString(b : Int) : String = toBinaryString(b, 32)
	def toBinaryString(b : Short) : String = toBinaryString(b, 16)
	def toBinaryString(b : Byte) : String = toBinaryString(b, 8)

	def toPaddedBinaryString(b : Byte) = {
		val s = b.toBinaryString
		val miss = 8 - s.length
		"0" * miss + s 
	}
}
