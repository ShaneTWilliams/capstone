import math

import yaml
from jinja2 import Environment, PackageLoader


MAX_PACKET_SIZE_B = 8

class ValuesGenerator:
    def __init__(self, values_file):
        self._env = Environment(loader=PackageLoader("capstone.tools"))
        self._env.trim_blocks = True
        self._env.lstrip_blocks = True
        with open(values_file, "r") as f:
            self._values_file = yaml.safe_load(f)

        self.downlink_packets = []
        self.uplink_packets = []

        for value, value_config in self._values_file["values"].items():
            packet_list = None
            if value_config["origin"] == "drone":
                packet_list = self.downlink_packets
            elif value_config["origin"] == "ground":
                packet_list = self.uplink_packets
            else:
                raise ValueError(f"Unknown origin: {value['origin']}")

            if len(packet_list) == 0:
                packet_list.append([value])
            else:
                packet = packet_list[-1]
                size = 0
                for val in packet:
                    size += self._get_value_size(val)
                if size + self._get_value_size(value) > MAX_PACKET_SIZE_B * 8:
                    packet_list.append([value])
                else:
                    packet.append(value)

        for i, packet in enumerate(self.downlink_packets):
            count = 0
            for value in packet:
                size = self._get_value_size(value)
                count += size
            size = sum([self._get_value_size(val) for val in packet])
        for i, packet in enumerate(self.uplink_packets):
            size = sum([self._get_value_size(val) for val in packet])

    @staticmethod
    def _translate_type(type, map):
        if type in map:
            return map[type]
        raise ValueError(f"Unknown type: {type}")

    @staticmethod
    def _get_c_type(value):
        if value["type"]["base"] == "bool":
            return "bool"
        if value["type"]["base"] == "decimal":
            return "float"
        if value["type"]["base"] == "int":
            ret = "uint"
            if value["type"]["min"] < 0:
                ret = "int"
            if value["type"]["max"] - value["type"]["min"] < 2**8:
                return ret + "8_t"
            if value["type"]["max"] - value["type"]["min"] < 2**16:
                return ret + "16_t"
            if value["type"]["max"] - value["type"]["min"] < 2**32:
                return ret + "32_t"
            if value["type"]["max"] - value["type"]["min"] < 2**64:
                return ret + "64_t"
        if value["type"]["base"] == "enum":
            return value["type"]["name"].lower() + "_t"

    @staticmethod
    def _get_proto_type(value):
        if value["type"]["base"] == "bool":
            return "bool"
        if value["type"]["base"] == "decimal":
            return "float"
        if value["type"]["base"] == "int":
            ret = "uint"
            if value["type"]["min"] < 0:
                ret = "sint"
            if value["type"]["max"] - value["type"]["min"] < 2**32:
                return ret + "32"
            if value["type"]["max"] - value["type"]["min"] < 2**64:
                return ret + "64"
        if value["type"]["base"] == "enum":
            return "uint32"

    @staticmethod
    def _get_proto_type_field(value):
        proto_type = ValuesGenerator._get_proto_type(value)
        mapping = {
            "uint32": "u32",
            "sint32": "i32",
            "uint64": "u64",
            "sint64": "i64",
            "float": "f32",
            "double": "f64",
            "bool": "b",
        }
        return mapping[proto_type]

    @staticmethod
    def _get_python_type(value):
        if value["type"]["base"] == "bool":
            return "bool"
        if value["type"]["base"] == "decimal":
            return "float"
        if value["type"]["base"] == "int":
            return "int"
        if value["type"]["base"] == "enum":
            return ValuesGenerator._camel(value["type"]["name"])

    @staticmethod
    def _camel(string):
        return "".join(word.capitalize() for word in string.split("_"))

    def _get_value_size(self, value):
        enums = self._values_file["enums"]
        value_config = self._values_file["values"][value]
        size = 0
        type_config = value_config["type"]
        if type_config["base"] == "enum":
            size += math.ceil(math.log2(len(enums[type_config["name"]])))
        elif type_config["base"] == "bool":
            size += 1
        elif type_config["base"] == "int":
            size += math.ceil(math.log2(type_config["max"] - type_config["min"]))
        elif type_config["base"] == "decimal":
            size += math.ceil(
                math.log2(
                    (type_config["max"] - type_config["min"]) / type_config["lsb"]
                )
            )
        return size

    def _get_value_int_type(self, value):
        size = self._get_value_size(value)
        if size <= 8:
            return "uint8_t"
        if size <= 16:
            return "uint16_t"
        if size <= 32:
            return "uint32_t"
        if size <= 64:
            return "uint64_t"
        raise ValueError(f"Value {value} is too large")

    def _get_value_offset_in_packet(self, value, packet_num, packet_type):
        packet_list = self.downlink_packets if packet_type == "downlink" else self.uplink_packets
        offset = 0
        for val in packet_list[packet_num]:
            if val == value:
                return offset
            offset += self._get_value_size(val)
        return offset

    def _get_values_in_byte(self, byte, packet, packet_type):
        packet_list = self.downlink_packets if packet_type == "downlink" else self.uplink_packets
        ret = []
        for value in packet_list[packet]:
            offset = self._get_value_offset_in_packet(value, packet, packet_type)
            size = self._get_value_size(value)
            if offset < (byte + 1) * 8 and offset + size > byte * 8:
                ret.append((value, (byte * 8) + (offset % 8)))
        return ret

    def _get_bytes_for_value(self, value, packet, packet_type):
        ret = []
        offset = self._get_value_offset_in_packet(value, packet, packet_type)
        size = self._get_value_size(value)
        first_byte = offset // 8
        last_byte = (offset + size - 1) // 8
        return list(range(first_byte, last_byte + 1))

    def _get_packet_size(self, packet_num, packet_type):
        packet_list = self.downlink_packets if packet_type == "downlink" else self.uplink_packets
        return sum(self._get_value_size(val) for val in packet_list[packet_num])

    def _get_values_in_byte(self, byte, packet_num, packet_type):
        packet_list = self.downlink_packets if packet_type == "downlink" else self.uplink_packets
        ret = []
        for value in packet_list[packet_num]:
            offset = self._get_value_offset_in_packet(value, packet_num, packet_type)
            size = self._get_value_size(value)
            first_byte = offset // 8
            relative_byte = byte - first_byte
            if offset < (byte + 1) * 8 and offset + size > byte * 8:
                ret.append((value, offset % 8 - (relative_byte * 8)))
        return ret

    def generate_c(self):
        template = self._env.get_template("values.c.j2")
        return template.render(
            values=self._values_file["values"],
            enums=self._values_file["enums"],
            get_packet_size=self._get_packet_size,
            get_value_size=self._get_value_size,
            get_value_offset=self._get_value_offset_in_packet,
            get_values_in_byte=self._get_values_in_byte,
            get_value_int_type=self._get_value_int_type,
            ceil=math.ceil,
            downlink_packets=self.downlink_packets,
            uplink_packets=self.uplink_packets,
            get_value_offset_in_packet=self._get_value_offset_in_packet,
            get_bytes_for_value=self._get_bytes_for_value,
        )

    def generate_h(self):
        template = self._env.get_template("values.h.j2")
        return template.render(
            values=self._values_file["values"],
            enums=self._values_file["enums"],
            downlink_packets=self.downlink_packets,
            uplink_packets=self.uplink_packets,
            get_c_type=self._get_c_type,
            get_packet_size=self._get_packet_size,
        )

    def generate_proto(self):
        template = self._env.get_template("values.proto.j2")
        return template.render(
            values=self._values_file["values"],
            get_proto_type=self._get_proto_type,
        )

    def generate_python(self):
        template = self._env.get_template("values.py.j2")
        return template.render(
            values=self._values_file["values"],
            enums=self._values_file["enums"],
            get_proto_type=self._get_proto_type,
            get_python_type=self._get_python_type,
            get_proto_type_field=self._get_proto_type_field,
            camel=self._camel,
            downlink_packets=self.downlink_packets,
            uplink_packets=self.uplink_packets,
            get_bytes_for_value=self._get_bytes_for_value,
            get_value_offset_in_packet=self._get_value_offset_in_packet,
            get_value_size=self._get_value_size,
            get_packet_size=self._get_packet_size,
            get_values_in_byte=self._get_values_in_byte,
            ceil=math.ceil,
        )
