import sys
import _frida
import threading
import traceback
import subprocess

if sys.platform == 'win32':
  def find_process_names():
    process_names = {}
    pl = subprocess.Popen(["tasklist","/FO","CSV"],stdout=subprocess.PIPE).communicate()[0]
    for line in pl.split('\n'):
      tmp = line.split(',')
      try:
        cmd = tmp[0].split('"')[1]
        pid = int(tmp[1].split('"')[1])
        process_names[pid] = cmd
      except:
        pass
    return process_names

if sys.platform == 'darwin':
  def find_process_names():
      """Return a dict of `pid`:`process name` pairs."""
      process_names = {}
      pl = subprocess.Popen(['ps', 'axo', 'pid,command'], stdout=subprocess.PIPE).communicate()[0]
      for line in pl.split('\n'):
          l = line.strip()
          if len(l) == 0:
              continue
          s = l.find(" ")
          try:
              pid = int(l[0:s])
              cmd = l[s:]
              process_names[pid] = cmd
          except:
              pass
      return process_names

if sys.platform == 'linux2':
  def find_process_names():
      """Return a dict of `pid`:`process name` pairs."""
      process_names = {}
      pl = subprocess.Popen(['ps', '-eo', 'pid,cmd'], stdout=subprocess.PIPE).communicate()[0]
      for line in pl.split('\n'):
          l = line.strip()
          if len(l) == 0:
              continue
          s = l.find(" ")
          try:
              pid = int(l[0:s])
              cmd = l[s:]
              process_names[pid] = cmd
          except:
              pass
      return process_names

# FIXME add similar code for OSX and Windows

def find_pid(process_name):
    """Find first pid with where the process name contains the string given by `process_name`."""
    process_names = find_process_names()
    for key, value in process_names.iteritems():
        if process_name in value:
            return key
    return -1

def attach(pid, device_id = None):
    return Process(_frida.attach(pid))

class Process:
    def __init__(self, session):
        self._session = session

    def enumerate_modules(self):
        script = self._session.create_script(
"""
var modules = [];
Process.enumerateModules({
    onMatch: function(name, address, path) {
        modules.push({name: name, address: address.toString(), path: path});
    },
    onComplete: function() {
        send(modules);
    }
});
""")
        for data in execute_script(script):
            yield Module(data['name'], int(data['address']), data['path'], self._session)

    """
      @param protection example '--x'
    """
    def enumerate_ranges(self, protection):
        script = self._session.create_script(
"""
var ranges = [];
Process.enumerateRanges(\"%s\", {
    onMatch: function(address, size, protection) {
        ranges.push({address: address.toString(), size: size, protection: protection});
    },
    onComplete: function() {
        send(ranges);
    }
});
""" % protection)
        for data in execute_script(script):
            yield Range(int(data['address']), data['size'], data['protection'])

    def _exec_script(self, script_source, post_hook = None):
        script = self._session.create_script(script_source)
        return execute_script(script, post_hook)

    def find_base_address(self, module_name):
        return int(self._exec_script("send(Module.findBaseAddress(\"%s\").toString());" % module_name))

    def read_bytes(self, address, length):
        return self._exec_script("send(null, Memory.readByteArray(ptr(\"%u\"), %u));" % (address, length))

    def read_utf8(self, address, length = -1):
        return self._exec_script("send(Memory.readUtf8String(ptr(\"%u\"), %u));" % (address, length))

    def write_bytes(self, address, bytes):
        script = \
"""
recv(function(bytes) {
    var base = ptr("%u");
    for (var i = 0; i < bytes.length; i++)
        Memory.writeU8(base.add(i), bytes[i]);
    send(true);
});
""" % address

        def send_data(script):
            script.post_message([ord(x) for x in bytes])

        return self._exec_script(script, send_data)

    def write_utf8(self, address, string):
        script = \
"""
recv(function(string) {
    Memory.writeUtf8String(ptr("%u"), string);
    send(true);
});
""" % address

        def send_data(script):
            script.post_message(string)

        return self._exec_script(script, send_data)

class Module:
    def __init__(self, name, address, path, _session):
        self.name = name
        self.address = address
        self.path = path
        self._session = _session

    def enumerate_exports(self):
        script = self._session.create_script(
"""
var exports = [];
Module.enumerateExports(\"%s\", {
    onMatch: function(name, address) {
        exports.push({name: name, address: address.toString()});
    },
    onComplete: function() {
        send(exports);
    }
});
""" % self.name)
        for export in execute_script(script):
            yield Export(export["name"], int(export["address"]))

    """
      @param protection example '--x'
    """
    def enumerate_ranges(self, protection):
        script = self._session.create_script(
"""
var ranges = [];
Module.enumerateRanges(\"%s\", \"%s\", {
    onMatch: function(address, size, protection) {
        ranges.push({address: address.toString(), size: size, protection: protection});
    },
    onComplete: function() {
        send(ranges);
    }
});
""" % (self.name, protection))
        for data in execute_script(script):
            yield Range(int(data['address']), data['size'], data['protection'])

class Export:
    def __init__(self, name, address):
        self.name = name
        self.address = address

class Range:
    def __init__(self, address, size, protection):
        self.address = address
        self.size = size
        self.protection = protection

class Error(Exception):
    pass

def execute_script(script, post_hook = None):
    def msg(message, data):
        if message['type'] == 'send':
            if data is not None:
                result['data'] = data
            else:
                result['data'] = message['payload']
        elif message['type'] == 'error':
            result['error'] = message['description']
        event.set()

    result = {}
    event = threading.Event()

    script.on('message', msg)
    script.load()
    if post_hook:
        post_hook(script)
    event.wait()
    script.unload()

    if 'error' in result:
        raise Error, result['error']

    return result['data']
