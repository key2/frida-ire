package org.boblycat.frida.core;

public class BitUtils {
	
  public static byte[] convertHexStringToBinary(String data) {
    final byte[] r = new byte[data.length()/2];
    for(int i = 0; i < data.length()/2; i++) {
    	String b = data.substring(i*2,i*2+2);
        r[i] = (byte)Integer.parseInt(b, 16);
    }
    return r;
  }

  public static long stringToBinary(String lit) {
	  long r = 0;
	  for(int i = 0; i < lit.length(); i++) {
		  char c = lit.charAt(i);
		  if(c == '0') {
			  r <<= 1;
		  } else if (c == '1') {
			  r <<= 1;
			  r |= 1;
		  } else {
			  return -1; 
		  }
	  }
	  return r;
	}


}
