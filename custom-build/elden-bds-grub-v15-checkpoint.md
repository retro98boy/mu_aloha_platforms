# Elden verified BDS / GRUB checkpoint

User hardware reports: 9501/9503 confirm BDS fallback to FrontPage, physical
buttons and Restart now work. 9506 confirms BDS loaded ARM64 GRUB from the
FAT test volume in boot_b and volume-key navigation works in GRUB.
No Linux kernel boot or microSD support has been verified.

Firmware artifact outside this repository:
../porting-artifacts/elden-aloha-bds-keys-v15/boot.img
SHA256: 3b211298df1f47cc35bda6e55c02f020691a4a776414d02080a6b7ccc3c9246f

Test filesystem artifact outside this repository:
../porting-artifacts/elden-grub-bootb-v1/boot_b-fat-grub.img
SHA256: e82117a55bc6447ca43e62333c2e476937816349bed5b19cb017015ae27d3d80

Continue booting slot A. boot_b contains a FAT test filesystem, NOT an Android
boot image. USB/5V bring-up remains paused. UFS write test is disabled.
The MU_BASECORE submodule commit is part of this checkpoint: restore the
recorded submodule revision as well as the parent revision when reproducing.
Build using custom-build/build-elden.sh in the existing Mu Docker environment.
