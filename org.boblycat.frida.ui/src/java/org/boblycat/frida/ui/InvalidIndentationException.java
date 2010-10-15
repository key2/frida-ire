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
public class InvalidIndentationException extends RuntimeException {
    protected final int lineNumber;
    protected final int colNumber;

    public InvalidIndentationException(int lineNumber, int colNumber) {
        super("Invalid indentation at line " + lineNumber + ", column " + colNumber);
        this.lineNumber = lineNumber;
        this.colNumber = colNumber;
    }

    public int getLineNumber() { return lineNumber; }
    public int getColumnNumber() { return colNumber; }
}
