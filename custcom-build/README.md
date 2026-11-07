# mu_aloha_platforms Docker build

This Ubuntu 24.04 environment includes the AArch64 toolchain, NuGet and avbtool.
uv 0.12.13 installs Python 3.10.21 and creates the virtual environment at
`/opt/mu-aloha-platforms/venv`, which is already on `PATH`.

## Build the Docker image

Run from the repository root on the host:

```sh
git submodule update --init --recursive
docker buildx build --load \
    -f custcom-build/Dockerfile \
    -t retro98boy/mu-aloha-platforms-build .
```

`--load` makes the image available to the local Docker engine.

## Enter the build environment

Run from the repository root on the host:

```sh
docker run --rm -it \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp/mu-aloha-platforms-home \
    -v "$PWD:/workspace/mu-aloha-platforms" \
    -w /workspace/mu-aloha-platforms \
    retro98boy/mu-aloha-platforms-build
```

The host UID/GID keeps source files and build outputs owned by the current user.
Inside the container, initialize the temporary home and Git trust settings:

```sh
mkdir -p "$HOME"
git config --global --add safe.directory /workspace/mu-aloha-platforms
git config --global --add safe.directory '/workspace/mu-aloha-platforms/*'
```

All commands below run inside this container. Source files and build outputs
remain in the host checkout after the container exits.

## Build one device

For Lenovo Y700 Gen4, initialize the repository's build environment and build a
RELEASE image without Secure Boot:

```sh
python3 build_uefi.py --init -d lenovo-elden -s 0 -t RELEASE
```

`--init` runs the repository's Stuart setup/update steps before compiling. Omit
it on subsequent builds; use it again when dependency metadata changes.
The output image is `Build/PakalaPkg/lenovo-elden.img`.

For another device, such as Xiaomi Pad 6S Pro (`xiaomi-sheng`):

```sh
python3 build_uefi.py -d xiaomi-sheng -s 0 -t RELEASE
```

Its image is `Build/KailuaPkg/xiaomi-sheng.img`. Replace `RELEASE` with `DEBUG`
for a single-device debug build. Use `-s 1` for devices supporting Secure Boot.
Available device and platform names are listed in the root README and
`build_cfg/*.json`.

## Build all targets

Build every device in the SM8550 platform:

```sh
python3 build_uefi.py -d all -p Sm8550
```

Build every device across all configured platforms:

```sh
python3 build_uefi.py -p all
```

The all-platform path creates its own targets with the default RELEASE/NOSB
settings; it does not propagate the command-line `-t` and `-s` values to them.
Images are written to `Build/<Package>/<device>.img`. Run builds sequentially:
BootShim and package output directories are shared between devices.

The existing `build_uefi.py` wrapper does not propagate every child command's
failure. Check the build summary and newly generated artifacts, rather than
relying only on its exit status. These commands build images; they do not flash
any device. The environment has been verified with Lenovo Y700 Gen4; the other
examples describe supported build entry points, not validation of every target.

## Generate compile_commands.json for a host editor

Inside the container, enable the built-in `COMPILE_INFO` report. For Lenovo
Y700 Gen4:

```sh
python3 Platforms/PakalaPkg/PlatformBuildNoSb.py \
    TARGET=RELEASE TARGET_DEVICE=lenovo-elden \
    "BUILDREPORT_TYPES=PCD DEPEX FLASH BUILD_FLAGS LIBRARY FIXED_ADDRESS HASH COMPILE_INFO"
```

For another device, select its platform build entry point and `TARGET_DEVICE`.
The database is generated at
`Build/<Package>/<TARGET>_CLANGPDB/CompileInfo/compile_commands.json`.

Exit the container, then run these commands from the repository root **on the
host**. The example assumes a host checkout path without spaces, quotes or
backslashes:

```sh
mv Build/PakalaPkg/RELEASE_CLANGPDB/CompileInfo/compile_commands.json ./compile_commands.json
host_root=$(pwd -P)
# Escape characters that have special meaning in sed's replacement string.
sed_root=$(printf '%s' "$host_root" | sed 's/[&|]/\\&/g')
sed -i "s|/workspace/mu-aloha-platforms|$sed_root|g" compile_commands.json
```

clangd can discover the database at the repository root. Keep the corresponding
`Build/` directory: the commands also reference generated headers such as
`AutoGen.h` and compiler response files. Paths inside those response files and
container-only compiler/toolchain paths are not rewritten by this command; a
host editor may need matching tools or additional path adjustments. Regenerate
and repeat the move/path replacement after changing devices or build targets.
