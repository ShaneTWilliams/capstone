import click
import serial
from serial.tools import list_ports


def prompt_and_get_board(device_type):
    devices = []
    ports = list_ports.comports()
    for port in ports:
        if port.serial_number and port.serial_number.startswith(
            f"capstone-{device_type}-"
        ):
            devices.append(port.device)

    if len(devices) == 0:
        return None
    else:
        if len(devices) == 1:
            device = devices[0]
        else:
            click.secho("Multiple flashable devices found:")
            for i, device in enumerate(devices):
                click.secho(f"    [{i+1}] {device}")
            index = int(input("Select a device: ")) - 1
            device = devices[index]

    return device


def send_request(request, serial_port, response_class, wait_for_response=True):
    binary = request.SerializeToString()
    serial_port.write(len(binary).to_bytes(1, "big") + binary)
    if not wait_for_response:
        return None
    count = int(serial_port.read()[0])

    data = serial_port.read(count)
    response = response_class.FromString(data)
    return response
