#pragma once

#using <WindowsBase.dll>

#include <msclr/gcroot.h>
#include <zed-core.h>

using namespace System;
using System::Windows::Threading::Dispatcher;

namespace Frida
{
  ref class Script;
  value struct ScriptId;
  ref class ScriptMessageEventArgs;
  public delegate void ScriptMessageHandler (Object ^ sender, ScriptMessageEventArgs ^ e);

  public value struct Address
  {
    UInt64 Value;

    Address (UInt64 val)
      : Value (val)
    {
    }
  };

  public ref class Session
  {
  public:
    event EventHandler ^ Closed;

    Session (void * handle, Dispatcher ^ dispatcher);
    ~Session ();

    void Close ();

    Script ^ CreateScript (String ^ source);

    void OnClosed (Object ^ sender, EventArgs ^ e);

  private:
    FridaSession * handle;
    msclr::gcroot<Session ^> * selfHandle;

    Dispatcher ^ dispatcher;
    EventHandler ^ onClosedHandler;
  };

  public ref class Script
  {
  public:
    event ScriptMessageHandler ^ Message;

    Script (FridaScript * handle, Dispatcher ^ dispatcher);
    ~Script ();

    void Load ();
    void Unload ();

    void OnMessage (Object ^ sender, ScriptMessageEventArgs ^ e);

  private:
    FridaScript * handle;
    msclr::gcroot<Script ^> * selfHandle;

    Dispatcher ^ dispatcher;
    ScriptMessageHandler ^ onMessageHandler;
  };

  public ref class ScriptMessageEventArgs : public EventArgs
  {
  public:
    property String ^ Message { String ^ get () { return message; } };

    ScriptMessageEventArgs (String ^ message)
    {
      this->message = message;
    }

  private:
    String ^ message;
  };
}
