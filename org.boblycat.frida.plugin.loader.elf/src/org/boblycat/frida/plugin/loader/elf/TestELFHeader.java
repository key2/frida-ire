package org.boblycat.frida.plugin.loader.elf;

import org.boblycat.frida.plugin.loader.elf.ELFHeader;

import java.io.FileInputStream;
import java.io.IOException;

/**
 * Created: Mar 19, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class TestELFHeader {

    public static void main(String[] args) throws IOException {
        ELFHeader hdr = ELFHeader.loadFromStream(new FileInputStream("test.elf"));
        hdr.dump();

    }
}
