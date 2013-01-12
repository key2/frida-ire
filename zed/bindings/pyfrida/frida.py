
import _frida
import threading
import traceback


class Error(Exception):
    pass

def execute_script(script):

    def msg(message, data):
        if message['type'] == 'send':
            result['data'] = message['payload']
        elif message['type'] == 'error':
            result['error'] = message['description']
        event.set()

    result = {}
    event = threading.Event()

    script.on('message', msg)
    script.load()
    event.wait()
    script.unload()

    if 'error' in result:
        raise Error, result['error']

    return result['data']

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
        exports.push({name: name, address: address});
    },
    onComplete: function() {
        send(exports);
    }
});
""" % self.name)

        result = execute_script(script)
        for export in result:
          yield Export(export["name"], export["address"])

class Export:
    def __init__(self, name, address):
        self.name = name
        self.address = address

class Process:

    def __init__(self, session):
        self._session = session

    def enumerate_modules(self):
        script = self._session.create_script(
"""
var modules = [];
Process.enumerateModules({
    onMatch: function(name, address, path) {
        modules.push({name: name, address: address, path: path});
    },
    onComplete: function() {
        send(modules);
    }
});
""")

        result = execute_script(script)

        for data in result:
          yield Module(data['name'], data['address'], data['path'], self._session)


def attach(pid, device_id = None):
    return Process(_frida.attach(pid))


