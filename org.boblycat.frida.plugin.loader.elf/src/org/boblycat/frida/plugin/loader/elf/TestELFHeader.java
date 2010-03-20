package org.boblycat.frida.plugin.loader.elf;

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
        ELFFile elf = ELFFile.loadFromStream(new FileInputStream("/home/karltk/ole/test.elf"));
        elf.dump();

    }
}
