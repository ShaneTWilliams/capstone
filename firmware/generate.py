import json
from os import listdir
from os.path import isfile, join

BLOCK_COMMENT_WIDTH = 40


def type_from_size(size):
    if size == 1:
        return "bool"
    elif size <= 8:
        return "uint8_t"
    elif size <= 16:
        return "uint16_t"
    elif size <= 32:
        return "uint32_t"
    elif size <= 64:
        return "uint64_t"
    else:
        raise Exception("Invalid size")

def make_reserved_field(offset, size):
    return ("RESERVED", {"offset": offset, "size": size, "access": "x"})


def generate_diagram(reg_name, reg_config, header):
    header.write("┌─\n")
    header.write(f"│    {reg_name} @ 0x{reg_config['offset']}\n")

    next_bit = (reg_config["size"] * 8) - 1
    fields_sorted = sorted(reg_config["fields"].items(), key=lambda x: -x[1]["offset"])
    for field_name, field_config in fields_sorted.copy():
        if field_config["offset"] + field_config["size"] <= next_bit:
            fields_sorted.append(
                make_reserved_field(
                    field_config["offset"] + field_config["size"],
                    next_bit + 1 - (field_config["offset"] + field_config["size"]),
                )
            )

        next_bit = field_config["offset"] - 1
    if next_bit >= 0:
        fields_sorted.append(make_reserved_field(0, next_bit + 1))
    fields_sorted = sorted(fields_sorted, key=lambda x: -x[1]["offset"])

    top_border = "│   "
    bottom_border = "│   "
    numbers = "│   "
    field_start_bit = (reg_config["size"] * 8) - 1

    field_centers = []
    for name, field in fields_sorted:
        numbers += "│"
        old_num_len = len(numbers)
        for num in list(range(field_start_bit, field_start_bit - field["size"], -1)):
            if name == "RESERVED":
                numbers += f"{' ' if num == field_start_bit else '░'}░{' ' if num == field_start_bit - field['size'] + 1 else '░'}"
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

    header.write(top_border)
    header.write(numbers)
    header.write(bottom_border)

    for line in range(len(field_centers) + 1):
        header.write("│   ")
        char = 0
        while True:
            if line != 0 and char == field_centers[len(field_centers) - line]:
                header.write("└")
            elif line != 0 and char > field_centers[len(field_centers) - line]:
                header.write("─")
            elif char in field_centers[: len(field_centers) - line]:
                header.write("│")
            else:
                header.write(" ")
            char += 1
            if line != 0 and char > (field_centers[-1] + 3) or line == 0 and char > field_centers[-1]:
                break

        if line > 0:
            i = 0
            for field_name, field_config in fields_sorted[::-1]:
                if field_name != "RESERVED":
                    i += 1
                if i == line:
                    header.write(f"■ {field_name} ({field_config['access'].lower()})")
                    break
        header.write("\n")


def generate_comment_block(text, header, end=True):
    num_boxes_start = 18 - len(text) // 2
    num_boxes_end = num_boxes_start - (0 if len(text) % 2 == 0 else 1)
    header.write(f"/* {'▄' * (40)}\n")
    header.write(f"   {':' * num_boxes_start}  {text}  {':' * num_boxes_end}\n")
    header.write(f"   {'▀' * (40)} {'*/' if end else ''}\n\n")


def generate_driver(path, header_dir):
    driver_name = path.split("/")[-1].split(".")[0]
    with open(path, "r") as file:
        config = json.load(file)

    if config["transport"] == "i2c":
        ops_type = "i2c_operations_t"
        io_status_type = "i2c_status_t"
    else:
        raise Exception("Unknown transport type")
    dev_type = f"{driver_name.lower()}_t"

    header = open(join(header_dir, driver_name) + ".h", "w")
    source = open(join(header_dir, driver_name) + ".c", "w")

    generate_comment_block(f"{driver_name.upper()} Header", header)
    header.write("#pragma once\n\n")
    header.write('#include "driver_defs.h"\n\n')

    generate_comment_block(f"{driver_name.upper()} Source", source)
    source.write(f"#include \"{driver_name.lower()}.h\"\n\n")

    header.write("typedef struct {\n")
    header.write("    uint8_t address;\n")
    header.write(f"    {ops_type} operations;\n")
    header.write(f"}} {driver_name.lower()}_t;\n\n")

    generate_comment_block("REGISTER DIAGRAMS", header, end=False)

    for reg_name, reg_config in config["registers"].items():
        generate_diagram(reg_name, reg_config, header)
        header.write("└─\n")
    header.write("*/\n\n")

    for reg_name, reg_config in config["registers"].items():
        generate_comment_block(f"REGISTER: {reg_name}", header)
        generate_comment_block(f"REGISTER: {reg_name}", source)
        header.write(f"#define {reg_name}_ADDRESS 0x{reg_config['offset']}\n")
        header.write(f"#define {reg_name}_SIZE {reg_config['size']}\n\n")

        for field_name, field_config in reg_config["fields"].items():
            offset_name = f"{driver_name.upper()}_{reg_name}_{field_name}_OFFSET"
            size_name = f"{driver_name.upper()}_{reg_name}_{field_name}_SIZE"
            mask_name = f"{driver_name.upper()}_{reg_name}_{field_name}_MASK"
            get_name = f"{driver_name.upper()}_GET_{reg_name}_{field_name}"
            set_name = f"{driver_name.upper()}_SET_{reg_name}_{field_name}"

            header.write(f"#define {offset_name} {field_config['offset']}\n")
            header.write(f"#define {size_name} {field_config['size']}\n")
            header.write(f"#define {mask_name} (1 << {size_name}) - 1\n")
            header.write(
                f"#define {get_name}(reg) (((reg) >> {offset_name}) & {mask_name})\n"
            )
            header.write(
                f"#define {set_name}(reg, value) (reg) = ((reg) & ~({mask_name} << {offset_name})) | ((value & {mask_name}) << {offset_name})\n\n"
            )

        read_signature = f"{io_status_type} {driver_name.lower()}_read_{reg_name.lower()}({dev_type} *dev"
        for field_name, field_config in reg_config["fields"].items():
            read_signature += f", {type_from_size(field_config['size'])} *{field_name.lower()}"
        write_signature = f"{io_status_type} {driver_name.lower()}_write_{reg_name.lower()}({dev_type} *dev"
        for field_name, field_config in reg_config["fields"].items():
            write_signature += f", {type_from_size(field_config['size'])} {field_name.lower()}"
        header.write(read_signature + ");\n")
        header.write(write_signature + ");\n\n")

        source.write(read_signature + ") {\n")
        source.write(f"{type_from_size(reg_config['size'] * 8)} value;\n")
        source.write(
            f"{io_status_type} ret = dev->operations.read(dev->address, {reg_name}_ADDRESS, &value, 1);\n"
        )
        source.write("if (ret != I2C_STATUS_OK) {\n")
        source.write("return ret;\n")
        source.write("}\n\n")
        for field_name, field_config in reg_config["fields"].items():
            source.write(
                f"    *{field_name.lower()} = {driver_name.upper()}_GET_{reg_name}_{field_name}(value);\n"
            )
        source.write("return I2C_STATUS_OK;\n")
        source.write("}\n\n")

        source.write(write_signature + ") {\n")
        source.write(f"{type_from_size(reg_config['size'] * 8)} value;\n")

        for field_name, field_config in reg_config["fields"].items():
            source.write(
                f"{driver_name.upper()}_SET_{reg_name}_{field_name}(value, {field_name.lower()});\n"
            )
        source.write(
            f"return dev->operations.write(dev->address, {reg_name}_ADDRESS, value, {reg_config['size']});\n"
        )
        source.write("}\n\n")


input_dir = "drivers"
input_filenames = [
    join(input_dir, f)
    for f in listdir(input_dir)
    if isfile(join(input_dir, f)) and f.endswith(".json")
]

for filename in input_filenames:
    generate_driver(filename, "output")
