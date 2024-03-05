import time

from serial.tools import list_ports

from capstone import values

def try_connect():
    ports = list_ports.comports()
    for port in ports:
        if port.serial_number and port.serial_number.startswith("capstone-app-"):
            return serial.Serial(port.device)
    return None

def main():
    from capstone.proto.app_pb2 import Request, Response

    while True:
        device = try_connect()
        if device is not None:
            click.secho("USB device connected", bold=True, fg="green")
        while device is not None and self.running:
            for tag in values.ValueTag:
                origin = self.values["values"][tag.name]["origin"]
                if origin == "drone":
                    request = Request()
                    request.get.tag = int(tag)
                elif origin == "ground":
                    request = Request()
                    request.set.tag = int(tag)
                    request.set.value.CopyFrom(current_values.get_proto(tag))
                else:
                    continue  # Error?

                try:
                    response = send_request(request, device, Response)
                except serial.serialutil.SerialException as e:
                    click.secho("USB device disconnected", bold=True, fg="red")
                    device = None
                    break

                if origin == "drone":
                    current_values.set_proto(tag, response.get.value)
            time.sleep(0.1)
        time.sleep(0.5)
