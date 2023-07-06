from setuptools import setup

setup(name="capstone",
      version="0.0.1",
      description="Capstone Python Library",
      author="Noah Bailey, Stefan Boon-Petersen, Dylan Matthews, Shane Williams",
      packages=["capstone"],
      package_dir={"": "python"},
      install_requires=["protobuf", "pyserial", "alive-progress", "click"],
      entry_points={
        "console_scripts": [
            "capstone = capstone.cli:cli",
        ],
    },)
