# Elden 独立屏幕探针 v1

## 2026-09-11 真机检查点

用户刷入本版后提供照片，Lenovo Logo仍在，照片左下侧可见清晰黑白棋盘格。
对应测试boot SHA256：`cc59d609c363fa1b22d0d77997d65810f48262a024029c8854a1c057d3bcf420`。
这证明本次ABL已执行探针、入口保护和运行时FDT检查通过，候选地址及stride下的写入可见。
照片横向摆放，因此代码坐标不应直接等同照片坐标；黑白图案不能证明RGB/BGR色序。
探针不含FD，不能据此声称Aloha、内存初始化或DXE成功。

保存分支：`checkpoint/elden-screen-probe-v1-working`；开发分支：`lenovo-elden-wip`。
本检查点包含平台源码、测试和已导入的固件输入，不包含生成的boot镜像。
复现仍需构建脚本固定hash的外部原厂boot和运行时DTB，见脚本及父目录移植记录。
后续定位顺序：保留这个独立探针基线，在新的试验中增加FD跳转前、PrePi入口、
内存初始化后和DXE交接前标记。每步仅报告最后实际可见阶段，不改电源键。

这是定位用 boot，不含 Aloha FD，不启动 Android/GRUB，不测试 USB。
使用已核实的 1.1.11.263 原厂 boot_a.img 头；stock/boot.img 是该原始备份的逐字节副本。
只适用于已解锁、原厂 1.1.11.263 配套分区完整、明确测试 a 槽的 Elden。
不刷 vendor_boot/init_boot/dtbo/vbmeta，不擦分区，不动 b 槽。
若系统已升级或配套分区改过，先停止，不混用本包。

## 预期与限制

ABL 交接后，探针检查 EL1/EL2、MMU和数据缓存关闭、x0处于候选DTB范围，
再检查运行时 FDT 的 Elden 3k 面板选择及 splash=0xfc800000/0x02b00000。
全部通过才按 **待真机验证的** 1904×3040、BGRA32、stride=7616 候选，
在左上方 (64,64) 写 256×64 黑白棋盘格。仅写这块矩形，然后主动 WFE 停住。
不访问存储/PMIC/显示控制器寄存器，不改电源键、看门狗或背光。
有棋盘格说明探针已执行到写屏；不是完整 Aloha 启动成功。
没有棋盘格不能区分 ABL 未交接、保护检查拒绝、地址/格式假设不符或显示未刷新。
不会出现 Hello、Android 动画或新的 USB 枚举；停住是设计行为。
AVB algorithm=NONE，仅自哈希验证通过，不是联想签名，不保证设备接受。

## 操作（宿主机，逐条执行）

在本包目录执行，先确认恢复文件、已实测强制重启方式和槽位：

```bash
sha256sum -c SHA256SUMS
fastboot getvar current-slot
# 仅当确认为 a 且版本/配套分区满足以上前提：
fastboot flash boot_a boot.img
fastboot reboot
```

建议录像：Logo → Orange State → 是否出现小棋盘格，记录位置与形状。
当前设备已实测：长按音量减＋电源重启，随后立即改按音量加＋电源进 bootloader。
此经验不保证任何故障都可恢复。进入 bootloader 后恢复同版本原厂 boot：

```bash
fastboot flash boot_a stock/boot.img
fastboot reboot
```

不要用旧 V1 包中 1.5.10.259 的 stock/boot.img 恢复当前 1.1.11.263。
本包构建与验证没有连接或刷写设备。
