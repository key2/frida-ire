package org.boblycat.frida.core;

/**
 *
 * Created: Feb 26, 2010
 *
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 *
 * @author: karltk@boblycat.org
 */

public class BitUtils {
	
    public static byte[] hexStringToBinary(String data) {
        final int n = data.length() / 2;
        final byte[] r = new byte[n];
        for(int i = 0; i < n; i++) {
            String b = data.substring(i*2, i*2 + 2);
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
