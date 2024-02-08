import math

import yaml
from jinja2 import Environment, PackageLoader


class ValuesGenerator:
    def __init__(self, values_file, control_file, telem_file):
        self._env = Environment(loader=PackageLoader("capstone.tools"))
        self._env.trim_blocks = True
        self._env.lstrip_blocks = True
        with open(values_file, "r") as f:
            self._values_file = yaml.safe_load(f)
        with open(control_file, "r") as f:
            self._control_file = yaml.safe_load(f)
        with open(telem_file, "r") as f:
            self._telem_file = yaml.safe_load(f)
        self._process_telem()

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
            return "value_" + value["type"]["name"].lower() + "_t"

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

    def _get_value_offset_in_packet(self, value, packet):
        values = (
            self._control_file[packet]
            if packet in self._control_file
            else self._telem_file[packet]
        )
        offset = 0
        for val in values:
            if val == value:
                return offset
            offset += self._get_value_size(val)
        return offset

    def _get_values_in_byte(self, byte, packet):
        values = (
            self._control_file[packet]
            if packet in self._control_file
            else self._telem_file[packet]
        )
        ret = []
        for value in values:
            offset = self._get_value_offset_in_packet(value, packet)
            size = self._get_value_size(value)
            if offset < (byte + 1) * 8 and offset + size > byte * 8:
                ret.append((value, (byte * 8) + (offset % 8)))
        return ret

    def _get_packet_size(self, packet):
        values = (
            self._control_file[packet]
            if packet in self._control_file
            else self._telem_file[packet]
        )
        return sum(self._get_value_size(val) for val in values)

    def _get_values_in_byte(self, byte, packet):
        values = (
            self._control_file[packet]
            if packet in self._control_file
            else self._telem_file[packet]
        )
        ret = []
        for value in values:
            offset = self._get_value_offset_in_packet(value, packet)
            size = self._get_value_size(value)
            first_byte = offset // 8
            relative_byte = byte - first_byte
            if offset < (byte + 1) * 8 and offset + size > byte * 8:
                ret.append((value, offset % 8 - (relative_byte * 8)))
        return ret

    def _process_telem(self):
        enums = self._values_file["enums"]
        for _, values in self._telem_file.items():
            size = 0
            alt_size = 0
            for value in values:
                value_config = self._values_file["values"][value]
                type_config = value_config["type"]
                if type_config["base"] == "enum":
                    size += math.ceil(math.log2(len(enums[type_config["name"]])))
                    alt_size += 8
                elif type_config["base"] == "bool":
                    size += 1
                    alt_size += 8
                elif type_config["base"] == "int":
                    size += math.ceil(
                        math.log2(type_config["max"] - type_config["min"])
                    )
                    if type_config["min"] < 0:
                        size += 1
                    if type_config["max"] - type_config["min"] < 2**8:
                        alt_size += 8
                    elif type_config["max"] - type_config["min"] < 2**16:
                        alt_size += 16
                    elif type_config["max"] - type_config["min"] < 2**32:
                        alt_size += 32
                    elif type_config["max"] - type_config["min"] < 2**64:
                        alt_size += 64
                elif type_config["base"] == "decimal":
                    size += math.ceil(
                        math.log2(
                            (type_config["max"] - type_config["min"])
                            / type_config["lsb"]
                        )
                    )
                    alt_size += 32

    def generate_c(self):
        template = self._env.get_template("values.c.j2")
        return template.render(
            values=self._values_file["values"],
            enums=self._values_file["enums"],
            control=self._control_file,
            telem=self._telem_file,
            get_packet_size=self._get_packet_size,
            get_value_size=self._get_value_size,
            get_value_offset=self._get_value_offset_in_packet,
            get_values_in_byte=self._get_values_in_byte,
            get_value_int_type=self._get_value_int_type,
            ceil=math.ceil,
        )

    def generate_h(self):
        template = self._env.get_template("values.h.j2")
        return template.render(
            values=self._values_file["values"],
            enums=self._values_file["enums"],
            control=self._control_file,
            telem=self._telem_file,
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
        )
