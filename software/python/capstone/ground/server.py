import os
import threading
import time
from concurrent import futures

import click
import grpc
import serial
from serial.tools import list_ports

from capstone.constants import GRPC_SERVER_PORT
from capstone.proto import ground_pb2_grpc
from capstone.values import CurrentValues, ValueTag, VALUE_CONFIGS
from google.protobuf.empty_pb2 import Empty
from capstone.ground.radio import SX1262Radio
from capstone.tools.usb import send_request
# from capstone.ground.io import CapstoneGPIO, CapstoneSPI, CapstonePin, GPS_UART_DEV, GPS_UART_BAUD

current_values = CurrentValues()


class BaseStation:
    def __init__(self):
        self.thread_funcs = [
            # self.radio_thread_func,
            # self.gps_thread_func,
            self.usb_thread_func
        ]
        self.running = True
        self.threads = []

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
            tx_data = current_values.pack_uplink_packet_0()
            self.radio.transmit(tx_data)
            while not self.radio.tx_done():
                pass
            tx_time = time.time()

            self.radio.set_receive()
            while time.time() - tx_time < 0.05:
                if not self.radio.rx_done():
                    continue

                rx_data = self.radio.get_receive_data()
                if len(rx_data) == 0:
                    break
                print(bytes(rx_data).hex(), f"{count:05}")
                try:
                    current_values.unpack_downlink_packet(rx_data[1:9], int(rx_data[0]))
                except KeyError as e:
                    click.secho(f"Unknown tag: {e}", bold=True, fg="red")
                count += 1
                break

    def gps_thread_func(self):
        dev = serial.Serial(GPS_UART_DEV, baudrate=GPS_UART_BAUD)
        gps_pin = CapstoneGPIO(CapstonePin.GPS_RESET)
        gps_pin.set(False)
        time.sleep(0.01)
        gps_pin.set(True)

        while self.running:
            try:
                line = dev.readline().decode("ascii").split(",")
            except UnicodeDecodeError:
                continue
            if line[0] == "$GNGGA":
                try:
                    current_values.set(ValueTag.BASE_LATITUDE, float(line[2]))
                    current_values.set(ValueTag.BASE_LONGITUDE, float(line[4]))
                except Exception as e:
                    print(e)
                    continue

    def usb_thread_func(self):
        from capstone import values
        from capstone.proto.app_pb2 import Request, Response

        def try_connect():
            ports = list_ports.comports()
            for port in ports:
                if port.serial_number and port.serial_number.startswith("capstone-app-"):
                    return serial.Serial(port.device)
            return None

        while self.running:
            device = try_connect()
            if device is not None:
                click.secho("USB device connected", bold=True, fg="green")
            while device is not None and self.running:
                for tag in values.ValueTag:
                    origin = VALUE_CONFIGS[tag]["origin"]
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

    def start(self):
        for thread_func in self.thread_funcs:
            self.threads.append(threading.Thread(target=thread_func))

        for thread in self.threads:
            thread.start()


    def stop(self):
        self.running = False
        for thread in self.threads:
            thread.join()



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
