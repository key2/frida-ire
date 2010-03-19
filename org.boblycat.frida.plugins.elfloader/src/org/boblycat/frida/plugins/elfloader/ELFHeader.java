package org.boblycat.frida.plugins.elfloader;

import org.boblycat.frida.plugins.elfloader.types.*;

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
public class ELFHeader {
    public static final int NIDENT = 16;

    final UChar[] ident = new UChar[NIDENT];
    final E32Half type;
    final E32Half machine;
    final E32Word version;
    final E32Addr entry;
    final E32Off phoff;
    final E32Off shoff;
    final E32Word flags;
    final E32Half ehsize;
    final E32Half phentsize;
    final E32Half phnum;
    final E32Half shentsize;
    final E32Half shtstrndx;

    private ELFHeader(
            final UChar[] ident,
            final E32Half type,
            final E32Half machine,
            final E32Word version,
            final E32Addr entry,
            final E32Off phoff,
            final E32Off shoff,
            final E32Word flags,
            final E32Half ehsize,
            final E32Half phentsize,
            final E32Half phnum,
            final E32Half shentsize,
            final E32Half shtstrndx) {
        System.arraycopy(this.ident, 0, ident, 0, NIDENT);
        this.type = type;
        this.machine = machine;
        this.version = version;
        this.entry = entry;
        this.phoff = phoff;
        this.shoff = shoff;
        this.flags = flags;
        this.ehsize = ehsize;
        this.phentsize = phentsize;
        this.phnum = phnum;
        this.shentsize = shentsize;
        this.shtstrndx = shtstrndx;
    }

    public static ELFHeader loadFromStream(InputStream ins) throws IOException {
        final ELFStreamReader esr = new ELFStreamReader(ins);
        return new ELFHeader(
                esr.readUChar(16), // ident
                esr.readE32Half(), // type
                esr.readE32Half(), // machine
                esr.readE32Word(), // version
                esr.readE32Addr(), // entry
                esr.readE32Off(), // phoff
                esr.readE32Off(), // shoff
                esr.readE32Word(), // flags
                esr.readE32Half(), // ehsize
                esr.readE32Half(), // phentsize
                esr.readE32Half(), // phnum
                esr.readE32Half(), // shentsize
                esr.readE32Half() // shtstrndx
        );
    }

    public void dump() {
        System.out.println(ident);
    }
}