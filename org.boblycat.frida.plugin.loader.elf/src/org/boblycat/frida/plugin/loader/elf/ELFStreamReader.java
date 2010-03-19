package org.boblycat.frida.plugin.loader.elf;

import org.boblycat.frida.plugin.loader.elf.types.*;

import java.io.IOException;
import java.io.InputStream;

/**
 * Created: Mar 19, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class ELFStreamReader {
    private final InputStream ins;
    
    public ELFStreamReader(InputStream ins) {
        this.ins = ins;
    }

    public UChar[] readUChar(int n) throws IOException {
        final UChar[] r = new UChar[n];
        for(int i = 0; i < n; i++) {
            r[i] = new UChar((byte)ins.read());
        }
        return r;
    }

    // FIXME assume little-endian. correct?
    public E32Half readE32Half() throws IOException {
        final short lsb = (short)(ins.read() & 0xFF);
        final short msb = (short)(ins.read() & 0xFF);
        return new E32Half((short)((msb << 8) | lsb));
    }

    public E32Word readE32Word() throws IOException {
        final int b0 = ins.read() & 0xFF;
        final int b1 = ins.read() & 0xFF;
        final int b2 = ins.read() & 0xFF;
        final int b3 = ins.read() & 0xFF;

        return new E32Word(b3 << 24 | b2 << 16 | b1 << 8 | b0);
    }

    public E32Addr readE32Addr() throws IOException {
        return new E32Addr(readE32Word().value);
    }

    public E32Off readE32Off() throws IOException {
            return new E32Off(readE32Word().value);
    }
}
