import math
import os

import serial
from alive_progress import alive_bar
from capstone.tools.usb import send_request

__FLASH_SECTOR_SIZE = 4096
# 64 kB erases are double the speed of 32 kB erases... for some reason.
__ERASE_SIZE = 16 * __FLASH_SECTOR_SIZE
__WRITE_SIZE = 128


def flash(bin_file_path, dev_path):
    serial_port = serial.Serial(dev_path)
    with open(bin_file_path, "rb") as bin_file:
        binary = bin_file.read()

        # pylint: disable=import-outside-toplevel,no-name-in-module
        from capstone.proto.boot_pb2 import Request, Response

        request = Request()
        request.erase.offset = 0
        request.erase.length = __ERASE_SIZE
        total_erase_length = math.ceil(len(binary) / __ERASE_SIZE) * __ERASE_SIZE

        erase_title = "Erasing flash"
        flash_title = f"Writing {os.path.basename(bin_file_path)}"
        title_len = max(len(erase_title), len(flash_title))
        erase_title = erase_title.ljust(title_len)
        flash_title = flash_title.ljust(title_len)

        with alive_bar(
            int(total_erase_length / 1024),
            unit=" kB",
            manual=True,
            title=erase_title,
        ) as progress_bar:
            for offset in range(0, len(binary), __ERASE_SIZE):
                request.erase.offset = offset
                send_request(request, serial_port, Response)
                progress_bar(  # pylint: disable=not-callable
                    (offset + __ERASE_SIZE) / total_erase_length
                )

        padding_len = __WRITE_SIZE - (len(binary) % __WRITE_SIZE)
        binary += bytes([0xFF] * padding_len)

        with alive_bar(
            int(len(binary) / 1024), unit=" kB", manual=True, title=flash_title
        ) as progress_bar:
            for offset in range(0, len(binary), __WRITE_SIZE):
                request.write.offset = offset
                request.write.data = binary[offset : offset + __WRITE_SIZE]
                send_request(request, serial_port, Response)
                progress_bar(  # pylint: disable=not-callable
                    (offset + __WRITE_SIZE) / len(binary)
                )

    request.go.SetInParent()
    send_request(request, serial_port, Response, wait_for_response=False)
