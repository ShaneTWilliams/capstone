import os
import platform
import shutil
import subprocess
import sys
from glob import glob
from pathlib import Path

import click
from capstone.tools.drivers import DriverGenerator
from capstone.tools.flash import flash as flash_usb
from capstone.tools.flash import get_boards

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
DRIVERS_YAML_DIR = os.path.join(FIRMWARE_DIR, "drivers")
DRIVERS_OUTPUT_DIR = os.path.join(FIRMWARE_DIR, "drivers", "output")

# Files
OPENOCD_CFG = os.path.join(FIRMWARE_DIR, "capstone-ocd.cfg")
BOOT_PROTO = os.path.join(PROTO_DIR, "boot.proto")
APP_PROTO = os.path.join(PROTO_DIR, "app.proto")
GROUND_PROTO = os.path.join(PROTO_DIR, "ground.proto")
PYLINTRC = os.path.join(PYTHON_DIR, "pylintrc")

# Tool binaries
OPENOCD = os.path.join(OPENOCD_DIR, "src", "openocd")

# Binaries
APP_ELF = os.path.join(BUILD_DIR, "app.elf")
BOOT_ELF = os.path.join(BUILD_DIR, "boot.elf")
APP_BIN = os.path.join(BUILD_DIR, "app.bin")
BOOT_BIN = os.path.join(BUILD_DIR, "boot.bin")


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
    cmake_cmd = [
        "cmake",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DFAMILY=rp2040",
        f"-DTARGET={executable}",
    ]
    if platform.system() == "Windows":
        cmake_cmd.extend(["-G", "Unix Makefiles"])
    cmake_cmd.append("..")
    elf_file = APP_ELF if executable == "app" else BOOT_ELF

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
    default="both",
    type=click.Choice(["boot", "app", "both"]),
    help="The executable to build.",
)
def build(executable):
    if executable in ("app", "both"):
        build_helper("app")
    if executable in ("boot", "both"):
        build_helper("boot")


@click.command()
@click.option(
    "--hard", "-h", default=False, type=bool, help="True to remove installations."
)
def clean(hard):
    remove_dir(BUILD_DIR)
    remove_dir(os.path.join(PYTHON_PACKAGE_DIR, "__pycache__"))
    remove_dir(os.path.join(SOFTWARE_DIR, "build"))
    remove_dir(PYTHON_PROTO_DIR)
    remove_dir(DRIVERS_OUTPUT_DIR)
    for path in Path(PYTHON_PACKAGE_DIR).rglob("__pycache__"):
        remove_dir(path.absolute())
    if hard:
        remove_dir(os.path.join(PYTHON_DIR, "capstone.egg-info"))
    click.secho("All temporary directories removed", bold=True, fg="green")


@click.command()
@click.option(
    "--interface",
    "-i",
    default="all",
    type=click.Choice(["app", "ground", "boot", "all"]),
    help="The proto file to build.",
)
def proto(interface):
    if interface in ("boot", "all"):
        proto_helper(os.path.join(PROTO_DIR, "boot.proto"))
    if interface in ("app", "all"):
        proto_helper(os.path.join(PROTO_DIR, "app.proto"))
    if interface in ("ground", "all"):
        proto_helper(os.path.join(PROTO_DIR, "ground.proto"))


@click.command()
@click.option(
    "--executable",
    "-e",
    default="app",
    type=click.Choice(["boot", "app"]),
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
        boards = get_boards()
        if len(boards) == 0:
            click.secho("No flashable devices found", bold=True, fg="red")
        else:
            if len(boards) == 1:
                board = boards[0]
            else:
                click.secho("Multiple flashable devices found:")
                for i, board in enumerate(boards):
                    click.secho(f"    [{i+1}] {board}")
                index = int(input("Select a device: ")) - 1
                board = boards[index]
            build_helper(executable)
            flash_usb(APP_BIN, board)
            click.secho(f"Successfully flashed {APP_BIN}", bold=True, fg="green")


@click.command()
@click.option(
    "--executable",
    "-e",
    default="app",
    type=click.Choice(["boot", "app"]),
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
        elf_file = APP_ELF if executable == "app" else BOOT_ELF
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
            for file in glob(os.path.join(FIRMWARE_SRC_DIR, "**", file_ending)):
                c_files.append(file)
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
    if not os.path.exists(DRIVERS_OUTPUT_DIR):
        os.makedirs(DRIVERS_OUTPUT_DIR)

    input_filenames = [
        os.path.join(DRIVERS_YAML_DIR, f)
        for f in os.listdir(DRIVERS_YAML_DIR)
        if os.path.isfile(os.path.join(DRIVERS_YAML_DIR, f)) and f.endswith(".yaml")
    ]

    output_files = []
    for filename in input_filenames:
        click.secho(f"Generating driver code for {filename}")
        driver_gen = DriverGenerator(filename)
        output_filename = os.path.join(
            DRIVERS_OUTPUT_DIR, os.path.basename(filename).removesuffix(".yaml")
        )
        header_file = output_filename + ".h"
        source_file = output_filename + ".c"
        output_files.extend([source_file, header_file])
        with open(header_file, "w+", encoding="utf-8") as file:
            file.write(driver_gen.generate_header())
        with open(source_file, "w+", encoding="utf-8") as file:
            file.write(driver_gen.generate_source())
    click.secho("Successfully generated driver code", bold=True, fg="green")

    run_cmd(
        ["clang-format", "-i", "-style=file", "-verbose"] + output_files,
        "Failed to format generated drivers",
    )
    click.secho("Successfully formatted driver code", bold=True, fg="green")


def main():
    cli.add_command(build)
    cli.add_command(clean)
    cli.add_command(proto)
    cli.add_command(flash)
    cli.add_command(debug)
    cli.add_command(format_code, "format")
    cli.add_command(lint)
    cli.add_command(drivers)

    try:
        ctx = cli.make_context("cli", sys.argv[1:])
        cli.invoke(ctx)
    except click.exceptions.Exit as exit_exception:
        if exit_exception.exit_code == 0:
            sys.exit(0)
        else:
            raise exit_exception
