import os
import threading
import time
from concurrent import futures

import click
import grpc
import serial
from capstone.constants import GRPC_SERVER_PORT
from capstone.tools.usb import send_request
from capstone.values import CurrentValues
from serial.tools import list_ports

current_values = CurrentValues()


class DroneUsb:
    def __init__(self, values):
        self.thread = threading.Thread(target=self.thread_func)
        self.running = True
        self.values = values

    def try_connect(self):
        ports = list_ports.comports()
        for port in ports:
            if port.serial_number and port.serial_number.startswith("capstone-app-"):
                return serial.Serial(port.device)
        return None

    def thread_func(self):
        from capstone.proto.app_pb2 import Request, Response

        from capstone import values

        while self.running:
            device = self.try_connect()
            if device is not None:
                click.secho("USB device connected", bold=True, fg="green")
            while device is not None and self.running:
                for tag in values.ValueTag:
                    origin = self.values["values"][tag.name]["origin"]
                    if origin == "drone":
                        request = Request()
                        request.get.tag = int(tag)

                    else:
                        request = Request()
                        request.set.tag = int(tag)
                        request.set.value.CopyFrom(current_values.get_proto(tag))

                    try:
                        response = send_request(request, device, Response)
                    except serial.serialutil.SerialException as e:
                        click.secho("USB device disconnected", bold=True, fg="red")
                        device = None
                        break

                    if origin == "drone":
                        current_values.set_proto(tag, response.get.value)
                time.sleep(0.01)
            time.sleep(0.1)

    def start(self):
        self.thread.start()

    def stop(self):
        self.running = False
        try:
            self.thread.join()
        except KeyboardInterrupt:
            pass


def main(values):
    from capstone.proto import ground_pb2_grpc
    from google.protobuf.empty_pb2 import Empty

    class GroundStationServicer(ground_pb2_grpc.GroundStationServicer):
        def GetValue(self, request, context):
            return current_values.get_proto(request.tag)

        def SetValue(self, request, context):
            current_values.set_proto(request.tag, request.value)
            return Empty()

        def SetController(self, request, context):
            print(request)
            return Empty()

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    ground_pb2_grpc.add_GroundStationServicer_to_server(GroundStationServicer(), server)
    server.add_insecure_port(f"localhost:{GRPC_SERVER_PORT}")

    usb = DroneUsb(values)
    usb.start()

    server.start()
    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        server.stop(0)
    usb.stop()
