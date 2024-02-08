import time

import grpc
import pygame
from google.protobuf.empty_pb2 import Empty
import click

from capstone.proto import ground_pb2, ground_pb2_grpc
from capstone.constants import GRPC_SERVER_PORT


class XboxOneController:
    init = False

    BUTTON_TO_STR = {
        0: "A",
        1: "B",
        2: "X",
        3: "Y",
        4: "VIEW",
        5: "XBOX",
        6: "MENU",
        7: "L3",
        8: "R3",
        9: "LB",
        10: "RB",
        11: "UP",
        12: "DOWN",
        13: "LEFT",
        14: "RIGHT",
        15: "SHARE"
    }

    AXIS_TO_STR = {
        0: "LX",
        1: "LY",
        2: "RX",
        3: "RY",
        4: "LT",
        5: "RT",
    }

    def __init__(self):
        if not self.init:
            self.init = True
            pygame.init()

        self.joystick = pygame.joystick.Joystick(0)
        self.joystick.init()

        self.state = {
            "A": False,
            "B": False,
            "X": False,
            "Y": False,
            "LB": False,
            "RB": False,
            "LT": 0,
            "RT": 0,
            "XBOX": False,
            "VIEW": False,
            "MENU": False,
            "L3": False,
            "R3": False,
            "UP": False,
            "DOWN": False,
            "LEFT": False,
            "RIGHT": False,
            "LX": 0,
            "LY": 0,
            "RX": 0,
            "RY": 0,
        }

    def update(self):
        for event in pygame.event.get():
            if event.type == pygame.JOYAXISMOTION:
                self.state[self.AXIS_TO_STR[event.axis]] = event.value
            elif event.type == pygame.JOYBUTTONDOWN:
                self.state[self.BUTTON_TO_STR[event.button]] = True
            elif event.type == pygame.JOYBUTTONUP:
                self.state[self.BUTTON_TO_STR[event.button]] = False


def main():
    while True:
        controller_disconnect_time = None
        while True:
            try:
                controller = XboxOneController()
                controller_disconnect_time = None
                break
            except pygame.error:
                if controller_disconnect_time is None:
                    controller_disconnect_time = time.time()
                    click.secho("Controller disconnected", bold=True, fg="red")
                    continue
                elapsed = time.time() - controller_disconnect_time
                click.secho(f"\r[{round(elapsed):04}]: Waiting for controller...", bold=True, fg="red", nl=False)
                time.sleep(0.1)
                continue

        click.secho("Controller connected", bold=True, fg="green")

        server_disconnect_time = None
        state = None

        channel = grpc.insecure_channel(f"localhost:{GRPC_SERVER_PORT}")
        stub = ground_pb2_grpc.GroundStationStub(channel)
        request = ground_pb2.SetControllerState()

        while True:
            try:
                controller.update()
            except pygame.error:
                break

            new_state = controller.state
            if new_state == state:
                continue

            state = new_state.copy()

            request.a = controller.state["A"]
            request.b = controller.state["B"]
            request.x = controller.state["X"]
            request.y = controller.state["Y"]
            request.lb = controller.state["LB"]
            request.rb = controller.state["RB"]
            request.lt = controller.state["LT"]
            request.rt = controller.state["RT"]
            request.xbox = controller.state["XBOX"]
            request.view = controller.state["VIEW"]
            request.menu = controller.state["MENU"]
            request.l3 = controller.state["L3"]
            request.r3 = controller.state["R3"]

            if controller.state["UP"]:
                request.pad = ground_pb2.PadState.UP
            elif controller.state["DOWN"]:
                request.pad = ground_pb2.PadState.DOWN
            elif controller.state["LEFT"]:
                request.pad = ground_pb2.PadState.LEFT
            elif controller.state["RIGHT"]:
                request.pad = ground_pb2.PadState.RIGHT
            else:
                request.pad = ground_pb2.PadState.NONE

            request.left_stick.x = controller.state["LX"]
            request.left_stick.y = controller.state["LY"]
            request.right_stick.x = controller.state["RX"]
            request.right_stick.y = controller.state["RY"]

            try:
                ret = stub.SetController(request)
                assert type(ret) == Empty
                if server_disconnect_time is not None:
                    click.secho("\nReconnected to server", bold=True, fg="green")
                    server_disconnect_time = None
            except grpc.RpcError as e:
                if server_disconnect_time is None:
                    server_disconnect_time = time.time()
                    click.secho("Lost connection to server", bold=True, fg="red")
                    continue
                server_disconnect_time = time.time()
                click.secho(f"\r[{round(server_disconnect_time):04}]: Attempting to reconnect...", bold=True, fg="red", nl=False)
                time.sleep(0.1)



if __name__ == "__main__":
    main()
