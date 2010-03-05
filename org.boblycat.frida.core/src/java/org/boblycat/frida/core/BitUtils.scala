package org.boblycat.frida.core;

class BitUtils {
	
  public byte[] convertHexStringToBinary(String data) {
    final byte[] r = new byte[data.length/2];
    for(int i = 0; i < data.length/2; i++) {
 	String b = data.substring(i*2,i*2+2);
        r[i] = Integer.parseInt(b, 16).toByte
    }
    return r;
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
