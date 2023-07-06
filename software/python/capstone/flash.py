import sys
import math

import serial.tools.list_ports
from alive_progress import alive_bar

sys.path.insert(0, "firmware/build/nanopb/generator/proto/")
from capstone.proto.boot_pb2 import Request, Response

__FLASH_SECTOR_SIZE = 4096
# 64 kB erases are double the speed of 32 kB erases... for some reason.
__ERASE_SIZE = 16 * __FLASH_SECTOR_SIZE
__WRITE_SIZE = 128

def send_request(request, serial, wait_for_response=True):
    bin = request.SerializeToString()
    serial.write(len(bin).to_bytes(1, "big") + bin)
    if not wait_for_response:
        return
    count = int(serial.read()[0])
    response = Response.FromString(serial.read(count))
    return response

def get_boards():
    boards = []
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.product and port.product == "capstone-bootloader":
            boards.append(port.device)
    return boards

def flash(binary):
    boards = get_boards()
    serial_port = None
    if len(boards) == 0:
        raise Exception("No flashable devices found")
    elif len(boards) == 1:
        serial_port = serial.Serial(boards[0])
    else:
        print("Multiple flashable devices found:")
        for i, board in enumerate(boards):
            print(f"    [{i+1}] {board}")
        index = int(input("Select a device: ")) - 1
        serial_port = serial.Serial(boards[index])

    file = None
    with open(binary, "rb") as f:
        file = f.read()

    request = Request()
    request.erase.offset = 0
    request.erase.length = __ERASE_SIZE
    total_erase_length = math.ceil(len(file) / __ERASE_SIZE) * __ERASE_SIZE

    with alive_bar(int(total_erase_length / 1024), unit=" kB", manual=True, title="Erasing flash".ljust(len(binary))) as bar:
        for offset in range(0, len(file), __ERASE_SIZE):
            request.erase.offset = offset
            send_request(request, serial_port)
            bar((offset + __ERASE_SIZE) / total_erase_length)

    padding_len = __WRITE_SIZE - (len(file) % __WRITE_SIZE)
    file += bytes([0xFF] * padding_len)

    with alive_bar(int(len(file) / 1024), unit=" kB", manual=True, title=binary) as bar:
        for offset in range(0, len(file), __WRITE_SIZE):
            request.write.offset = offset
            request.write.data = file[offset:offset+__WRITE_SIZE]
            send_request(request, serial_port)
            bar((offset + 128) / len(file))

    request.go.SetInParent()
    send_request(request, serial_port, wait_for_response=False)
