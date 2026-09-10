#!/usr/bin/env bash
# Run inside retro98boy/mu-aloha-dev with the repository mounted at /workspace.
# Unlike build_uefi.py's os.system wrapper, propagate a failed Stuart build.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
target="${1:-DEBUG}"
if [[ "$target" != DEBUG ]]; then
    echo 'Only the DEBUG bring-up profile has been validated.' >&2
    exit 2
fi
python3 custom-build/check-elden-inputs.py
python3 custom-build/prepare-elden-ufs.py
python3 custom-build/prepare-elden-i2c.py
python3 custom-build/prepare-elden-usb-probe.py
python3 custom-build/prepare-elden-sdcc.py
# Force regeneration: Make does not track changes to UEFI_BASE/UEFI_SIZE flags.
make -B -C BootShim UEFI_BASE=0xD7C00000 UEFI_SIZE=0x00400000
python3 Platforms/PakalaPkg/PlatformBuildNoSb.py TARGET="$target" TARGET_DEVICE=lenovo-elden
python3 custom-build/check-elden-output.py
