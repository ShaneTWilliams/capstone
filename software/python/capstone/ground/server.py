import os
import threading
import time
from concurrent import futures

import click
import grpc
import serial

from capstone.constants import GRPC_SERVER_PORT
from capstone.proto import ground_pb2_grpc
from capstone.tools.usb import send_request
from capstone.values import CurrentValues, ValueTag
from google.protobuf.empty_pb2 import Empty
from capstone.ground.radio import SX1262Radio
from capstone.ground.io import CapstoneGPIO, CapstoneSPI, CapstonePin

current_values = CurrentValues()


class BaseStation:
    def __init__(self):
        self.radio_thread = threading.Thread(target=self.radio_thread_func)
        self.running = True

    def radio_thread_func(self):
        from capstone import values

        busy_pin = CapstoneGPIO(CapstonePin.DIGITAL_RADIO_BUSY)
        reset_pin = CapstoneGPIO(CapstonePin.DIGITAL_RADIO_RESET)
        spi = CapstoneSPI()
        self.radio = SX1262Radio(spi, busy_pin, reset_pin)
        self.radio.reset()
        self.radio.init()

        count = 0
        while self.running:
            self.radio.transmit([1, 2, 3, 4, 5, 6, 7, 8])
            while not self.radio.tx_done():
                pass
            tx_time = time.time()

            self.radio.set_receive()
            while time.time() - tx_time < 0.05:
                if not self.radio.rx_done():
                    continue

                recv_data = self.radio.get_receive_data()
                print(bytes(recv_data).hex(), f"{count:05}")
                try:
                    current_values.unpack_downlink_packet(recv_data[1:9], int(recv_data[0]))
                except KeyError as e:
                    click.secho(f"Unknown tag: {e}", bold=True, fg="red")
                count += 1
                recv_data = bytearray()
                print(current_values._values)
                break

    def start(self):
        self.radio_thread.start()

    def stop(self):
        self.running = False
        try:
            self.radio_thread.join()
        except KeyboardInterrupt:
            pass


def main():
    class GroundStationServicer(ground_pb2_grpc.GroundStationServicer):
        def GetValue(self, request, context):
            return current_values.get_proto(request.tag)

        def SetValue(self, request, context):
            current_values.set_proto(request.tag, request.value)
            return Empty()

        def SetController(self, request, context):
            current_values.set(ValueTag.THROTTLE, request.rt)
            current_values.set(ValueTag.IGNITION, request.menu)
            return Empty()

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    ground_pb2_grpc.add_GroundStationServicer_to_server(GroundStationServicer(), server)
    server.add_insecure_port(f"localhost:{GRPC_SERVER_PORT}")

    base_station = BaseStation()
    base_station.start()

    server.start()
    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        server.stop(0)
    base_station.stop()
