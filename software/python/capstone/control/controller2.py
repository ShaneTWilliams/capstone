import time

import click
import grpc
import pygame
from capstone.constants import GRPC_SERVER_PORT
from capstone.proto import ground_pb2, ground_pb2_grpc
from google.protobuf.empty_pb2 import Empty


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
        15: "SHARE",
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
        pygame.init()
        pygame.joystick.init()
        self.joystick = pygame.joystick.Joystick(0)

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
            print(event)
            if event.type == pygame.JOYAXISMOTION:
                self.state[self.AXIS_TO_STR[event.axis]] = event.value
            elif event.type == pygame.JOYBUTTONDOWN:
                button = self.BUTTON_TO_STR[event.button]
                if button == "MENU":
                    self.state[button] = not self.state[button]
                    pygame.joystick.quit()
                    pygame.joystick.init()
                    joystick = pygame.joystick.Joystick(0)
                    joystick.init()
                    joystick.rumble(1, 1, 1000)
                    print("RUMBLE")
                else:
                    self.state[button] = True
            elif event.type == pygame.JOYBUTTONUP:
                button = self.BUTTON_TO_STR[event.button]
                if button == "MENU":
                    continue
                else:
                    self.state[button] = False


def main():
    controller = XboxOneController()
    server_disconnect_time = time.time()

    while True:
        channel = grpc.insecure_channel(f"localhost:{GRPC_SERVER_PORT}")
        stub = ground_pb2_grpc.GroundStationStub(channel)
        request = ground_pb2.SetControllerState()

        while True:
            controller.update()

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
                    click.secho("\nConnected to server", bold=True, fg="green")
                    server_disconnect_time = None
            except grpc.RpcError as e:
                if server_disconnect_time is None:
                    server_disconnect_time = time.time()
                    click.secho("Lost connection to server", bold=True, fg="red")
                    continue
                elapsed = time.time() - server_disconnect_time
                click.secho(
                    f"\r[{round(elapsed):04}]: Attempting to reconnect...",
                    bold=True,
                    fg="red",
                    nl=False,
                )
                time.sleep(0.1)

            time.sleep(0.001)


if __name__ == "__main__":
    main()
