from jinja2 import Environment, PackageLoader


class RegisterDriverGenerator:
    def __init__(self, yaml):
        self._yaml = yaml
        self._device = yaml["name"]
        self._env = Environment(loader=PackageLoader("capstone.tools"))
        self._env.trim_blocks = True
        self._env.lstrip_blocks = True
        self._env.filters["read"] = self._read_filter
        self._env.filters["write"] = self._write_filter

    @staticmethod
    def _read_filter(fields):
        return [
            (field, config)
            for field, config in fields.items()
            if "r" in config["access"]
        ]

    @staticmethod
    def _write_filter(fields):
        return [
            (field, config)
            for field, config in fields.items()
            if "w" in config["access"]
        ]

    def _get_endian_func(self, reg):
        if reg["size"] == 1:
            return ""
        if self._yaml["endianness"] == "big":
            prefix = "be"
        else:
            prefix = "le"
        return f"{prefix}{reg['size'] * 8}toh"

    def _set_endian_func(self, reg):
        if reg["size"] == 1:
            return ""
        if self._yaml["endianness"] == "big":
            suffix = "be"
        else:
            suffix = "le"
        return f"hto{suffix}{reg['size'] * 8}"

    def _type_from_field(self, field_config):
        if "type" in field_config:
            if field_config["type"] in {
                "bool",
                "uint8_t",
                "uint16_t",
                "uint32_t",
                "uint64_t",
            }:
                return field_config["type"]
            if field_config["type"] in self._yaml["enums"]:
                return f"{self._device}_{field_config['type'].lower()}_t"
            if (
                "formats" in self._yaml
                and field_config["type"] in self._yaml["formats"]
            ):
                return "float"
            raise TypeError(f"Invalid type: {field_config['type']}")
        return self._type_from_size(field_config["size"])

    def _is_formatted_field(self, field_config):
        return (
            "type" in field_config
            and "formats" in self._yaml
            and self._yaml["formats"] != None
            and field_config["type"] in self._yaml["formats"]
        )

    @staticmethod
    def _type_from_size(size):
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
    def _make_reserved_field(offset, size):
        return ("RESERVED", {"offset": offset, "size": size, "access": "x"})

    @staticmethod
    def _generate_diagram_labels(field_centers, fields_sorted):
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

    def _process_diagram_fields(self, reg_config):
        next_bit = (reg_config["size"] * 8) - 1
        fields_sorted = sorted(
            reg_config["fields"].items(), key=lambda x: x[1]["offset"], reverse=True
        )
        fields_with_reserved = []
        for field_name, field_config in fields_sorted:
            if field_config["offset"] + field_config["size"] <= next_bit:
                fields_with_reserved.append(
                    self._make_reserved_field(
                        field_config["offset"] + field_config["size"],
                        next_bit + 1 - (field_config["offset"] + field_config["size"]),
                    )
                )

            fields_with_reserved.append((field_name, field_config))

            next_bit = field_config["offset"] - 1
        if next_bit >= 0:
            fields_with_reserved.append(self._make_reserved_field(0, next_bit + 1))
        return fields_with_reserved

    def _generate_diagram(self, reg_name, reg_config):
        header = "┌─\n"
        header += f"│    {reg_name} @ 0x{reg_config['address']:02X}\n"

        top_border = "│   "
        bottom_border = "│   "
        numbers = "│   "
        field_start_bit = (reg_config["size"] * 8) - 1

        field_centers = []
        fields = self._process_diagram_fields(reg_config)
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
            + self._generate_diagram_labels(field_centers, fields)
        )

        return header

    def _generate_diagrams(self):
        diagrams = ""

        for reg_name, reg_config in self._yaml["registers"].items():
            diagrams += self._generate_diagram(reg_name, reg_config)
            diagrams += "└─\n"
        return diagrams

    def generate_header(self):
        template = self._env.get_template("reg_dev.h.j2")
        return template.render(
            cfg=self._yaml,
            diagrams=self._generate_diagrams(),
            get_type=self._type_from_field,
            get_int_type=self._type_from_size,
            get_endian_func=self._get_endian_func,
            set_endian_func=self._set_endian_func,
        )

    def generate_source(self):
        template = self._env.get_template("reg_dev.c.j2")
        return template.render(
            cfg=self._yaml,
            diagrams=self._generate_diagrams(),
            get_type=self._type_from_field,
        )
