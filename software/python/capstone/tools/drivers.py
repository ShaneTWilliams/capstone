import os

import yaml


class DriverGenerator:
    def __init__(self, yaml_path):
        self.__driver_name = os.path.basename(yaml_path).removesuffix(".yaml").lower()
        self.__dev_type = f"{self.__driver_name.lower()}_t"

        with open(yaml_path, "r", encoding="ascii") as file:
            self.__config = yaml.safe_load(file)

        if self.__config["transport"] == "i2c":
            self.__ops_type = "i2c_operations_t"
            self.__io_status_type = "i2c_status_t"
        else:
            raise ValueError("Unknown transport type")

    def __type_from_field(self, field_config):
        if "type" in field_config:
            if field_config["type"] in {
                "bool",
                "uint8_t",
                "uint16_t",
                "uint32_t",
                "uint64_t",
            }:
                return field_config["type"]
            if field_config["type"] in self.__config["enums"]:
                return f"{self.__driver_name}_{field_config['type'].lower()}_t"
            raise TypeError(f"Invalid type: {field_config['type']}")
        return self.__type_from_size(field_config["size"])

    @staticmethod
    def __type_from_size(size):
        if size == 1:
            return "bool"
        if size <= 8:
            return "uint8_t"
        if size <= 16:
            return "uint16_t"
        if size <= 32:
            return "uint32_t"
        if size <= 64:
            return "uint64_t"
        raise ValueError("Invalid size")

    @staticmethod
    def __make_reserved_field(offset, size):
        return ("RESERVED", {"offset": offset, "size": size, "access": "x"})

    @staticmethod
    def __generate_diagram_labels(field_centers, fields_sorted):
        labels = ""
        for line in range(len(field_centers) + 1):
            labels += "│   "
            char = 0
            while True:
                if line != 0 and char == field_centers[len(field_centers) - line]:
                    labels += "└"
                elif line != 0 and char > field_centers[len(field_centers) - line]:
                    labels += "─"
                elif char in field_centers[: len(field_centers) - line]:
                    labels += "│"
                else:
                    labels += " "
                char += 1
                if (
                    line != 0
                    and char > (field_centers[-1] + 3)
                    or line == 0
                    and char > field_centers[-1]
                ):
                    break

            if line > 0:
                i = 0
                for field_name, field_config in fields_sorted[::-1]:
                    if field_name != "RESERVED":
                        i += 1
                    if i == line:
                        labels += f"■ {field_name} ({field_config['access'].lower()})"
                        break
            labels += "\n"
        return labels

    def __process_diagram_fields(self, reg_config):
        next_bit = (reg_config["size"] * 8) - 1
        fields_sorted = sorted(
            reg_config["fields"].items(), key=lambda x: x[1]["offset"], reverse=True
        )
        fields_with_reserved = []
        for field_name, field_config in fields_sorted:
            if field_config["offset"] + field_config["size"] <= next_bit:
                fields_with_reserved.append(
                    self.__make_reserved_field(
                        field_config["offset"] + field_config["size"],
                        next_bit + 1 - (field_config["offset"] + field_config["size"]),
                    )
                )
            else:
                fields_with_reserved.append((field_name, field_config))

            next_bit = field_config["offset"] - 1
        if next_bit >= 0:
            fields_with_reserved.append(self.__make_reserved_field(0, next_bit + 1))
        return fields_with_reserved

    def __generate_diagram(self, reg_name, reg_config):
        header = "┌─\n"
        header += f"│    {reg_name} @ 0x{reg_config['address']}\n"

        top_border = "│   "
        bottom_border = "│   "
        numbers = "│   "
        field_start_bit = (reg_config["size"] * 8) - 1

        field_centers = []
        fields = self.__process_diagram_fields(reg_config)
        for name, field in fields:
            numbers += "│"
            old_num_len = len(numbers)
            for num in list(
                range(field_start_bit, field_start_bit - field["size"], -1)
            ):
                if name == "RESERVED":
                    numbers += f"{' ' if num == field_start_bit else '░'}░"
                    numbers += (
                        f"{' ' if num == field_start_bit - field['size'] + 1 else '░'}"
                    )
                else:
                    numbers += f" {num} "
            num_expand_len = len(numbers) - old_num_len
            if name != "RESERVED":
                field_centers.append(old_num_len + (num_expand_len // 2) - 4)
            if field["offset"] + field["size"] == reg_config["size"] * 8:
                top_border += f"┌{'─' * (num_expand_len)}"
                bottom_border += f"└{'─' * (num_expand_len)}"
            else:
                top_border += f"┬{'─' * (num_expand_len)}"
                bottom_border += f"┴{'─' * (num_expand_len)}"
            field_start_bit -= field["size"]
        top_border += "┐\n"
        bottom_border += "┘\n"
        numbers += "│\n"

        header += (
            top_border
            + numbers
            + bottom_border
            + self.__generate_diagram_labels(field_centers, fields)
        )

        return header

    def __generate_comment_block(self, text, end=True):
        num_boxes_start = 18 - len(text) // 2
        num_boxes_end = num_boxes_start - (0 if len(text) % 2 == 0 else 1)
        return (
            f"/* {'▄' * (40)}\n"
            + f"   {':' * num_boxes_start}  {text}  {':' * num_boxes_end}\n"
            + f"   {'▀' * (40)}{' */' if end else ''}\n\n"
        )

    def __generate_read_reg_signature(self, reg_name, reg_config):
        code = f"{self.__io_status_type} "
        code += f"{self.__driver_name.lower()}_read_{reg_name.lower()}({self.__dev_type} *dev"
        for field_name, field_config in reg_config["fields"].items():
            code += f", {self.__type_from_field(field_config)}"
            code += f" *{field_name.lower()}"
        return code

    def __generate_write_reg_signature(self, reg_name, reg_config):
        code = f"{self.__io_status_type} "
        code += f"{self.__driver_name.lower()}_write_{reg_name.lower()}"
        code += f"({self.__dev_type} *dev"
        for field_name, field_config in reg_config["fields"].items():
            code += f", {self.__type_from_field(field_config)}"
            code += f" {field_name.lower()}"
        return code

    def __generate_read_field_signature(self, reg_name, field_name, field_config):
        field_type = self.__type_from_field(field_config)
        code = f"{self.__io_status_type} "
        code += (
            f"{self.__driver_name.lower()}_read_{reg_name.lower()}_{field_name.lower()}"
        )
        code += f"({self.__dev_type} *dev, {field_type} *{field_name.lower()}"
        return code

    def __generate_write_field_signature(self, reg_name, field_name, field_config):
        field_type = self.__type_from_field(field_config)
        code = f"{self.__io_status_type} "
        code += f"{self.__driver_name.lower()}_write_{reg_name.lower()}_{field_name.lower()}"
        code += f"({self.__dev_type} *dev, {field_type} {field_name.lower()}"
        return code

    def __generate_read_field_function(
        self, reg_name, reg_config, field_config, field_name
    ):
        reg_type = self.__type_from_size(reg_config["size"] * 8)
        code = self.__generate_read_field_signature(reg_name, field_name, field_config)
        code += ") {{\n"
        code += f"{reg_type} {reg_name.lower()};\n"
        code += f"{self.__io_status_type} ret = dev->operations.read(dev->address, "
        code += (
            f"{reg_name}_ADDRESS, &{reg_name.lower()}, sizeof({reg_name.lower()}));\n"
        )
        code += "if (ret != I2C_STATUS_OK) {\n"
        code += "return ret;\n"
        code += "}\n\n"
        code += f"    *{field_name.lower()} = "
        code += f"{self.__driver_name.upper()}_GET_{reg_name}_{field_name}({reg_name.lower()});\n"
        code += "return I2C_STATUS_OK;\n"
        code += "}\n\n"
        return code

    def __generate_write_field_function(
        self, reg_name, reg_config, field_config, field_name
    ):
        reg_type = self.__type_from_size(reg_config["size"] * 8)
        code = self.__generate_write_field_signature(reg_name, field_name, field_config)
        code += ") {{\n"
        code += f"{reg_type} {reg_name.lower()};\n"
        code += f"{self.__io_status_type} ret = dev->operations.read(dev->address, "
        code += (
            f"{reg_name}_ADDRESS, &{reg_name.lower()}, sizeof({reg_name.lower()}));\n"
        )
        code += "if (ret != I2C_STATUS_OK) {\n"
        code += "return ret;\n"
        code += "}\n\n"
        code += f"{self.__driver_name.upper()}_SET_{reg_name}_{field_name}"
        code += f"({reg_name.lower()}, {field_name.lower()});\n"
        code += f"return dev->operations.write(dev->address, {reg_name.lower()});\n"
        code += "}\n\n"
        return code

    def generate_header(self):  # pylint: disable=too-many-locals
        header = self.__generate_comment_block(f"{self.__driver_name.upper()} Header")
        header += "#pragma once\n\n"
        header += '#include "driver_defs__.self, h"\n\n'

        header += (
            "typedef __struct self, {"
            + "uint8_t address;"
            + f"{self.__ops_type} operations;"
            + f"}} {self.__driver_name.lower()}_t;\n\n"
        )

        header += self.__generate_comment_block("REGISTER DIAGRAMS", end=False)

        for reg_name, reg_config in self.__config["registers"].items():
            header += self.__generate_diagram(reg_name, reg_config)
            header += "└─\n"
        header += "*/\n\n"

        header += self.__generate_comment_block("FIELD ENUMS")

        for enum_name, enum_config in self.__config["enums"].items():
            header += "typedef enum {\n"
            for name, value in enum_config.items():
                header += (
                    f"{self.__driver_name.upper()}_{enum_name.upper()}_{name.upper()} "
                )
                header += f"0x{value:02},\n"

            header += f"}} {self.__driver_name.lower()}_{enum_name.lower()}_t;\n\n"

        for reg_name, reg_config in self.__config["registers"].items():
            header += self.__generate_comment_block(f"REGISTER: {reg_name}")
            header += f"#define {reg_name}_ADDRESS 0x{reg_config['address']}\n"
            header += f"#define {reg_name}_SIZE {reg_config['size']}\n\n"

            for field_name, field_config in reg_config["fields"].items():
                offset_name = (
                    f"{self.__driver_name.upper()}_{reg_name}_{field_name}_OFFSET"
                )
                size_name = f"{self.__driver_name.upper()}_{reg_name}_{field_name}_SIZE"
                mask_name = f"{self.__driver_name.upper()}_{reg_name}_{field_name}_MASK"

                header += f"#define {offset_name} {field_config['offset']}\n"
                header += f"#define {size_name} {field_config['size']}\n"
                header += f"#define {mask_name} (1 << {size_name}) - 1\n"
                header += (
                    f"#define {self.__driver_name.upper()}_GET_{reg_name}_{field_name}(reg) "
                    + f"(((reg) >> {offset_name}) & {mask_name})\n"
                )
                header += (
                    f"#define {self.__driver_name.upper()}_SET_{reg_name}_{field_name}"
                )
                header += "(reg, value) (reg) = "
                header += f"((reg) & ~({mask_name} << {offset_name})) | "
                header += f"((value & {mask_name}) << {offset_name})\n\n"

            if any(
                (
                    "r" in field_config["access"]
                    for field_config in reg_config["fields"].values()
                )
            ):
                header += self.__generate_read_reg_signature(reg_name, reg_config)
                header += ");\n"

            if any(
                (
                    "w" in field_config["access"]
                    for field_config in reg_config["fields"].values()
                )
            ):
                header += self.__generate_write_reg_signature(reg_name, reg_config)
                header += ");\n"

            header += "\n"

            for field_name, field_config in reg_config["fields"].items():
                if "r" not in field_config["access"]:
                    continue
                header += self.__generate_read_field_signature(
                    reg_name, field_name, field_config
                )
                header += ");\n"
            header += "\n"
            for field_name, field_config in reg_config["fields"].items():
                if "w" not in field_config["access"]:
                    continue
                header += self.__generate_write_field_signature(
                    reg_name, field_name, field_config
                )
                header += ");\n"
            header += "\n"
        return header

    def generate_source(self):
        source = ""

        self.__generate_comment_block(f"{self.__driver_name.upper()} Source", source)
        source += f'#include "{self.__driver_name.lower()}.h"\n\n'

        for reg_name, reg_config in self.__config["registers"].items():
            source += self.__generate_comment_block(f"REGISTER: {reg_name}")

            # Read function
            if any(
                (
                    "r" in field_config["access"]
                    for field_config in reg_config["fields"].values()
                )
            ):
                source += self.__generate_read_reg_signature(reg_name, reg_config)
                source += ") {\n"

                source += f"{self.__type_from_size(reg_config['size'] * 8)} value;\n"
                source += (
                    f"{self.__io_status_type} ret = dev->operations.read(dev->address, "
                    + f"{reg_name}_ADDRESS, &value, {reg_config['size']});\n"
                )
                source += "if (ret != I2C_STATUS_OK) {\n"
                source += "return ret;\n"
                source += "}\n\n"
                for field_name, field_config in reg_config["fields"].items():
                    source += (
                        f"    *{field_name.lower()} = "
                        + f"{self.__driver_name.upper()}_GET_{reg_name}_{field_name}(value);\n"
                    )
                source += "return I2C_STATUS_OK;\n"
                source += "}\n\n"

            # Write function
            if any(
                (
                    "w" in field_config["access"]
                    for field_config in reg_config["fields"].values()
                )
            ):
                source += self.__generate_write_reg_signature(reg_name, reg_config)
                source += ") {\n"
                source += f"{self.__type_from_size(reg_config['size'] * 8)} value;\n"

                for field_name, field_config in reg_config["fields"].items():
                    source += (
                        f"{self.__driver_name.upper()}_SET_{reg_name}_{field_name}"
                        + f"(value, {field_name.lower()});\n"
                    )
                source += (
                    "return dev->operations.write(dev->address, "
                    + f"{reg_name}_ADDRESS, value, {reg_config['size']});\n"
                )
                source += "}\n\n"

            for field_name, field_config in reg_config["fields"].items():
                if "r" in field_config["access"]:
                    source += self.__generate_read_field_function(
                        reg_name, reg_config, field_config, field_name
                    )
            for field_name, field_config in reg_config["fields"].items():
                if "w" in field_config["access"]:
                    source += self.__generate_write_field_function(
                        reg_name, reg_config, field_config, field_name
                    )
        return source
