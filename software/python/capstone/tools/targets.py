import curses
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

import click
import grpc
import yaml
from capstone.constants import GRPC_SERVER_PORT
from capstone.tools.drivers.command_dev import CommandDriverGenerator
from capstone.tools.drivers.register_dev import RegisterDriverGenerator
from capstone.tools.flash import flash as flash_usb
from capstone.tools.usb import prompt_and_get_board, send_request
from capstone.tools.values import ValuesGenerator
from grpc_tools.protoc import main as grpc_main

#####################################
############### PATHS ###############
#####################################


if platform.system() == "Windows":
    OS_DIR = "windows-x86"
elif platform.system() == "Linux":
    OS_DIR = "linux-x86"
elif platform.system() == "Darwin":
    OS_DIR = "macos-x86"
else:
    raise RuntimeError("Unsupported OS")

# Directories
SOFTWARE_DIR = os.path.relpath(
    os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
    )
)
FIRMWARE_DIR = os.path.join(SOFTWARE_DIR, "firmware")
FIRMWARE_SRC_DIR = os.path.join(FIRMWARE_DIR, "src")
APP_SRC_DIR = os.path.join(FIRMWARE_SRC_DIR, "app")
BUILD_DIR = os.path.join(FIRMWARE_DIR, "build")
TOOLS_DIR = os.path.join(SOFTWARE_DIR, "tools")
PYTHON_DIR = os.path.join(SOFTWARE_DIR, "python")
PYTHON_PACKAGE_DIR = os.path.join(PYTHON_DIR, "capstone")
PROTO_DIR = os.path.join(SOFTWARE_DIR, "proto")
PYTHON_PROTO_DIR = os.path.join(PYTHON_PACKAGE_DIR, "proto")
NANOPB_PROTO_DIR = os.path.join(FIRMWARE_DIR, "lib", "nanopb", "generator", "proto")
LIB_DIR = os.path.join(FIRMWARE_DIR, "lib")
OPENOCD_DIR = os.path.join(LIB_DIR, "openocd")
OPENOCD_TCL_DIR = os.path.join(OPENOCD_DIR, "tcl")
DRIVERS_YAML_DIR = os.path.join(APP_SRC_DIR, "drivers")
CMD_DEV_YAML_DIR = os.path.join(DRIVERS_YAML_DIR, "command")
REG_DEV_YAML_DIR = os.path.join(DRIVERS_YAML_DIR, "register")
GENERATED_DIR = os.path.join(APP_SRC_DIR, "generated")
VALUES_YAML_DIR = os.path.join(SOFTWARE_DIR, "values")
FRONTEND_DIR = os.path.join(SOFTWARE_DIR, "frontend")
FRONTEND_PROTO_DIR = os.path.join(FRONTEND_DIR, "proto")
GRPC_WEB_PROTOC = os.path.join(
    FRONTEND_DIR, "node_modules", ".bin", "grpc_tools_node_protoc"
)

# Files
OPENOCD_CFG = os.path.join(FIRMWARE_DIR, "capstone-ocd.cfg")
PYLINTRC = os.path.join(PYTHON_DIR, "pylintrc")
VALUES_YAML = os.path.join(VALUES_YAML_DIR, "values.yaml")
CONTROL_PACKET_YAML = os.path.join(VALUES_YAML_DIR, "control_pack.yaml")
TELEM_PACKET_YAML = os.path.join(VALUES_YAML_DIR, "telem_pack.yaml")
GROUND_PROTO_FILE = os.path.join(PROTO_DIR, "ground.proto")
VALUES_PROTO_FILE = os.path.join(PROTO_DIR, "values.proto")
ENVOY_YAML = os.path.join(FRONTEND_DIR, "envoy.yaml")

# Tool binaries
OPENOCD = os.path.join(OPENOCD_DIR, "src", "openocd")
ENVOY = "envoy"

# Binaries
APP_ELF = os.path.join(BUILD_DIR, "app.elf")
BOOT_ELF = os.path.join(BUILD_DIR, "boot.elf")
RADIO_ELF = os.path.join(BUILD_DIR, "radio.elf")
APP_BIN = os.path.join(BUILD_DIR, "app.bin")
BOOT_BIN = os.path.join(BUILD_DIR, "boot.bin")
RADIO_BIN = os.path.join(BUILD_DIR, "radio.bin")

# Values file
with open(VALUES_YAML, "r") as f:
    VALUES = yaml.safe_load(f)
VALUE_NAMES = {}
for value_name in VALUES["values"].keys():
    VALUE_NAMES[value_name.upper()] = value_name
    VALUE_NAMES[value_name.lower()] = value_name
    VALUE_NAMES["-".join(value_name.upper().split("_"))] = value_name
    VALUE_NAMES["-".join(value_name.lower().split("_"))] = value_name

#####################################
############## HELPERS ##############
#####################################


def run_cmd(cmd, error, cwd=None):
    click.secho("Running", bold=True, nl=False)
    for text in cmd:
        if " " in text:
            click.secho(f' "{text}"', nl=False, underline=True)
        else:
            click.secho(f" {text}", nl=False, underline=True)
    if cwd is not None:
        click.secho(" in ", bold=True, nl=False)
        click.secho(cwd, underline=True, nl=False)
    click.secho("")

    with subprocess.Popen(cmd, cwd=cwd) as proc:
        if proc.wait() != 0:
            click.secho(error, bold=True, fg="red")
            sys.exit(1)


def remove_dir(path):
    shutil.rmtree(path, ignore_errors=True)
    click.secho(f"Removed {path}")


def build_helper(executable):
    for proto_file in os.listdir(PROTO_DIR):
        proto_helper(os.path.join(PROTO_DIR, proto_file))

    cmake_cmd = [
        "cmake",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DFAMILY=rp2040",
        f"-DTARGET={executable}",
    ]
    if platform.system() == "Windows":
        cmake_cmd.extend(["-G", "Unix Makefiles"])
    cmake_cmd.append("..")
    elf_file = {
        "app": APP_ELF,
        "boot": BOOT_ELF,
        "radio": RADIO_ELF,
    }[executable]

    click.secho(f"Building {elf_file}", bold=True)
    Path(f"{FIRMWARE_DIR}/build").mkdir(exist_ok=True)

    run_cmd(cmake_cmd, f"Failed to configure {elf_file}", cwd=BUILD_DIR)
    run_cmd(["make", "-j4"], f"Failed to build {elf_file}", cwd=BUILD_DIR)
    run_cmd(["arm-none-eabi-size", elf_file], f"Failed to read out {elf_file} size")
    click.secho(f"Successfully built {elf_file}", bold=True, fg="green")


def proto_helper(input_file):
    Path(PYTHON_PROTO_DIR).mkdir(exist_ok=True)
    cmd = [
        "protoc",
        f"--python_out={PYTHON_PROTO_DIR}",
        f"-I={NANOPB_PROTO_DIR}",
        f"-I={PROTO_DIR}",
        input_file,
    ]
    output_file = os.path.join(
        PYTHON_PROTO_DIR, os.path.basename(input_file).replace(".proto", "_pb2.py")
    )
    run_cmd(cmd, f"Failed to generate protos from {input_file}")
    click.secho(f"Successfully generated {output_file}", bold=True, fg="green")


def proto_bool_from_str(value):
    is_false = value.lower() in ["false", "0", "no", "off", "disable", "disabled"]
    is_true = value.lower() in ["true", "1", "yes", "on", "enable", "enabled"]
    if not is_false and not is_true:
        raise ValueError("Invalid boolean value: {}".format(value))
    return is_true


def proto_pack_value(proto_value, tag, value):
    if VALUES["values"][tag.name]["type"]["base"] == "bool":
        is_false = value.lower() in ["false", "0", "no", "off", "disable", "disabled"]
        is_true = value.lower() in ["true", "1", "yes", "on", "enable", "enabled"]
        if not is_false and not is_true:
            raise ValueError("Invalid boolean value: {}".format(value))
        proto_value.b = is_true
    elif VALUES["values"][tag.name]["type"]["base"] == "decimal":
        proto_value.f32 = float(value)
    elif VALUES["values"][tag.name]["type"]["base"] == "int":
        proto_value.i32 = int(value)
    elif VALUES["values"][tag.name]["type"]["base"] == "enum":
        proto_value.u32 = VALUES["values"][tag.name]["type"]["name"].index(value)
    else:
        raise ValueError("Invalid type: {}".format(VALUES["values"][tag.name]["type"]))


def proto_unpack_value(proto_msg, tag):
    type_config = VALUES["values"][tag.name]["type"]
    if type_config["base"] == "bool":
        return proto_msg.b
    if type_config["base"] == "decimal":
        # Round to nearest LSB
        lsb = type_config["lsb"]
        lsb_rounded = round(proto_msg.f32 / lsb) * lsb
        return round(lsb_rounded, 10)  # Prevent ugly floating point errors
    if type_config["base"] == "int":
        if type_config["min"] < 0:
            return proto_msg.i32
        if type_config["base"] == "int":
            return proto_msg.u32
    if type_config["base"] == "enum":
        return VALUES["enums"][type_config["enum"]][proto_msg.u32]
    raise ValueError(f"Invalid type: {type_config}")


def values_helper():
    if not os.path.exists(GENERATED_DIR):
        os.makedirs(GENERATED_DIR)

    c = os.path.join(GENERATED_DIR, "values.c")
    h = os.path.join(GENERATED_DIR, "values.h")
    proto = os.path.join(PROTO_DIR, "values.proto")
    python = os.path.join(PYTHON_PACKAGE_DIR, "values.py")
    output_files = [c, h]
    gen = ValuesGenerator(VALUES_YAML, CONTROL_PACKET_YAML, TELEM_PACKET_YAML)
    with open(c, "w") as f:
        f.write(gen.generate_c())
    with open(h, "w") as f:
        f.write(gen.generate_h())
    with open(proto, "w") as f:
        f.write(gen.generate_proto())
    with open(python, "w") as f:
        f.write(gen.generate_python())

    click.secho("Successfully generated value code", bold=True, fg="green")
    run_cmd(
        ["clang-format", "-i", "-style=file", "-verbose"] + output_files,
        "Failed to format generated drivers",
    )
    run_cmd(["black", python], "Failed to format Python code")
    click.secho("Successfully formatted value code", bold=True, fg="green")


#####################################
########### CLICK TARGETS ###########
#####################################


@click.group()
def cli():
    pass


@click.command()
@click.option(
    "--executable",
    "-e",
    default="app",
    type=click.Choice(["boot", "app", "radio", "all"]),
    help="The executable to build.",
)
def build(executable):
    if executable == "all":
        build_helper("boot")
        build_helper("app")
        build_helper("radio")
    else:
        build_helper(executable)


@click.command()
@click.option(
    "--hard", "-h", default=False, type=bool, help="True to remove installations."
)
def clean(hard):
    remove_dir(BUILD_DIR)
    remove_dir(os.path.join(PYTHON_PACKAGE_DIR, "__pycache__"))
    remove_dir(os.path.join(SOFTWARE_DIR, "build"))
    remove_dir(PYTHON_PROTO_DIR)
    remove_dir(GENERATED_DIR)
    for path in Path(PYTHON_PACKAGE_DIR).rglob("__pycache__"):
        remove_dir(path.absolute())
    if hard:
        remove_dir(os.path.join(PYTHON_DIR, "capstone.egg-info"))
    click.secho("All temporary directories removed", bold=True, fg="green")


@click.command()
def proto():
    if not os.path.exists(FRONTEND_PROTO_DIR):
        os.makedirs(FRONTEND_PROTO_DIR)
    for proto_file in os.listdir(PROTO_DIR):
        proto_helper(os.path.join(PROTO_DIR, proto_file))
    python_server_cmd = [
        "grpc_tools.protoc",
        f"-I{PROTO_DIR}",
        f"-I{NANOPB_PROTO_DIR}",
        "-I/usr/include",
        "-I/usr/local/include",
        f"--python_out={PYTHON_PROTO_DIR}",
        f"--pyi_out={PYTHON_PROTO_DIR}",
        f"--grpc_python_out={PYTHON_PROTO_DIR}",
        GROUND_PROTO_FILE,
    ]

    click.secho("Running " + " ".join(python_server_cmd), bold=True)
    if grpc_main(python_server_cmd) == 0:
        click.secho("Successfully generated GRPC proto code", bold=True, fg="green")
    else:
        click.secho("Failed to generate GRPC proto code", bold=True, fg="red")
        return
    for proto_file in [GROUND_PROTO_FILE, VALUES_PROTO_FILE]:
        js_client_cmd = [
            GRPC_WEB_PROTOC,
            f"-I{PROTO_DIR}",
            proto_file,
            f"--js_out=import_style=commonjs:{FRONTEND_PROTO_DIR}",
            f"--grpc-web_out=import_style=commonjs,mode=grpcwebtext:{FRONTEND_PROTO_DIR}",
        ]
        click.secho("Running " + " ".join(js_client_cmd), bold=True)
        run_cmd(js_client_cmd, "Failed to generate JS client code")
    click.secho("Successfully generated JS proto code", bold=True, fg="green")


@click.command()
@click.option(
    "--executable",
    "-e",
    default="app",
    type=click.Choice(["boot", "app", "radio"]),
    help="The executable to flash.",
)
@click.option(
    "--transport",
    "-t",
    default="swd",
    type=click.Choice(["swd", "usb"]),
    help="The physical transport to flash with.",
)
def flash(executable, transport):
    if executable == "boot" and transport == "usb":
        click.secho("Can't flash bootloader over USB. Try SWD.", bold=True, fg="red")
        return

    if transport == "swd":
        if platform.system() == "Windows":
            click.secho(
                "You can't do that on Windows. Come back with a real computer.",
                bold=True,
                fg="red",
            )
            sys.exit(1)

        if not os.path.isfile(OPENOCD):
            click.secho(
                f"Couldn't find {OPENOCD}. Building now.", bold=True, fg="yellow"
            )
            run_cmd(["./bootstrap"], "Failed to bootstrap OpenOCD", cwd=OPENOCD_DIR)
            run_cmd(
                ["./configure", "--disable-werror", "--enable-picoprobe"],
                "Failed to configure OpenOCD",
                cwd=OPENOCD_DIR,
            )
            run_cmd(["make", "-j4"], "Failed to build OpenOCD", cwd=OPENOCD_DIR)
            click.secho("Successfully built OpenOCD", bold=True, fg="green")

        build_helper(executable)

        elf_file = os.path.join(BUILD_DIR, f"{executable}.elf")
        click.secho(f"Flashing {elf_file}", bold=True)
        run_cmd(
            [
                OPENOCD,
                "-f",
                "interface/cmsis-dap.cfg",
                "-f",
                "target/rp2040.cfg",
                "-f",
                OPENOCD_CFG,
                "-s",
                OPENOCD_TCL_DIR,
                "-c",
                f"program {elf_file} reset exit",
            ],
            f"Failed to flash {elf_file}",
        )
        click.secho(f"Successfully flashed {elf_file}", bold=True, fg="green")
    elif transport == "usb" and executable == "app":
        device = prompt_and_get_board("boot")
        if device is None:
            click.secho("No flashable devices found", bold=True, fg="red")
            return
        build_helper(executable)
        flash_usb(APP_BIN, device)
        click.secho(f"Successfully flashed {APP_BIN}", bold=True, fg="green")


@click.command()
@click.option(
    "--executable",
    "-e",
    default="app",
    type=click.Choice(["boot", "app", "radio"]),
    help="The application to debug.",
    required=True,
)
def debug(executable):
    click.secho("Running OpenOCD", bold=True)
    # OpenOCD logs to STDERR because of course it does.
    with subprocess.Popen(
        [
            OPENOCD,
            "-f",
            "interface/cmsis-dap.cfg",
            "-f",
            "target/rp2040.cfg",
            "-f",
            OPENOCD_CFG,
            "-s",
            OPENOCD_TCL_DIR,
        ],
        stderr=subprocess.PIPE,
        stdin=subprocess.PIPE,
    ) as openocd:
        while True:
            line = openocd.stderr.readline().decode("utf-8")
            print(line, end="")
            if "for gdb connections" in line:
                # Grab the port value
                port = line.split(" ")[-4].strip()
                break

        click.secho("Running GDB", bold=True)
        elf_file = {
            "app": APP_ELF,
            "boot": BOOT_ELF,
            "radio": RADIO_ELF,
        }[executable]
        with subprocess.Popen(
            ["arm-none-eabi-gdb", "-ex", f"target extended-remote :{port}", elf_file],
        ) as gdb:
            while True:
                try:
                    gdb.wait()
                    break
                except KeyboardInterrupt:
                    pass
        openocd.kill()


@click.command()
@click.option(
    "--code",
    "-c",
    default="all",
    type=click.Choice(["python", "c", "all"]),
    help="The code to format.",
    required=True,
)
def format_code(code):
    if code in ("python", "all"):
        run_cmd(
            ["isort", "--profile=black", PYTHON_PACKAGE_DIR],
            "Failed to isort Python code",
        )
        run_cmd(["black", PYTHON_PACKAGE_DIR], "Failed to format Python code")
        click.secho("Successfully formatted Python code", bold=True, fg="green")
    if code in ("c", "all"):
        c_files = []
        file_endings = ["*.c", "*.h"]
        for file_ending in file_endings:
            for path in Path(FIRMWARE_SRC_DIR).rglob(file_ending):
                c_files.append(path.as_posix())
        run_cmd(
            ["clang-format", "-i", "-style=file", "-verbose"] + c_files,
            "Failed to format C code",
        )
        click.secho("Successfully formatted C code", bold=True, fg="green")


@click.command()
@click.option(
    "--code",
    "-c",
    default="all",
    type=click.Choice(["python", "c", "all"]),
    help="The code to lint.",
    required=True,
)
def lint(code):
    if code in ("python", "all"):
        run_cmd(
            ["pylint", PYTHON_PACKAGE_DIR, f"--rcfile={PYLINTRC}", "-j4"],
            "Python code failed the linting process",
        )
        click.secho("Successfully linted Python code", bold=True, fg="green")
    if code in ("c", "all"):
        run_cmd(
            ["cppcheck", "--inline-suppr", "--error-exitcode=1", FIRMWARE_SRC_DIR],
            "C code failed the linting process",
        )


@click.command()
def drivers():
    if not os.path.exists(GENERATED_DIR):
        os.makedirs(GENERATED_DIR)

    reg_dev_yaml_filenames = [
        os.path.join(REG_DEV_YAML_DIR, f)
        for f in os.listdir(REG_DEV_YAML_DIR)
        if os.path.isfile(os.path.join(REG_DEV_YAML_DIR, f)) and f.endswith(".yaml")
    ]
    cmd_dev_yaml_filenames = [
        os.path.join(CMD_DEV_YAML_DIR, f)
        for f in os.listdir(CMD_DEV_YAML_DIR)
        if os.path.isfile(os.path.join(CMD_DEV_YAML_DIR, f)) and f.endswith(".yaml")
    ]

    def generate(filename, GeneratorClass):
        click.secho(f"Generating driver code for {filename}")
        with open(filename, "r", encoding="ascii") as file:
            driver_yaml = yaml.safe_load(file)
            driver_gen = GeneratorClass(driver_yaml)
            output_filename = Path(filename).stem
            c_file = os.path.join(GENERATED_DIR, f"{output_filename}.c")
            h_file = os.path.join(GENERATED_DIR, f"{output_filename}.h")
            with open(c_file, "w") as f:
                f.write(driver_gen.generate_source())
            with open(h_file, "w") as f:
                f.write(driver_gen.generate_header())
        output_files.extend([c_file, h_file])

    output_files = []
    for filename in cmd_dev_yaml_filenames:
        generate(filename, CommandDriverGenerator)
    for filename in reg_dev_yaml_filenames:
        generate(filename, RegisterDriverGenerator)

    click.secho("Successfully generated driver code", bold=True, fg="green")
    run_cmd(
        ["clang-format", "-i", "-style=file", "-verbose"] + output_files,
        "Failed to format generated drivers",
    )
    click.secho("Successfully formatted driver code", bold=True, fg="green")


@click.command()
def generate_values():
    values_helper()


@click.command()
def ground():
    from capstone.ground.server import main as ground_station_main

    with subprocess.Popen(
        [
            ENVOY,
            "-c",
            ENVOY_YAML,
        ],
        stderr=subprocess.PIPE,
        stdin=subprocess.PIPE,
    ) as envoy:
        ground_station_main(VALUES)
        envoy.kill()


@click.command()
@click.argument("tag_str", type=click.Choice(VALUE_NAMES.keys()))
@click.argument("value")
def grpc_set(tag_str, value):
    # pylint: disable=import-outside-toplevel,no-name-in-module
    from capstone.proto import ground_pb2, ground_pb2_grpc
    from google.protobuf.empty_pb2 import Empty

    from capstone import values

    channel = grpc.insecure_channel(f"localhost:{GRPC_SERVER_PORT}")
    stub = ground_pb2_grpc.GroundStationStub(channel)
    tag = values.ValueTag[VALUE_NAMES[tag_str]]
    request = ground_pb2.SetValueRequest()
    request.tag = tag
    proto_pack_value(request.value, tag, value)
    assert type(stub.SetValue(request)) == Empty


@click.command()
@click.argument("tag_str", type=click.Choice(VALUE_NAMES.keys()))
def grpc_get(tag_str):
    # pylint: disable=import-outside-toplevel,no-name-in-module
    from capstone.proto import ground_pb2, ground_pb2_grpc

    from capstone import values

    channel = grpc.insecure_channel(f"localhost:{GRPC_SERVER_PORT}")
    stub = ground_pb2_grpc.GroundStationStub(channel)
    tag = values.ValueTag[VALUE_NAMES[tag_str]]
    request = ground_pb2.GetValueRequest()
    request.tag = tag
    response = stub.GetValue(request)
    value = proto_unpack_value(response, tag)
    print(value)


@click.command()
@click.argument("tag_strings", type=click.Choice(VALUE_NAMES.keys()), nargs=-1)
def watch(tag_strings):
    # pylint: disable=import-outside-toplevel,no-name-in-module
    from capstone.proto import ground_pb2, ground_pb2_grpc

    from capstone import values

    stdscr = curses.initscr()
    curses.noecho()
    curses.cbreak()
    stdscr.keypad(True)
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_GREEN, -1)

    channel = grpc.insecure_channel(f"localhost:{GRPC_SERVER_PORT}")
    stub = ground_pb2_grpc.GroundStationStub(channel)
    request = ground_pb2.GetValueRequest()

    while True:
        try:
            stdscr.clear()
            for i, tag_str in enumerate(tag_strings):
                tag = values.ValueTag[VALUE_NAMES[tag_str]]
                request.tag = tag
                response = stub.GetValue(request)
                value = proto_unpack_value(response, tag)
                if (
                    VALUES["values"][tag.name]["type"]["base"] == "int"
                    and VALUES["values"][tag.name]["type"]["hex"]
                ):
                    value = f"0x{value:02X}"

                stdscr.addstr(i, 0, tag_str + ":", curses.A_BOLD | curses.color_pair(1))
                stdscr.addstr(i, len(tag_str) + 2, str(value))
            stdscr.refresh()
            time.sleep(0.05)
        except:
            curses.nocbreak()
            stdscr.keypad(False)
            curses.echo()
            curses.endwin()
            raise


@click.command()
def ui():
    run_cmd(
        ["npm", "run", "dev"], "Failed to run frontend development server", FRONTEND_DIR
    )


@click.command()
def convert():
    with open(VALUES_YAML_DIR + "/values.json", "w+") as f:
        json.dump(VALUES, f)


@click.command()
def controller():
    from capstone.control.controller import main
    main()

def main():
    cli.add_command(build)
    cli.add_command(clean)
    cli.add_command(proto)
    cli.add_command(flash)
    cli.add_command(debug)
    cli.add_command(format_code, "format")  # `format` is a built-in
    cli.add_command(lint)
    cli.add_command(drivers)
    cli.add_command(generate_values, "values")
    cli.add_command(ground)
    cli.add_command(grpc_get, "get")
    cli.add_command(grpc_set, "set")
    cli.add_command(watch)
    cli.add_command(ui)
    cli.add_command(convert)
    cli.add_command(controller)

    # So that generated proto files can import one another.
    sys.path.append(PYTHON_PROTO_DIR)
    sys.path.append(NANOPB_PROTO_DIR)

    try:
        ctx = cli.make_context("cli", sys.argv[1:])
        cli.invoke(ctx)
    except click.exceptions.Exit as exit_exception:
        if exit_exception.exit_code == 0:
            sys.exit(0)
        else:
            raise exit_exception
