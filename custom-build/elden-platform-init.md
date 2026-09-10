# Elden 平台初始化与原版 MU_BASECORE

## 边界

`MU_BASECORE` 回到 Elden 移植前的上游提交
`762a65dad79e2934cddbefa467b7e2c7b5ee4bc0`。不再给 DXE Core、Dispatcher、
Image、GCD 或内存分配器打 Elden 补丁，构建脚本也不向子模块应用补丁。
原移植提交仍保存在子模块的 `elden-microsd-bringup` 分支，未删除历史。

平台功能位于 `Platforms/PakalaPkg/Device/lenovo-elden/`：

- `EldenStorageDxe/`：由 DXE Dispatcher 根据 DEPEX 调度，执行 UFS 前置检查、
  SD 供电和原厂驱动适配；不依赖 BDS 调用。
- `EldenBootManager/EldenConnect.c`：只读 GPT 校验、存储和显示控制器连接。
  原有 EFI 选择框、按键和倒计时不变。
- `Override/`：Elden 专用的 GraphicsConsoleDxe、PartitionDxe、DxeRngLib、
  FrameBufferBltLib。来源和各自差异见 `Override/README.non-core.md`。

DSC/FDF 的替换受 `TARGET_DEVICE == lenovo-elden` 限制；其他设备继续引用上游
模块。保留原模块版权和许可证，没有另复制整个 DxeCore。

原 `ELDEN_DXE_TRACE` 宏还控制早期 framebuffer 光标保留、滚屏和每次启动的
清屏，并非只有日志作用。这些行为保留，改用独立的
`ELDEN_FRAMEBUFFER_CONSOLE` 宏；RNG 兼容在 Elden 专用库中直接启用。

## 初始化顺序

1. 原版 DXE Dispatcher 初始化公共服务；原版 Core 检查 architectural protocols。
2. `EldenStorageDxe` 的 DEPEX 等待 architectural protocols、Env/DALSys、
   Clock/NPA/HWIO 等服务，然后在 DXE 入口同步初始化。
3. 启动 HALIOMMU 并检查真实返回值；成功后执行 UFS software/SID/HWIO/UIC
   检查及 UFS 启动，再启动 SD、触发枚举和执行固定电压适配。
4. UFS 启动成功才发布 `gEldenUfsReadyProtocolGuid` 标记。不能仅凭 BlockIo
   已存在就认定原厂驱动启动成功。
5. 驱动返回后，由外层 Dispatcher 调度新协议的消费者；驱动中不递归调用
   `Dispatch()`。BDS 阶段才校验 GPT、连接分区/文件系统以及显示/UI。
6. 由已有选择框选择 EFI 程序。

显示协议替换由公共 `GopOverrideDxe` 完成，与其他平台一致；板级代码只连接
其控制器，再调度 RenderingEngine/DisplayEngine 和连接 OSK，不再自行安装、
卸载或回退 GOP 协议。EFI 选择框和倒计时没有改动。

启动路径不再执行协议清点、寄存器快照、UFS 启动后的 UIC 采样，以及 SD
重复扇区/EFI 文件试读；离线 SD FAT 诊断解析器及其测试已删除。仍保留决定
UFS 启动的前置检查、只读 GPT 校验、SD 电源事务和原厂驱动适配。文件系统
识别交给正常驱动连接，EFI 候选文件检查仍由启动选择器完成。

HALIOMMU/UFS/SD 的 PE 载荷改用 `FREEFORM` FV 文件保存，不再被 Dispatcher
自动发现。平台通过标准 FV 设备路径调用 `LoadImage()/StartImage()`，保留镜像
认证路径，并显式检查原依赖/前置条件。这样能获取真实返回值和 SD 镜像地址，
也不会和自动调度重复加载。一次初始化中，选定镜像加载或启动失败后不再重试。

这不等于用“协议已存在”代替启动成功：HALIOMMU 可能先发布协议再失败。
失败时不使用其早期协议继续初始化。SD 镜像地址/大小仅在启动成功后保存，
仍保留对镜像大小、指令和配置字节的校验，避免访问失败后已卸载的镜像。

初始化保持同步运行于 `TPL_APPLICATION`，不改为通知回调或库构造函数。
SD 的短暂高 TPL 事务和恢复保持原有顺序。没有新增 RPMh 驱动、AP 启动、
USB 电源策略或磁盘写入。SD 直接 TCS 操作仍是当前板级实现的限定行为，
不能据此推断未来加入其他 RPMh 使用者也安全。

## 验证与构建

```sh
python3 custom-build/test-elden-platform-boundary.py
python3 custom-build/test-elden-fv-loader.py
python3 custom-build/test-elden-sdcc.py
python3 custom-build/test-elden-sd-vote.py
python3 custom-build/test-elden-gpt.py
python3 custom-build/test-elden-gop-copy.py
python3 custom-build/test-elden-rng-query.py
python3 custom-build/test-elden-efi-choice.py
```

使用已有构建容器，在仓库根目录运行：

```sh
docker run --rm -v "$PWD:/workspace" retro98boy/mu-aloha-dev:latest \
    bash custom-build/build-elden.sh DEBUG
```

`check-elden-output.py` 除原有载荷和电源隔离检查外，也检查三个平台启动镜像
确实打包为 FREEFORM、没有自动调度 DEPEX、PE 内容与预期一致。
板级 `image_validation.cfg` 让 FREEFORM PE 使用原有 AARCH64 默认校验要求
（4 KiB 对齐、可写/可执行分离），没有排除这些镜像或关闭检查。

`Build/PakalaPkg/lenovo-elden.img` 仍是工程容器。正式测试包继续使用现有
`custom-build/package-elden-flash.py --output 新目录` 生成；不要把工程容器
误当成原厂格式的刷入包。

主机测试和编译无法验证硬件时序。真机应分别测试有卡/无卡、UFS/SD EFI
发现与选择、按键倒计时、屏幕控制台，以及 Linux 接管后的 Wi-Fi/USB。
本次没有改变原有 USB 隔离策略。
