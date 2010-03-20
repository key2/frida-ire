package org.boblycat.frida.plugin.loader.elf;

import java.io.IOException;
import java.io.InputStream;

/**
 * Created: Mar 20, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class ELFFile {
    private ELFHeader header = null;
    private ProgramHeader programHeaders[] = null;
    //private SectionHeader sectionHeaders[] = null;

    public static ELFFile loadFromStream(InputStream ins) throws IOException {
        final ELFStreamReader esr = new ELFStreamReader(ins);
        ELFFile r = new ELFFile();
        r.header = ELFHeader.loadFromStream(esr);
        esr.skipTo(r.header.phoff.value);
        r.programHeaders = new ProgramHeader[r.header.phnum.value];
        for(int i = 0; i < r.header.phnum.value; i++)
            r.programHeaders[i] = ProgramHeader.loadFromStream(esr);
        System.out.println(esr.getOffset());
        return r;
    }

    public void dump() {
        header.dump();
        for(int i = 0; i < programHeaders.length; i++)
            programHeaders[i].dump();

    }
}
