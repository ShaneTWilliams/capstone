# capstone

<img width="1203" alt="Screenshot 2023-11-19 at 9 28 59 PM" src="https://github.com/ShaneTWilliams/capstone/assets/44215543/8c8dc27f-abcb-48eb-9e67-2c526ce97632">
<img width="1103" alt="Screenshot 2023-11-19 at 9 29 54 PM" src="https://github.com/ShaneTWilliams/capstone/assets/44215543/a03519ca-c2b6-45cd-9c22-6c8706f74f3d">

## Setup

First, clone submodules with:

```
git submodule update --init
cd software/firmware/lib/pico-sdk
git submodule update --init
```

You don't *have* to, but using a `venv` for the following steps is a good idea. To create one, run:

```
cd software
python -m venv .csvenv
```

Then to activate it, run the following. You'll need to do this each time you open a new shell, but you can put it in your shell's dotfile to automate this.

```
Bash:                   source .csvenv/bin/activate
Csh:                    source .csvenv/bin/activate.csh
Fish:                   source .csvenv/bin/activate.fish
Windows CMD:            .csvenv\Scripts\activate.bat
Windows PowerShell:     .csvenv\Scripts\Activate.ps1
```

### Windows

Open PowerShell as an administrator and install chocolatey using [these instructions](https://chocolatey.org/install#individual).

Still using an administrator shell, install dependencies with:

```
choco install make cmake mingw gcc-arm-embedded protoc python --installargs '"ADD_CMAKE_TO_PATH=User"'
```

In a non-administrator PowerShell, install the Python package with:

```
cd software
pip install -e . --user
```

If you get warnings about the scripts directory not being on your PATH, add it by typing `env` in the Windows search bar and click on "Edit the system environment variables". Next, on the `Advanced` tab, click `Environment Variables`. Under `User Variables`, find `Path`, click on it, select `Edit`, then click `New`. Type in the directory that Pip warned about, and exit.

Close and reopen your PowerShell window.

### Linux

Install dependencies with the following command:

```
sudo apt install cmake protobuf-compiler pnpm
```

Install the [GNU ARM embedded toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) manually.

Install the Python package with:

```
cd software
pip install -e .
```

If you get warnings about the scripts directory not being on your PATH, add it adding the following line to your shell's `rc` dotfile.

```
export PATH=$PATH:<scripts dir here>
```

### MacOS

Install dependencies with the following command:

```
brew install make cmake protobuf gcc-arm-embedded pnpm
```

Install the Python package with:

```
cd software
pip install -e .
```

If you get warnings about the scripts directory not being on your PATH, add it adding the following line to your shell's `rc` dotfile.

```
export PATH=$PATH:<scripts dir here>
```
