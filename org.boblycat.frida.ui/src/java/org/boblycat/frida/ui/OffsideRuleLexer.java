package org.boblycat.frida.ui;

/**
 * Created: Mar 31, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class OffsideRuleLexer {

    public static final int DEFAULT_TAB_WIDTH = 4;
    public static final int OUTDENT_CHAR = '\\';

    private final int tabWidth;

    public OffsideRuleLexer() {
        tabWidth = DEFAULT_TAB_WIDTH;
    }
    public OffsideRuleLexer(int tabWidth) {
        this.tabWidth = tabWidth;
    }

    public static interface IndentationErrorCallback {
        public boolean indentationError(int offset, int column, int line);
    }

    String preprocess(String input, IndentationErrorCallback iec) {
        char[] result = new char[input.length() * 2];
        final int n = input.length();
        int lineNumber = 0;
        int colNumber = 0;
        int pos = 0;
        int expectedIndent = 0;
        int currentIndent = 0;
        boolean examineIndent = false;
        for(int i = 0; i < n; i++) {
            final char c = input.charAt(i);
            switch(c) {
                case ' ':
                    if(examineIndent)
                        currentIndent++;
                    colNumber++;
                    break;
                case '\t':
                    if(examineIndent)
                        currentIndent += tabWidth;
                    colNumber += tabWidth;
                    break;
                case '\n':
                    currentIndent = 0;
                    examineIndent = true;
                    colNumber = 0;
                    lineNumber++;
                    break;
                case ':':
                    expectedIndent = currentIndent + tabWidth;
                    colNumber++;
                    break;
                default:
                    System.out.println(currentIndent + "/" + examineIndent + "/" + c);
                    if(examineIndent) {
                        System.out.println(currentIndent + "/" + expectedIndent);
                        if(!(currentIndent == expectedIndent || currentIndent == (expectedIndent - tabWidth))) {
                            iec.indentationError(i, colNumber, lineNumber);
                        }
                        final int outdentCount = (expectedIndent - currentIndent) / tabWidth;
                        System.out.println("o=" + outdentCount);
                        for(int j=0; j<outdentCount; j++)
                            result[pos++] = OUTDENT_CHAR; 
                    }
                    examineIndent = false;
                    colNumber++;
            }
            result[pos++] = c;
        }
        return new String(result);
    }

    public static void main(String[] args) {
        String s = "def ():\n\tif 0:\n\t\tbreak\n\t\tbreak\n\tif 1:\n\t\tbreak\n\t\telse:\n\t\tbreak\n\tbreak";
        System.out.println(s);
        System.out.println(new OffsideRuleLexer().preprocess(s, new IndentationErrorCallback() {
            @Override
            public boolean indentationError(int offset, int column, int line) {
                System.err.println("Indentation error at line " + line + ", column " + column);
                return true;
            }
        }));
    }
}
