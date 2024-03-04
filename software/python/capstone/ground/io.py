import RPi.GPIO as gpio_lib
from spidev import SpiDev
from enum import IntEnum

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
    
__GPIO_CONFIGS = {
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


class CapstoneGPIO:
    def __init__(self, pin):
        self.pin = pin

    def set(self, state):
        if self.__GPIO_CONFIGS[pin]["mode"] != gpio_lib.OUT:
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


class SX1262:
    def __init__(self, busy, spi):
        self.spi = spi
        self.busy = busy
    
    def xfer(self, tx):
        rx = self.spi.xfer(tx)
        while self.busy.get():
            pass
        return rx

XTAL_FREQ = 32_000_000
RF_FREQUENCY = 915_000_000
RF_FREQUENCY_REG = int((RF_FREQUENCY) * (1 << 25) / XTAL_FREQ)

if __name__ == "__main__":
    gpio_lib.setmode(gpio_lib.BCM)
    gpio_lib.setwarnings(False)

    for io, cfg in self.__GPIO_CONFIGS.items():
        gpio_lib.setup(io, cfg["mode"])
        if cfg["mode"] == gpio_lib.OUT:
            gpio_lib.output(io, cfg["default"])

    busy = CapstoneGPIO(CapstonePin.DIGITAL_RADIO_BUSY)
    spi = CapstoneSPI()
    radio = SX1262(busy, spi)

    radio.xfer(sx1262_pack_set_packet_type(Sx1262PacketType.SX1262_LORA))
    radio.xfer(sx1262_pack_set_rf_frequency(RF_FREQUENCY_REG))
    radio.xfer(sx1262_pack_set_pa_config(Sx1262PaConfig.SX1262_SX1262_22_DBM))
    radio.xfer(sx1262_pack_set_tx_params(22, Sx1262RampTimeUs.SX1262_80))
    radio.xfer(sx1262_pack_set_buffer_base_addr(0, 128))
    radio.xfer(sx1262_pack_set_lora_modulation_params(Sx1262Sf.SX1262_SF5, Sx1262LoraBw.SX1262_500_KHZ, Sx1262Cr.SX1262_45, False))
    radio.xfer(sx1262_pack_set_lora_packet_params(4, Sx1262LoraHeaderType.SX1262_IMPLICIT, 9, False, False))
    radio.xfer(sx1262_pack_set_dio2_as_rf_switch_ctrl(True))
    radio.xfer(sx1262_set_tx(0))

    while True:
        print(radio.xfer(sx1262_get_status()))
