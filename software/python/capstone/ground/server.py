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
from serial.tools import list_ports
from capstone.ground.radio import Radio

current_values = CurrentValues()


class Drone:
    def __init__(self, values):
        self.usb_thread = threading.Thread(target=self.usb_thread_func)
        self.radio_thread = threading.Thread(target=self.radio_thread_func)
        self.running = True
        self.values = values
        #self.radio = Radio()

    def try_connect(self):
        ports = list_ports.comports()
        for port in ports:
            if port.serial_number and port.serial_number.startswith("capstone-app-"):
                return serial.Serial(port.device)
        return None

    def usb_thread_func(self):
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

    def radio_thread_func(self):
        from capstone import values

        recv_data = bytearray()
        count = 0
        while self.running:
            self.radio.send("123456789")
            tx_time = time.time()
            while time.time() - tx_time < 0.05:
                data = self.radio.recv()
                if data is None:
                    continue

                recv_data.extend(data)
                if len(recv_data) >= 9:
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
        self.usb_thread.start()
        #self.radio_thread.start()

    def stop(self):
        self.running = False
        try:
            self.usb_thread.join()
            #self.radio_thread.join()
        except KeyboardInterrupt:
            pass


def main(values):
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

    drone = Drone(values)
    drone.start()

    server.start()
    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        server.stop(0)
    drone.stop()
