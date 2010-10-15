package org.boblycat.frida.ui


/**
 * Created: Mar 12, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk @boblycat.org
 */

import java.io.IOException
import org.apache.pivot.collections._
import org.apache.pivot.serialization.SerializationException
import org.apache.pivot.wtk._
import org.apache.pivot.wtkx.WTKXSerializer
import org.boblycat.frida.core._
import org.boblycat.frida.core.disassembler.Disassembler
import org.boblycat.frida.core.disassembler.DisassemblerFactory
import org.boblycat.frida.plugin.disassembler.mips.MIPSDisasm
import org.apache.pivot.wtk.MenuHandler.Adapter


object FridaApp {
  private class BinaryBlob(val displayName : String, val chunkId : ChunkId) {
    override def toString = displayName
  }

  def main(args: Array[String]): Unit = {
    DesktopApplicationContext.main(classOf[FridaApp], args)
  }

  private class TabInfo(val tab : Component, val index : Int) {
    override def hashCode: Int = {
      return index * 13 + tab.hashCode
    }
    
    override def equals(o : Any): Boolean = {
      if (!(o.isInstanceOf[TabInfo])) return false
      var other: TabInfo = o.asInstanceOf[TabInfo]
      return other.tab == tab && other.index == index
    }
  }

  class FridaApp extends Application {
    private def createOrSelectTab(blob: BinaryBlob): Unit = {
      if (!openedTabs.containsKey(blob.displayName)) {
        var tab: Component = loadTab(blob)
        val tabIndex: Int = tabPane.getTabs.getLength
        openedTabs.put(blob.displayName, new TabInfo(tab, tabIndex))
        tabPane.getTabs.add(tab)
        TabPane.setLabel(tab, "#" + tabPane.getTabs.getLength + " " + blob.displayName)
        tabPane.setSelectedIndex(tabIndex)
      }
      else {
        tabPane.setSelectedIndex(openedTabs.get(blob.displayName).index)
      }
    }


    def resume: Unit = {
    }

    def shutdown(optional: Boolean): Boolean = {
      FridaCore.shutdown
      if (window != null) {
        window.close
      }
      return false
    }

    private var menuHandler : MenuHandler = new Adapter {
      private var textInputSelectionListener: TextInputSelectionListener = new TextInputSelectionListener {
        def selectionChanged(textInput: TextInput, previousSelectionStart: Int, previousSelectionLength: Int): Unit = {
          updateActionState(textInput)
        }
      }

      override def cleanupMenuBar(component: Component, menuBar: MenuBar): Unit = {
        if (component.isInstanceOf[TextInput]) {
          var textInput: TextInput = component.asInstanceOf[TextInput]
          textInput.getTextInputTextListeners.remove(textInputTextListener)
          textInput.getTextInputSelectionListeners.remove(textInputSelectionListener)
        }
      }

      private def updateActionState(textInput: TextInput): Unit = {
        Action.getNamedActions.get("cut").setEnabled(textInput.getSelectionLength > 0)
        Action.getNamedActions.get("copy").setEnabled(textInput.getSelectionLength > 0)
      }

      private var textInputTextListener : TextInputTextListener = new TextInputTextListener {
        def textChanged(textInput: TextInput): Unit = {
          updateActionState(textInput)
        }
      }

      override def configureMenuBar(component: Component, menuBar: MenuBar): Unit = {
        if (component.isInstanceOf[TextInput]) {
          var textInput: TextInput = component.asInstanceOf[TextInput]
          updateActionState(textInput)
          Action.getNamedActions.get("paste").setEnabled(true)
          textInput.getTextInputTextListeners.add(textInputTextListener)
          textInput.getTextInputSelectionListeners.add(textInputSelectionListener)
        }
        else {
          Action.getNamedActions.get("cut").setEnabled(false)
          Action.getNamedActions.get("copy").setEnabled(false)
          Action.getNamedActions.get("paste").setEnabled(false)
        }
      }
    }

    private var tabPane: TabPane = null

    private def loadTab(blob: BinaryBlob): Component = {
      var wtkxSerializer = new WTKXSerializer
      var tab: Component = null
      try {
        var disasmTable: TableView = wtkxSerializer.readObject(this, "disasm.wtkx").asInstanceOf[TableView]
        MIPSDisasm.register
        var dis: Disassembler = DisassemblerFactory.create("mipsel")
        var res: CodeChunk = dis.disassemble(0, ChunkStore.getChunk(blob.chunkId).get.getBytes)
        var ls: List[Map[String, String]] = new ArrayList[Map[String, String]](res.length)
        for (ins <- res.getInstructions) {
          var m: Map[String, String] = new HashMap[String, String]
          m.put("address", String.format("0x%x", new java.lang.Long(ins.address)))
          m.put("instr", ins.instruction)
          m.put("args", ins.args)
          m.put("comment", ins.comment)
          ls.add(m)
        }
        disasmTable.setTableData(ls)
        tab = new Border(disasmTable)
      }
      catch {
        case exception: IOException => {
          throw new RuntimeException(exception)
        }
        case exception: SerializationException => {
          throw new RuntimeException(exception)
        }
      }
      return tab
    }

    private var window: Window = null

    def suspend: Unit = {
    }


    Action.getNamedActions.put("fileNew", new Action {
      def perform: Unit = {
        var wtkxSerializer: WTKXSerializer = new WTKXSerializer
        var tab: Component = null
        try {
          var boxPane: BoxPane = wtkxSerializer.readObject(this, "document.wtkx").asInstanceOf[BoxPane]
          tab = new Border(boxPane)
          var archText: TextInput = wtkxSerializer.get("archText").asInstanceOf[TextInput]
          archText.setMenuHandler(menuHandler)
          var subarchText: TextInput = wtkxSerializer.get("subarchText").asInstanceOf[TextInput]
          subarchText.setMenuHandler(menuHandler)
          var saveButton: PushButton = wtkxSerializer.get("saveButton").asInstanceOf[PushButton]
          saveButton.setMenuHandler(menuHandler)
          wtkxSerializer.reset
          var projectContents: Component = new Border(wtkxSerializer.readObject(this, "project-contents.wtkx").asInstanceOf[Component])
          boxPane.add(projectContents)
        }
        catch {
          case exception: IOException => {
            throw new RuntimeException(exception)
          }
          case exception: SerializationException => {
            throw new RuntimeException(exception)
          }
        }
        tabPane.getTabs.add(tab)
        TabPane.setLabel(tab, "Project #" + tabPane.getTabs.getLength)
        tabPane.setSelectedIndex(tabPane.getTabs.getLength - 1)
      }
    })
    Action.getNamedActions.put("fileOpen", new Action {
      def perform: Unit = {
        fileBrowserSheet.open(window,  new SheetCloseListener() {
          override def sheetClosed(sheet : Sheet) {
            val selectedFiles = fileBrowserSheet.getSelectedFiles();
            for(i <- 0 until selectedFiles.getLength) {
              val f = selectedFiles.get(i)
              println("adding " + f.getName)
              ChunkStore.addChunk(f.getName, new BinaryCodeChunk(0, FileUtils.loadBytes(f)))
            }
            refreshProjectContent
          }})
      }
    })

    Action.getNamedActions.put("cut", new Action((false)) {
      def perform: Unit = {
        var textInput: TextInput = window.getFocusDescendant.asInstanceOf[TextInput]
        textInput.cut
      }
    })
    Action.getNamedActions.put("copy", new Action((false)) {
      def perform: Unit = {
        var textInput: TextInput = window.getFocusDescendant.asInstanceOf[TextInput]
        textInput.copy
      }
    })
    Action.getNamedActions.put("paste", new Action((false)) {
      def perform: Unit = {
        var textInput: TextInput = window.getFocusDescendant.asInstanceOf[TextInput]
        textInput.paste
      }
    })
    

    private var fileBrowserSheet : FileBrowserSheet = null

    def refreshProjectContent() {
      var ls: List[BinaryBlob] = new ArrayList[BinaryBlob]
      for((n, id) <- ChunkStore.listChunkNames) {
        println("Chunk info " + n + " " + id)
        ls.add(new BinaryBlob(n, id))
      }
      projectContent.setListData(ls)
    }
    private var projectContent : ListView = null
    
    def startup(display: Display, properties: Map[String, String]): Unit = {
      FridaCore.startup
      var wtkxSerializer: WTKXSerializer = new WTKXSerializer
      window = wtkxSerializer.readObject(this, "frida.wtkx").asInstanceOf[Window]
      tabPane = wtkxSerializer.get("tabPane").asInstanceOf[TabPane]
      projectContent = wtkxSerializer.get("projectContent").asInstanceOf[ListView]
      refreshProjectContent
      projectContent.getListViewSelectionListeners.add(new ListViewSelectionListener {
        override def selectedRangeAdded(listView: ListView, rangeStart: Int, rangeEnd: Int): Unit = {
          println("added")
        }

        override def selectedRangesChanged(listView: ListView, previousSelectedRanges: Sequence[Span]): Unit = {
          var selectedItems = listView.getSelectedItems
          for(i <- 0 until selectedItems.getLength)
            createOrSelectTab(selectedItems.get(i).asInstanceOf[BinaryBlob])
        }

        override def selectedRangeRemoved(listView: ListView, rangeStart: Int, rangeEnd: Int): Unit = {
          println("removed")
        }
      })
      fileBrowserSheet = new FileBrowserSheet(FileBrowserSheet.Mode.OPEN)
      window.open(display)
    }

    private var openedTabs: Map[String, TabInfo] = new HashMap[String, TabInfo]
  }
}

