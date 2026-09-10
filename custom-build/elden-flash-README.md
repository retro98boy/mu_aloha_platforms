# Elden Aloha V1：首次刷入试启动包

适用：已解锁的 Lenovo Elden / SM8750 / TB322，原厂基线 **ZUXOS_1.5.10.259**。
这是用于真机 bring-up 的 boot 镜像，不是可用的 Armbian，也不是已经验证能启动的 UEFI。
目前没有可保证工作的屏幕日志，可能停在旧 Logo、黑屏或返回 fastboot；仅凭这些现象无法判断是否到达新 UEFI。

## 本包做了什么

- 根目录 `boot.img`：原厂完整 v4 header 页仅修改 kernel_size；载荷替换为 gzip(BootShim + Elden FD)。`stock/boot.img` 才是原厂恢复镜像，勿混淆。
- 按原厂 96 MiB boot 分区生成 AVB 1.0 footer、boot SHA256 descriptor；算法 NONE、flags=0。保留原厂 rollback index 和 boot 属性用于兼容性；属性不是新载荷的 Android 身份证明。
- 新 footer 不是联想签名。保留原厂顶层 vbmeta；依赖解锁状态允许 unsigned verification error。本地 ABL/libavb 源码支持该路径，但未证明原厂二进制和真机一定接受。
- 原厂 kernel 后有额外 16 KiB 数据（起始为 AVB0），v4 signature_size 为 0。本包不复制失效的旧签名数据，也不伪造 GKI 签名。
- `stock/`：原封不动复制的同版本 boot、vendor_boot、init_boot、dtbo、vbmeta，用于配套和恢复；不是从你当前设备读取的备份。
- 不修改 ABL/XBL/UEFI 固件分区、GPT、super、userdata，不改电源键和 PMIC 长按配置；不提供自动刷机脚本。
- 未完成显示/UFS/GRUB，EFI 诊断程序已在 FV 中但尚未自动运行。

## 刷入前必须确认

1. 当前设备确为本机型，系统和启动固件匹配上述原厂版本。若不匹配，先停止，按当前系统的原厂镜像重新配套；不要为本包盲目降级整个固件。
2. Bootloader 已解锁；测试期间绝对不要重新上锁。不需要 unlock_critical。
3. **先在原厂环境实测强制重启及重新进入 bootloader 的按键操作。** 未修改电源键不等于我们已验证卡死时可恢复；本包不承诺单独长按电源必定复位。
4. 保存当前各启动分区的备份或确认能恢复的同版本原厂包。若此前两槽都被改过，不要把另一个槽当作已验证的退路。
5. 只测试一个槽，不同时覆盖 a/b。下文只以 a 为例；若 a 是唯一可工作的原厂槽，应保留 a 并对所有命令一致地改用 b。
6. USB 连接稳定、电量充足；先记录下列只读命令结果。任何分区容量、锁状态或机型不符都停止。

进入 bootloader fastboot 后，在此目录执行：

```bash
sha256sum -c SHA256SUMS
fastboot getvar product
fastboot getvar current-slot
fastboot getvar unlocked
fastboot getvar is-userspace
fastboot getvar max-download-size
fastboot getvar partition-size:boot_a
fastboot getvar partition-size:vendor_boot_a
fastboot getvar partition-size:init_boot_a
fastboot getvar partition-size:dtbo_a
```

要求在 bootloader fastboot，不是 fastbootd。不同实现可能不支持部分 getvar；未知值不能当作检查通过。
容量应为：boot/vendor_boot = `0x6000000`（100663296），init_boot = `0x800000`，dtbo = `0x2000000`。
本包未拆分镜像，max-download-size 应至少为 96 MiB。设备当前系统版本不能由 getvar product 证明，需要你核对原厂系统信息。

## 刷入示例：只测试 slot a

以下是**写入命令**，完成上述确认后才执行。每条成功后才继续；任何 FAILED 都停止，不要自动重启。

因为之前的 Armbian 测试可能擦除了 dtbo 或替换 vendor_boot/init_boot，先恢复这三个同版本原厂配套，再刷新的 boot：

```bash
fastboot flash vendor_boot_a stock/vendor_boot.img
fastboot flash init_boot_a stock/init_boot.img
fastboot flash dtbo_a stock/dtbo.img
fastboot flash boot_a boot.img
fastboot set_active a
fastboot getvar current-slot
```

确认返回 a，再单独执行：

```bash
fastboot reboot
```

不擦 dtbo，不刷 dummy DTBO：这次载荷是 Aloha，不是直接使用主线 DTB 的 Linux；ABL 先处理原厂 vendor_boot 的 DTB 和匹配的原厂 DTBO。
**默认不刷 vbmeta、不禁用全局验证**。如果此前已改过 vbmeta，先核实状态；`stock/vbmeta.img` 仅供同版本完整恢复，不要把随手更改 vbmeta flags 当作通用修复。
不把 `fastboot boot` 视作已经验证可用的替代路径。

## 失败后恢复

使用你已实测的按键操作回到 bootloader；不要反复重启猜测。

如果 b 是你此前亲自验证能启动、且本次未修改的原厂槽，可以切回 b：

```bash
fastboot set_active b
fastboot reboot
```

否则，回到 bootloader 后恢复本次修改的 a 槽四个分区（仍要求同版本原厂基线）：

```bash
fastboot flash boot_a stock/boot.img
fastboot flash vendor_boot_a stock/vendor_boot.img
fastboot flash init_boot_a stock/init_boot.img
fastboot flash dtbo_a stock/dtbo.img
fastboot set_active a
```

四条刷写均成功且槽位正确后再 `fastboot reboot`。本流程未修改 vbmeta，因此通常不需要恢复它；如果它在早先测试中就已被改过，需结合当前系统另行核对。
这些文件只恢复这里列出的启动分区，不恢复本次测试之前其他修改，也不是保数据完整救砖包。如果无法回到 bootloader，停止试验；本包没有验证 EDL 恢复流程。

## 测试时记录

保存全部 fastboot 输出、重启后的屏幕视频、USB 枚举和等待时长；返回 fastboot 后重新记录 current-slot。
旧 Logo 不代表 Hello 已运行；回 fastboot 也不能直接归因于 AVB。新 UEFI 尚未实现 Android 槽成功标记，反复试启动可能改变 A/B 重试状态。
本包内 `manifest.json`、`boot-avb-info.txt`、`boot-avb-verify.txt` 和 SHA256SUMS 用于复核实际产物；AVB 自校验通过不是联想签名认证或真机启动证明。
