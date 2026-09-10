# Elden V1：设备输入与内存初始化阶段

已注册 Sm8750/PakalaPkg/lenovo-elden，Docker 内完整 DEBUG 构建成功。现已制作带 AVB footer 和原厂配套的单槽试启动包，**未经过真机启动验证**。主移植记录在仓库上一级 Elden 目录的 README.md：第 16 节说明构建，第 17 节说明试启动包。

- `Binaries/` 来自选定的 lenovo-y700g4 备份，不混入自行提取版本。
- 备份重复 Ebl 的两个可执行文件均原样保存，供追溯；实际两份 DXE 清单去掉末尾重复引用。`.efi.1` 没有作为另一个组件注册。
- `memory-layout.json` 是 V1 地址与来源记录。`dxe_container` 包含新栈，`dxe_allocatable` 不含新栈；两者不是两个可重复分配的区域。
- `binary-sha256.json` 保存全部导入文件的 hash，包含经过清单去重后的文件。
- build_cfg/sm8750.json、DSC/FDF 和专用 SEC 已接入；使用下述脚本强制重建 BootShim 并传播构建失败状态。

检查命令（从仓库根目录执行）：

```bash
python3 custom-build/check-elden-inputs.py
python3 custom-build/test-elden-memory.py
bash custom-build/build-elden.sh
```

内存库必须与设备专用 MemoryInitPeiLib 配套：DXE Heap 是查找用容器，由 DXE Free/UEFI Stack 两个叶区间描述实际资源；栈 allocation HOB 由公共 PrePi 后续 BuildStackHob 生成。配置表保留原厂 CPU 数量，显式关闭首版未验证的多线程/多核解压等功能，但这些开关本身不保证其他核心已停驻。

MemoryPcds.dsc.inc 已被平台 DSC 引用，产物检查确认链接 EldenMemoryInitPeiLib。仅导入 10 个基础服务模块，未导入原厂 Pmic/Buttons/Charger/ResetRuntime/Watchdog，未使用任何其他设备的 PatchedBinaries。显示和存储依赖仍待补齐。

诊断应用已编译并打入 FV，需文本控制台/GOP 就绪才能显示；未自动运行，当前不保证设备亮屏。按用户最新要求，诊断应用不读取任何实体按键，不改变原厂 PON/长按复位配置。背光和音量键测试后置，电源键不纳入测试动作。

完整编译之后，在宿主机执行 `python3 custom-build/package-elden-flash.py`。默认从上一级原厂 1.5.10.259 包取输入，并调用本机 SM8750 kernel-platform 预编译 avbtool；可用 `--stock-dir`、`--avbtool` 指定路径，但原厂镜像必须匹配脚本的基准 hash。默认输出到上一级 `porting-artifacts/elden-aloha-v1-flash`，拒绝覆盖已有目录，可用 `--output` 指定新目录。不需要重建 Docker 镜像。

不要刷 `Build/PakalaPkg/lenovo-elden.img`；按生成包 README 的条件核对后，使用包根目录 `boot.img`，原厂恢复镜像在 `stock/boot.img`。本包不自动连接或刷写设备。
