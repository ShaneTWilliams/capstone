from serial.tools import list_ports
import serial

class UARTRadio:
    def __init__(self, baudrate=115200):
        ports = list_ports.comports()
        self.devs = []
        for port in ports:
            if port.serial_number and port.serial_number == "E6625C4893745B30":
                self.devs.append(serial.Serial(port.device, baudrate=baudrate))
        if len(self.devs) == 0:
            raise Exception("USB -> UART not found")
        elif len(self.devs) == 1:
            raise Exception("Only one USB -> UART found")

    def send(self, data):
        for dev in self.devs:
            dev.write(bytes(data, "ascii"))

    def recv(self):
        for dev in self.devs:
            if dev.in_waiting > 0:
                return dev.read()
        return None

class Radio:
    def __init__(self):
        pass

    def send(self, data):
        print(data)

    def recv(self):
        return bytes()
