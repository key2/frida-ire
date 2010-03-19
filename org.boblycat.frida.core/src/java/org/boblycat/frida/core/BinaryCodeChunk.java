package org.boblycat.frida.core;

/**
 * Created: Mar 19, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class BinaryCodeChunk {

    private final long baseAddress;
    private final byte[] rawData;

    public BinaryCodeChunk(long baseAddress, byte[] rawData) {
        this.baseAddress = baseAddress;
        this.rawData = rawData;
    }

    public byte byteAt(int index) { return rawData[index]; }
    public byte[] getBytes() { return rawData; }
    public long getBaseAddress() { return baseAddress; }
    public int length() { return rawData.length; }
}
