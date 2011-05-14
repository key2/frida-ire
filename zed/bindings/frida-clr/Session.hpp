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
  ref class GstPadStatsEventArgs;
  ref class GstPadStats;
  public delegate void GstPadStatsHandler (Object ^ sender, GstPadStatsEventArgs ^ e);

  public value struct Address
  {
    UInt64 Value;

    Address (UInt64 val)
      : Value (val)
    {
    }
  };

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
    event GstPadStatsHandler ^ GstPadStats;

    Session (void * handle, Dispatcher ^ dispatcher);
    ~Session ();

    void Close ();

    Script ^ LoadScript (String ^ text);

    Address ResolveModuleBase (String ^ moduleName);
    Address ResolveModuleExport (String ^ moduleName, String ^ symbolName);
    array<Address> ^ ScanModuleForCodePattern (String ^ moduleName, String ^ pattern);

    void InvokeFunction (Address address, String ^ arguments);

    void AddGLogPattern (String ^ pattern, LogLevel levels);
    void ClearGLogPatterns ();

    void EnableGMainWatchdog (double maxDuration);
    void DisableGMainWatchdog ();

    void EnableGstMonitor ();
    void DisableGstMonitor ();

    void OnClosed (Object ^ sender, EventArgs ^ e);
    void OnLogMessage (Object ^ sender, LogMessageEventArgs ^ e);
    void OnGstPadStats (Object ^ sender, GstPadStatsEventArgs ^ e);

  private:
    FridaSession * handle;
    msclr::gcroot<Session ^> * selfHandle;

    Dispatcher ^ dispatcher;
    EventHandler ^ onClosedHandler;
    LogMessageHandler ^ onLogMessageHandler;
    GstPadStatsHandler ^ onGstPadStatsHandler;
  };

  public ref class Script
  {
  public:
    event ScriptMessageHandler ^ Message;

    Script (FridaScript * handle, Dispatcher ^ dispatcher);
    ~Script ();

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

  public ref class LogMessageEventArgs : public EventArgs
  {
  public:
    property TimeSpan TimeStamp { TimeSpan get () { return timeStamp; } };
    property String ^ Domain { String ^ get () { return domain; } };
    property LogLevel Level { LogLevel get () { return level; } };
    property String ^ Message { String ^ get () { return message; } };

    LogMessageEventArgs (TimeSpan timeStamp, String ^ domain, LogLevel level, String ^ message)
    {
      this->timeStamp = timeStamp;
      this->domain = domain;
      this->level = level;
      this->message = message;
    }

  private:
    TimeSpan timeStamp;
    String ^ domain;
    LogLevel level;
    String ^ message;
  };

  public ref class GstPadStatsEventArgs : public EventArgs
  {
  public:
    property array <GstPadStats ^> ^ Entries { array <GstPadStats ^> ^ get () { return entries; } };

    GstPadStatsEventArgs (array <GstPadStats ^> ^ entries)
    {
      this->entries = entries;
    }

  private:
    array <GstPadStats ^> ^ entries;
  };

  public ref class GstPadStats
  {
  public:
    property String ^ PadName { String ^ get () { return padName; } };
    property double BuffersPerSecond { double get () { return buffersPerSecond; } };
    property String ^ TimingHistory { String ^ get () { return timingHistory; } }

    GstPadStats (String ^ padName, double buffersPerSecond, String ^ timingHistory)
    {
      this->padName = padName;
      this->buffersPerSecond = buffersPerSecond;
      this->timingHistory = timingHistory;
    }

  private:
    String ^ padName;
    double buffersPerSecond;
    String ^ timingHistory;
  };
}
