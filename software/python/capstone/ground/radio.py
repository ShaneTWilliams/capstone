import time

from serial.tools import list_ports
import serial

from capstone.drivers.sx1262 import *

class UARTRadio:
    def __init__(self, baudrate=115200):
        ports = list_ports.comports()
        self.devs = []
        for port in ports:
            if port.serial_number and port.serial_number == "E6625C4893745B30":
                self.devs.append(serial.Serial(port.device, baudrate=baudrate))
        if len(self.devs) == 0:
            raise Exception("USB -> UART not found")
        elif len(self.devs) == 1:
            raise Exception("Only one USB -> UART found")

    def send(self, data):
        for dev in self.devs:
            dev.write(bytes(data, "ascii"))

    def recv(self):
        for dev in self.devs:
            if dev.in_waiting > 0:
                return dev.read()
        return None

class SX1262Radio:
    __XTAL_FREQ = 32_000_000
    __RF_FREQUENCY = 915_000_000
    __RF_FREQUENCY_REG = int((__RF_FREQUENCY) * (1 << 25) / __XTAL_FREQ)
    __PACKET_SIZE = 9

    def __init__(self, spi, busy_pin, reset_pin):
        self.spi = spi
        self.busy_pin = busy_pin
        self.reset_pin = reset_pin

    def reset(self):
        self.reset_pin.set(False)
        time.sleep(0.01)
        self.reset_pin.set(True)
        time.sleep(0.01)

    def xfer(self, tx):
        while self.busy_pin.get():
            pass
        rx = self.spi.xfer(tx)
        return rx

    def init(self):
        self.xfer(sx1262_pack_set_packet_type(Sx1262PacketType.SX1262_LORA))
        self.xfer(sx1262_pack_set_rf_frequency(self.__RF_FREQUENCY_REG))
        self.xfer(sx1262_pack_set_pa_config(Sx1262PaConfig.SX1262_SX1262_14_DBM))
        self.xfer(sx1262_pack_set_tx_params(14, Sx1262RampTimeUs.SX1262_80))
        self.xfer(sx1262_pack_set_buffer_base_addr(0, 128))
        self.xfer(sx1262_pack_set_lora_modulation_params(Sx1262Sf.SX1262_SF7, Sx1262LoraBw.SX1262_500_KHZ, Sx1262Cr.SX1262_47, False))
        self.xfer(sx1262_pack_set_lora_packet_params(4, Sx1262LoraHeaderType.SX1262_EXPLICIT, self.__PACKET_SIZE, True, False))
        self.xfer(sx1262_pack_set_dio2_as_rf_switch_ctrl(True))
        self.xfer([0x08, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])

    def transmit(self, data):
        buffer = [SX1262_WRITE_BUFFER_OPCODE, 0] + data
        self.xfer(buffer)

        self.xfer(sx1262_pack_set_tx(0))

    def set_receive(self):
        self.xfer(sx1262_pack_set_rx(0))

    def get_receive_data(self):
        data = self.xfer(sx1262_pack_get_irq_status())
        irqs = sx1262_unpack_get_irq_status(data)
        if irqs["header_error"] or irqs["crc_error"]:
            self.xfer(sx1262_pack_clear_irq_status(
                False, False, False, False, False, True, True, False, False, False
            ))
            return []
            
        return self.xfer([SX1262_READ_BUFFER_OPCODE, 128] + [0] * (self.__PACKET_SIZE + 3))[3:]

    def get_rssi(self):
        pass

    def get_status(self):
        data = self.xfer(sx1262_pack_get_status())
        return sx1262_unpack_get_status(data)

    def tx_done(self):
        data = self.spi.xfer(sx1262_pack_get_status())
        status = sx1262_unpack_get_status(data)
        in_rc = (status["status_chip_mode"] == Sx1262StatusChipMode.SX1262_STDBY_RC)
        tx_done = (status["status_cmd"] == Sx1262StatusCmd.SX1262_TX_DONE)
        return in_rc and tx_done

    def rx_done(self):
        data = self.spi.xfer(sx1262_pack_get_status())
        status = sx1262_unpack_get_status(data)
        in_rc = (status["status_chip_mode"] == Sx1262StatusChipMode.SX1262_STDBY_RC)
        data_avail = (status["status_cmd"] == Sx1262StatusCmd.SX1262_DATA_AVAIL)
        return in_rc and data_avail
