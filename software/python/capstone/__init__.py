import os
import sys

SOFTWARE_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
)
FIRMWARE_DIR = os.path.join(SOFTWARE_DIR, "firmware")
NANOPB_DIR = os.path.join(FIRMWARE_DIR, "build/nanopb/generator/proto/")
sys.path.append(NANOPB_DIR)
