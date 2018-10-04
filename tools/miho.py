import socket

import cbor2


_subsystems = {}
def _subsystem(name, attr):
    def add_subsystem(cls):
        _subsystems[name] = (cls, attr)
        return cls
    return add_subsystem


class Miho:
    def __init__(self, addr):
        self.sock = socket.socket()
        self.sock.connect(addr)

        self.subsystems = {}
        for i, name in enumerate(self.query(0, 0), 1):
            self.subsystems[name] = i
            if name in _subsystems:
                cls, attr = _subsystems[name]
                setattr(self, attr, cls(self))

    def close(self):
        self.sock.close()
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def send(self, subsystem, command, *args):
        if isinstance(subsystem, str):
            subsystem = self.subsystems[subsystem]
        
        cmd = [subsystem, command, *args]
        self.sock.sendall(cbor2.dumps(cmd))

    def recv(self):
        data = self.sock.recv(1024)
        return cbor2.loads(data)

    def query(self, subsystem, command, *args):
        self.send(subsystem, command, *args)
        return self.recv()


@_subsystem(':mouse', 'mouse')
class MouseSubsystem:
    MOUSE_MOVE = 3
    MOUSE_BUTTON = 4
    BUTTONS = {'left': 1, 'right': 2, 'middle': 3}

    def __init__(self, miho):
        self.miho = miho
        self.id = miho.subsystems.get(':mouse', ':mouse')
    
    def move(self, dx, dy):
        self.miho.send(self.id, MouseSubsystem.MOUSE_MOVE, dx, dy)
    
    def button(self, btn, down):
        btn = MouseSubsystem.BUTTONS.get(btn, btn)
        btn -= 1
        self.miho.send(self.id, MouseSubsystem.MOUSE_BUTTON, btn, down)

    def click(self, btn):
        self.button(btn, True)
        self.button(btn, False)
