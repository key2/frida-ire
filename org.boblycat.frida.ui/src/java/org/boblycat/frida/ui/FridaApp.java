package org.boblycat.frida.ui;

/**
 * Created: Mar 12, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */

import java.io.IOException;

import org.apache.pivot.collections.*;
import org.apache.pivot.serialization.SerializationException;
import org.apache.pivot.wtk.*;
import org.apache.pivot.wtkx.WTKXSerializer;
import org.boblycat.frida.core.CodeChunk;
import org.boblycat.frida.core.disassembler.Disassembler;
import org.boblycat.frida.core.disassembler.DisassemblerFactory;
import org.boblycat.frida.core.disassembler.Instr;
import org.boblycat.frida.plugin.disassembler.mips.MIPSDisasm;

public class FridaApp implements Application {

    private Window window = null;
    private TabPane tabPane = null;
    //private FileBrowserSheet fileBrowserSheet = null;

    static class BinaryBlob {
        final byte[] blobData;
        final String displayName;
        BinaryBlob(String displayName, byte[] blobData) {
            this.displayName = displayName;
            this.blobData = blobData;
        }
        public String toString() { return displayName; }
    }

    @Override
    public void startup(Display display, Map<String, String> properties) throws SerializationException, IOException {
        WTKXSerializer wtkxSerializer = new WTKXSerializer();
        window = (Window)wtkxSerializer.readObject(this, "frida.wtkx");

        tabPane = (TabPane)wtkxSerializer.get("tabPane");

        ListView projectContent = (ListView)wtkxSerializer.get("projectContent");
        List<BinaryBlob> ls = new ArrayList<BinaryBlob>();
        ls.add(new BinaryBlob("One", new byte[] { (byte)0xE8, (byte)0xFF, (byte)0xBD, (byte)0x27 }));
        projectContent.setListData(ls);
        projectContent.getListViewSelectionListeners().add(new ListViewSelectionListener() {

            @Override
            public void selectedRangeAdded(ListView listView, int rangeStart, int rangeEnd) {
                System.out.println("added");
                Sequence<?> selectedItems = listView.getSelectedItems();
                for(int i = 0; i < selectedItems.getLength(); i++) {
                    System.out.println(selectedItems.get(i));  
                }
            }

            @Override
            public void selectedRangeRemoved(ListView listView, int rangeStart, int rangeEnd) {
                System.out.println("removed");
            }

            @Override
            public void selectedRangesChanged(ListView listView, Sequence<Span> previousSelectedRanges) {
                Sequence<?> selectedItems = listView.getSelectedItems();
                for(int i = 0; i < selectedItems.getLength(); i++) {
                    createOrSelectTab((BinaryBlob)selectedItems.get(i));
                }
            }
        });
        //fileBrowserSheet = new FileBrowserSheet(FileBrowserSheet.Mode.OPEN);

        window.open(display);
    }


    private MenuHandler menuHandler = new MenuHandler.Adapter() {
        TextInputTextListener textInputTextListener = new TextInputTextListener() {
            @Override
            public void textChanged(TextInput textInput) {
                updateActionState(textInput);
            }
        };

        TextInputSelectionListener textInputSelectionListener = new TextInputSelectionListener() {
            @Override
            public void selectionChanged(TextInput textInput, int previousSelectionStart,
                int previousSelectionLength) {
                updateActionState(textInput);
            }
        };

        @Override
        public void configureMenuBar(Component component, MenuBar menuBar) {
            if (component instanceof TextInput) {
                TextInput textInput = (TextInput)component;

                updateActionState(textInput);
                Action.getNamedActions().get("paste").setEnabled(true);

                textInput.getTextInputTextListeners().add(textInputTextListener);
                textInput.getTextInputSelectionListeners().add(textInputSelectionListener);
            } else {
                Action.getNamedActions().get("cut").setEnabled(false);
                Action.getNamedActions().get("copy").setEnabled(false);
                Action.getNamedActions().get("paste").setEnabled(false);
            }
        }

        @Override
        public void cleanupMenuBar(Component component, MenuBar menuBar) {
            if (component instanceof TextInput) {
                TextInput textInput = (TextInput)component;
                textInput.getTextInputTextListeners().remove(textInputTextListener);
                textInput.getTextInputSelectionListeners().remove(textInputSelectionListener);
            }
        }

        private void updateActionState(TextInput textInput) {
            Action.getNamedActions().get("cut").setEnabled(textInput.getSelectionLength() > 0);
            Action.getNamedActions().get("copy").setEnabled(textInput.getSelectionLength() > 0);
        }
    };

    private Component loadTab(BinaryBlob blob) {
        WTKXSerializer wtkxSerializer = new WTKXSerializer();
        Component tab;
        try {
            TableView disasmTable = (TableView)wtkxSerializer.readObject(this, "disasm.wtkx");

            MIPSDisasm.register();
		    Disassembler dis = DisassemblerFactory.create("mipsel");
		    CodeChunk res = dis.disassemble(0, blob.blobData);
            List<Map<String,String>> ls = new ArrayList<Map<String,String>>(res.length());
            for(Instr ins : res) {
                Map<String,String> m = new HashMap<String,String>();
                m.put("address", String.format("0x%x", ins.address()));
                m.put("instr", ins.instruction());
                m.put("args", ins.args());
                m.put("comment", ins.comment());
                ls.add(m);
            }
            disasmTable.setTableData(ls);
            tab = new Border(disasmTable);                  
        } catch (IOException exception) {
            throw new RuntimeException(exception);
        } catch (SerializationException exception) {
            throw new RuntimeException(exception);
        }
        return tab;
    }

    private static class TabInfo {
        final Component tab;
        final int index;
        TabInfo(Component tab, int index) {
            this.tab = tab;
            this.index = index;
        }
        public int hashCode() {
            return index * 13 + tab.hashCode();
        }
        public boolean equals(Object o) {
            if(!(o instanceof TabInfo))
                return false;
            TabInfo other = (TabInfo)o;
            return other.tab == tab && other.index == index;
        }
    }

    private Map<String, TabInfo> openedTabs = new HashMap<String, TabInfo>();

    private void createOrSelectTab(BinaryBlob blob) {
        if(!openedTabs.containsKey(blob.displayName)) {
            Component tab = loadTab(blob);
            final int tabIndex = tabPane.getTabs().getLength();
            openedTabs.put(blob.displayName, new TabInfo(tab, tabIndex));
            tabPane.getTabs().add(tab);
            TabPane.setLabel(tab, "#" + tabPane.getTabs().getLength() + " " + blob.displayName);
            tabPane.setSelectedIndex(tabIndex);
        } else {
            tabPane.setSelectedIndex(openedTabs.get(blob.displayName).index);
        }
    }


    public FridaApp() {
        Action.getNamedActions().put("fileNew", new Action() {
            @Override
            public void perform() {
                WTKXSerializer wtkxSerializer = new WTKXSerializer();
                Component tab;
                try {
                    BoxPane boxPane = (BoxPane)wtkxSerializer.readObject(this, "document.wtkx");
                    tab = new Border(boxPane);

                    TextInput archText = (TextInput)wtkxSerializer.get("archText");
                    archText.setMenuHandler(menuHandler);

                    TextInput subarchText = (TextInput)wtkxSerializer.get("subarchText");
                    subarchText.setMenuHandler(menuHandler);

                    PushButton saveButton = (PushButton)wtkxSerializer.get("saveButton");
                    saveButton.setMenuHandler(menuHandler);

                    wtkxSerializer.reset();
                    Component projectContents = new Border((Component)wtkxSerializer.readObject(this, "project-contents.wtkx"));
                    boxPane.add(projectContents);
                } catch (IOException exception) {
                    throw new RuntimeException(exception);
                } catch (SerializationException exception) {
                    throw new RuntimeException(exception);
                }

                tabPane.getTabs().add(tab);
                TabPane.setLabel(tab, "Project #" + tabPane.getTabs().getLength());
                tabPane.setSelectedIndex(tabPane.getTabs().getLength() - 1);
            }
        });

        Action.getNamedActions().put("fileOpen", new Action() {
            @Override
            public void perform() {
                //fileBrowserSheet.open(window);
            }
        });

        Action.getNamedActions().put("cut", new Action(false) {
            @Override
            public void perform() {
                TextInput textInput = (TextInput)window.getFocusDescendant();
                textInput.cut();
            }
        });

        Action.getNamedActions().put("copy", new Action(false) {
            @Override
            public void perform() {
                TextInput textInput = (TextInput)window.getFocusDescendant();
                textInput.copy();
            }
        });

        Action.getNamedActions().put("paste", new Action(false) {
            @Override
            public void perform() {
                TextInput textInput = (TextInput)window.getFocusDescendant();
                textInput.paste();
            }
        });
    }

    @Override
    public boolean shutdown(boolean optional) {
        if (window != null) {
            window.close();
        }

        return false;
    }

    @Override
    public void suspend() {
    }

    @Override
    public void resume() {
    }

    public static void main(String[] args) {
        DesktopApplicationContext.main(FridaApp.class, args);
    }
}
