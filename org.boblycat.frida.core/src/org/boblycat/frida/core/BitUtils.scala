package org.boblycat.frida.core

object BitUtils {
	
	def convertHexStringToBinary(data : String) : Array[Byte] = {
		val r = new Array[Byte](data.length/2)
		for(i <- 0 until data.length/2) {
			val b = data.substring(i*2,i*2+2)
			r(i) = Integer.parseInt(b, 16).toByte
		}
		r
	}

	def stringToBinary(lit : String) : Long = {
			var r = 0
		for(c <- lit) {
			if(c == '0') {
				r <<= 1
			} else if (c == '1') {
				r <<= 1 
				r |= 1
			} else {
				return -1 
			}
		}
		r
	}


}
