from enum import IntEnum
import time

import RPi.GPIO as gpio_lib
from spidev import SpiDev

from capstone.drivers.sx1262 import *

class CapstonePin(IntEnum):
    # SX1262
    DIGITAL_RADIO_DIO1 = 4
    DIGITAL_RADIO_DIO3 = 5
    DIGITAL_RADIO_BUSY = 6
    DIGITAL_RADIO_RESET = 7
    DIGITAL_RADIO_nCS = 8
    DIGITAL_RADIO_MISO = 9
    DIGITAL_RADIO_MOSI = 10
    DIGITAL_RADIO_SCLK = 11

    # GPS-01
    GPS_UART_RX = 14
    GPS_UART_TX = 15
    GPS_RESET = 16

    # ADV7282A-M
    DIGITIZER_INTRQ = 13
    DIGITIZER_PWRDWN = 22
    DIGITIZER_RESET = 23

    # RX5808
    VIDEO_CH1 = 25
    VIDEO_CH2 = 26
    VIDEO_CH3 = 27

_GPIO_CONFIGS = {
    CapstonePin.DIGITAL_RADIO_DIO1: {
        "mode": gpio_lib.IN
    },
    CapstonePin.DIGITAL_RADIO_DIO3: {
        "mode": gpio_lib.IN
    },
    CapstonePin.DIGITAL_RADIO_BUSY: {
        "mode": gpio_lib.IN
    },
    CapstonePin.DIGITAL_RADIO_RESET: {
        "mode": gpio_lib.OUT,
        "default": gpio_lib.HIGH
    },

    CapstonePin.GPS_RESET: {
        "mode": gpio_lib.OUT,
        "default": gpio_lib.HIGH
    },

    CapstonePin.DIGITIZER_INTRQ: {
        "mode": gpio_lib.IN
    },
    CapstonePin.DIGITIZER_PWRDWN: {
        "mode": gpio_lib.IN
    },
    CapstonePin.DIGITIZER_RESET: {
        "mode": gpio_lib.IN
    },

    CapstonePin.VIDEO_CH1: {
        "mode": gpio_lib.OUT,
        "default": gpio_lib.LOW
    },
    CapstonePin.VIDEO_CH2: {
        "mode": gpio_lib.OUT,
        "default": gpio_lib.LOW
    },
    CapstonePin.VIDEO_CH3: {
        "mode": gpio_lib.OUT,
        "default": gpio_lib.LOW
    },
}

gpio_lib.setmode(gpio_lib.BCM)
gpio_lib.setwarnings(False)

for io, cfg in _GPIO_CONFIGS.items():
    gpio_lib.setup(io, cfg["mode"])
    if cfg["mode"] == gpio_lib.OUT:
        gpio_lib.output(io, cfg["default"])


class CapstoneGPIO:
    def __init__(self, pin):
        self.pin = pin

    def set(self, state):
        if _GPIO_CONFIGS[self.pin]["mode"] != gpio_lib.OUT:
            raise RuntimeException("Can't set input pin state")
        gpio_lib.output(self.pin, state)

    def get(self):
        return bool(gpio_lib.input(self.pin))


class CapstoneSPI:
    SPI_DEVICE = 0
    SPI_BUS = 0
    SPI_SPEED_HZ = 1_000_000
    SPI_MODE = 0b00

    def __init__(self):
        self.bus = SpiDev()
        self.bus.open(self.SPI_BUS, self.SPI_DEVICE)
        self.bus.max_speed_hz = self.SPI_SPEED_HZ
        self.bus.mode = self.SPI_MODE

    def __del__(self):
        self.bus.close()

    def xfer(self, data):
        return self.bus.xfer(data)

GPS_UART_DEV = "/dev/ttyAMA0"
GPS_UART_BAUD = 9600
