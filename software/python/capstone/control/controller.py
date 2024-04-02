import ctypes
import time

import click
import grpc
from capstone.constants import GRPC_SERVER_PORT
from capstone.proto import ground_pb2, ground_pb2_grpc
from google.protobuf.empty_pb2 import Empty
from sdl2 import (
    SDL_CONTROLLERDEVICEADDED,
    SDL_CONTROLLERDEVICEREMOVED,
    SDL_INIT_GAMECONTROLLER,
    SDL_INIT_JOYSTICK,
    SDL_JOYAXISMOTION,
    SDL_JOYBUTTONDOWN,
    SDL_JOYBUTTONUP,
    SDL_Event,
    SDL_GameControllerClose,
    SDL_GameControllerName,
    SDL_GameControllerOpen,
    SDL_GameControllerPath,
    SDL_GameControllerRumble,
    SDL_GameControllerUpdate,
    SDL_Init,
    SDL_JoystickGetDeviceGUID,
    SDL_JoystickUpdate,
    SDL_PollEvent,
    SDL_Quit,
)

SDL_REINIT_PERIOD = 1
CONTROLLER_CONN_TIMEOUT = 3


class XboxOneController:
    BUTTON_MAPPING = {
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

    AXIS_MAPPING = {
        0: "LX",
        1: "LY",
        2: "RX",
        3: "RY",
        4: "LT",
        5: "RT",
    }

    TOGGLE_BUTTONS = "MENU"

    def __init__(self):
        SDL_Init(SDL_INIT_GAMECONTROLLER)
        SDL_Init(SDL_INIT_JOYSTICK)

        self.state = {button: False for button in self.BUTTON_MAPPING.values()} | {
            axis: 0 for axis in self.AXIS_MAPPING.values()
        }
        self.controller = None
        self.guid = ""
        self.connection_time = time.time() - CONTROLLER_CONN_TIMEOUT

    @staticmethod
    def normalize_trig(value):
        return (value + (1 << 15)) / (1 << 16)

    @staticmethod
    def normalize_stick(value):
        return value / (1 << 15)

    def update(self):
        SDL_GameControllerUpdate()
        SDL_JoystickUpdate()

        event = SDL_Event()
        while SDL_PollEvent(ctypes.byref(event)) != 0:
            if event.type == SDL_CONTROLLERDEVICEADDED:
                self.controller = SDL_GameControllerOpen(event.jdevice.which)
                guid = SDL_JoystickGetDeviceGUID(event.jdevice.which)
                guid_str = "".join([f"{d:02X}" for d in guid.data[:]])

                if (
                    guid_str == self.guid
                    and time.time() - self.connection_time < CONTROLLER_CONN_TIMEOUT
                ):
                    self.connection_time = time.time()
                    continue

                # Reconnect!
                self.connection_time = time.time()
                self.guid = guid_str
                SDL_GameControllerRumble(self.controller, 0x0000, 0xFFFF, 500)
                name = SDL_GameControllerName(self.controller).decode("utf-8")
                path = SDL_GameControllerPath(self.controller).decode("utf-8")
                click.secho(
                    f"Initialized '{name}' @ '{path}'",
                )

            elif event.type == SDL_CONTROLLERDEVICEREMOVED:
                SDL_GameControllerClose(self.controller)
                click.secho("Controller Removed", bold=True, fg="red")
                self.controller = None

            elif event.type == SDL_JOYAXISMOTION:
                label = self.AXIS_MAPPING[event.jaxis.axis]
                if label in ("LT", "RT"):
                    self.state[label] = self.normalize_trig(event.jaxis.value)
                elif label.endswith("Y"):
                    self.state[label] = -self.normalize_stick(event.jaxis.value)
                else:
                    self.state[label] = self.normalize_stick(event.jaxis.value)
            elif event.type == SDL_JOYBUTTONDOWN:
                label = self.BUTTON_MAPPING[event.jbutton.button]
                if label in self.TOGGLE_BUTTONS:
                    self.state[label] = not self.state[label]
            elif event.type == SDL_JOYBUTTONUP:
                label = self.BUTTON_MAPPING[event.jbutton.button]
                if label not in self.TOGGLE_BUTTONS:
                    self.state[self.BUTTON_MAPPING[event.jbutton.button]] = False

    def thread_func(self):
        while True:
            self.update()

    @property
    def present(self):
        return self.controller is not None


def main():
    controller = XboxOneController()
    channel = grpc.insecure_channel(f"localhost:{GRPC_SERVER_PORT}")
    stub = ground_pb2_grpc.GroundStationStub(channel)
    request = ground_pb2.SetControllerState()
    server_disconnect_time = time.time()

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

        time.sleep(0.01)


if __name__ == "__main__":
    main()
