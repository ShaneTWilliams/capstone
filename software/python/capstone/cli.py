import os
import subprocess
import shutil
from pathlib import Path
import platform
import sys

import click

if platform.system() == "Windows":
    OS_DIR = "windows-x86"
elif platform.system() == "Linux":
    OS_DIR = "linux-x86"
elif platform.system() == "Darwin":
    OS_DIR = "macos-x86"
else:
    raise RuntimeError("Unsupported OS")

# Directories
SOFTWARE_DIR = os.path.relpath(os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__)))))
FIRMWARE_DIR = os.path.join(SOFTWARE_DIR, "firmware")
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

# Files
OPENOCD_CFG = os.path.join(FIRMWARE_DIR, "capstone-ocd.cfg")
BOOT_PROTO = os.path.join(PROTO_DIR, "boot.proto")
DRONE_PROTO = os.path.join(PROTO_DIR, "drone.proto")
GROUND_PROTO = os.path.join(PROTO_DIR, "ground.proto")

# Tool binaries
PROTOC = os.path.join(TOOLS_DIR, "protoc", OS_DIR, "bin", "protoc")
OPENOCD = os.path.join(OPENOCD_DIR, "src", "openocd")

# Binaries
DRONE_APP_ELF = os.path.join(BUILD_DIR, "drone-app.elf")
DRONE_BOOT_ELF = os.path.join(BUILD_DIR, "drone-boot.elf")
GROUND_APP_ELF = os.path.join(BUILD_DIR, "ground-app.elf")
GROUND_BOOT_ELF = os.path.join(BUILD_DIR, "ground-boot.elf")
DRONE_APP_BIN = os.path.join(BUILD_DIR, "drone-app.bin")
DRONE_BOOT_BIN = os.path.join(BUILD_DIR, "drone-boot.bin")
GROUND_APP_BIN = os.path.join(BUILD_DIR, "ground-app.bin")
GROUND_BOOT_BIN = os.path.join(BUILD_DIR, "ground-boot.bin")

def run_cmd(cmd, error, cwd=None):
    click.secho(f"Running", bold=True, nl=False)
    for text in cmd:
        if " " in text:
            click.secho(f" \"{text}\"", nl=False, underline=True)
        else:
            click.secho(f" {text}", nl=False, underline=True)
    if cwd is not None:
        click.secho(f" in ", bold=True, nl=False)
        click.secho(cwd, underline=True, nl=False)
    click.secho("")

    if subprocess.Popen(cmd, cwd=cwd).wait() != 0:
        click.secho(error, bold=True, fg="red")
        sys.exit(1)

def remove_dir(path):
    shutil.rmtree(path, ignore_errors=True)
    click.secho(f"Removed {path}")

def build_helper(hardware, type):
    cmake_cmd = ["cmake", "-DCMAKE_BUILD_TYPE=Debug", "-DFAMILY=rp2040", f"-DHARDWARE={hardware}", f"-DTYPE={type}", ".."]
    binary = os.path.join(BUILD_DIR, f"{hardware}-{type}.elf")

    click.secho(f"Building {binary}", bold=True)
    Path(f"{FIRMWARE_DIR}/build").mkdir(exist_ok=True)

    run_cmd(cmake_cmd, f"Failed to configure {binary}", cwd=BUILD_DIR)
    run_cmd(["make", "-j4"], f"Failed to build {binary}", cwd=BUILD_DIR)
    run_cmd(["arm-none-eabi-size", binary], f"Failed to read out {binary} size")
    click.secho(f"Successfully built {binary}", bold=True, fg="green")

def proto_helper(input_file):
    Path(PYTHON_PROTO_DIR).mkdir(exist_ok=True)
    cmd = [PROTOC, f"--python_out={PYTHON_PROTO_DIR}", f"-I={NANOPB_PROTO_DIR}", f"-I={PROTO_DIR}", input_file]
    output_file = os.path.join(PYTHON_PROTO_DIR, os.path.basename(input_file).replace(".proto", "_pb2.py"))
    run_cmd(cmd, f"Failed to generate protos from {input_file}")
    click.secho(f"Successfully generated {output_file}", bold=True, fg="green")

@click.group()
def cli():
    pass

@click.command()
@click.option("--type", "-t", default="both", type=click.Choice(["boot", "app", "both"]), help="The application to build.")
@click.option("--hardware", "-h", default="both", type=click.Choice(["drone", "ground", "both"]), help="The hardware to build for.")
def build(type, hardware):
    if type == "app" or type == "both":
        if hardware == "drone" or hardware == "both":
            build_helper("drone", "app")
        if hardware == "ground" or hardware == "both":
            build_helper("ground", "app")
    if type == "boot" or type == "both":
        if hardware == "drone" or hardware == "both":
            build_helper("drone", "boot")
        if hardware == "ground" or hardware == "both":
            build_helper("ground", "boot")

@click.command()
@click.option("--hard", "-h", default=False, type=bool, help="True to remove installations.")
def clean(hard):
    remove_dir(BUILD_DIR)
    remove_dir(os.path.join(PYTHON_PACKAGE_DIR, "__pycache__"))
    remove_dir(os.path.join(SOFTWARE_DIR, "build"))
    remove_dir(os.path.join(PYTHON_PROTO_DIR))
    if hard:
        remove_dir(os.path.join(PYTHON_DIR, "capstone.egg-info"))
    click.secho("All temporary directories removed", bold=True, fg="green")

@click.command()
@click.option("--interface", "-i", default="all", type=click.Choice(["drone", "ground", "boot", "all"]), help="The proto file to build.")
def proto(interface):
    if interface == "boot" or interface == "all":
        proto_helper(os.path.join(PROTO_DIR, "boot.proto"))
    if interface == "drone" or interface == "all":
        proto_helper(os.path.join(PROTO_DIR, "drone.proto"))
    if interface == "ground" or interface == "all":
        proto_helper(os.path.join(PROTO_DIR, "ground.proto"))

@click.group()
def flash():
    pass

@click.command()
@click.option("--type", "-t", type=click.Choice(["boot", "app"]), help="The application to build.", required=True)
@click.option("--hardware", "-h", type=click.Choice(["drone", "ground"]), help="The hardware to build for.", required=True)
def flash_swd(type, hardware):
    if platform.system() == "Windows":
        click.secho(f"You can't do that on Windows. Come back with a real computer.", bold=True, fg="red")
        sys.exit(1)

    if not os.path.isfile(OPENOCD):
        click.secho(f"Couldn't find {OPENOCD}. Building now.", bold=True, fg="yellow")
        run_cmd(["./bootstrap"], f"Failed to bootstrap OpenOCD", cwd=OPENOCD_DIR)
        run_cmd(["./configure", "--disable-werror", "--enable-picoprobe"], f"Failed to configure OpenOCD", cwd=OPENOCD_DIR)
        run_cmd(["make", "-j4"], f"Failed to build OpenOCD", cwd=OPENOCD_DIR)
        click.secho(f"Successfully built OpenOCD", bold=True, fg="green")

    build_helper(hardware, type)
    binary = os.path.join(BUILD_DIR, f"{hardware}-{type}.elf")

    click.secho(f"Flashing {binary}", bold=True)
    run_cmd([OPENOCD, "-f", "interface/cmsis-dap.cfg", "-f", "target/rp2040.cfg", "-f", OPENOCD_CFG, "-s", OPENOCD_TCL_DIR, "-c", f"program {binary} reset exit"], f"Failed to flash {binary}")
    click.secho(f"Successfully flashed {binary}", bold=True, fg="green")

@click.command()
@click.option("--hardware", "-h", type=click.Choice(["drone", "ground"]), help="The hardware to build for.", required=True)
def flash_usb(hardware):
    # We need the Python proto to exist before we can import the flash file.
    proto_helper(BOOT_PROTO)
    from capstone.flash import flash as flash_app

    if hardware == "drone":
        flash_app(DRONE_APP_BIN)
    elif hardware == "ground":
        flash_app(GROUND_APP_BIN)

@click.command()
@click.option("--type", "-t", type=click.Choice(["boot", "app"]), help="The application to build.", required=True)
@click.option("--hardware", "-h", type=click.Choice(["drone", "ground"]), help="The hardware to build for.", required=True)
def debug(type, hardware):
    click.secho("Running OpenOCD", bold=True)
    # OpenOCD logs to STDERR because of course it does.
    openocd = subprocess.Popen([OPENOCD, "-f", "interface/cmsis-dap.cfg", "-f", "target/rp2040.cfg", "-f", OPENOCD_CFG, "-s", OPENOCD_TCL_DIR], stderr=subprocess.PIPE, stdin=subprocess.PIPE)
    while True:
        line = openocd.stderr.readline().decode("utf-8")
        print(line, end="")
        if "for gdb connections" in line:
            # Grab the port value
            port = line.split(" ")[-4].strip()
            break

    click.secho("Running GDB", bold=True)
    gdb = subprocess.Popen(["arm-none-eabi-gdb", f"-ex" , f"target extended-remote :{port}", DRONE_APP_ELF])
    gdb.wait()
    openocd.kill()
    # $(OPENOCD) -f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2040.cfg -s firmware/lib/openocd/tcl

cli.add_command(build)
cli.add_command(clean)
cli.add_command(proto)
cli.add_command(flash)
cli.add_command(debug)
flash.add_command(flash_swd, "swd")
flash.add_command(flash_usb, "usb")
