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
  ref class LogMessageEventArgs;
  public delegate void LogMessageHandler (Object ^ sender, LogMessageEventArgs ^ e);
  value struct Address;

  [Flags]
  public enum struct LogLevel : UInt32
  {
    FlagRecursion = 1 << 0,
    FlagFatal     = 1 << 1,

    Error         = 1 << 2,
    Critical      = 1 << 3,
    Warning       = 1 << 4,
    Message       = 1 << 5,
    Info          = 1 << 6,
    Debug         = 1 << 7,
  };

  public ref class Session
  {
  public:
    event EventHandler ^ Closed;
    event LogMessageHandler ^ LogMessage;

    Session (void * handle, Dispatcher ^ dispatcher);
    ~Session ();

    void Close ();

    Script ^ CompileScript (String ^ text);

    Address ResolveModuleBase (String ^ moduleName);
    Address ResolveModuleExport (String ^ moduleName, String ^ symbolName);

    void AddGLogPattern (String ^ pattern, LogLevel levels);
    void ClearGLogPatterns ();

    void EnableGMainWatchdog (double maxDuration);
    void DisableGMainWatchdog ();

    void OnClosed (Object ^ sender, EventArgs ^ e);
    void OnLogMessage (Object ^ sender, LogMessageEventArgs ^ e);

  private:
    FridaSession * handle;
    msclr::gcroot<Session ^> * selfHandle;

    Dispatcher ^ dispatcher;
    EventHandler ^ onClosedHandler;
    LogMessageHandler ^ onLogMessageHandler;
  };

  public ref class Script
  {
  public:
    event ScriptMessageHandler ^ Message;

    Script (FridaScript * handle, Dispatcher ^ dispatcher);
    ~Script ();

    void Destroy ();

    void AttachTo (Address address);

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

  public ref class LogMessageEventArgs : public EventArgs
  {
  public:
    property String ^ Domain { String ^ get () { return domain; } };
    property LogLevel Level { LogLevel get () { return level; } };
    property String ^ Message { String ^ get () { return message; } };

    LogMessageEventArgs (String ^ domain, LogLevel level, String ^ message)
    {
      this->domain = domain;
      this->level = level;
      this->message = message;
    }

  private:
    String ^ domain;
    LogLevel level;
    String ^ message;
  };

  public value struct Address
  {
    UInt64 Value;

    Address (UInt64 val)
      : Value (val)
    {
    }
  };
}
