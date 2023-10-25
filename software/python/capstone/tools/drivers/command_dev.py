from jinja2 import Environment, PackageLoader


class CommandDriverGenerator:
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
                return "double"
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
        else:
            return "uint8_t*"

    def generate_header(self):
        template = self._env.get_template("cmd_dev.h.j2")
        return template.render(cfg=self._yaml, get_type=self._type_from_field)

    def generate_source(self):
        template = self._env.get_template("cmd_dev.c.j2")
        return template.render(cfg=self._yaml, get_type=self._type_from_field)
