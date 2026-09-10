# Elden Aloha BDS 内存所有权修复 v15（尚未真机验证）

## 当前镜像 v15

9499 的 Pool.c 断言对应 v14 的显示 DevicePath 所有权错误：平台库
返回了借用的协议指针，但上层 BDS 会 FreePool。改成返回独立的
DuplicateDevicePath 分配，原始 GOP 设备路径仍由驱动持有。
保留 BDS、实体键、FrontPage 退出冷重启；不屏蔽内存校验断言。
请重测进入界面和按键导航，再测 Exit / Restart now。

## 历史镜像 v14

## 当前镜像 v14

移除 DXE 主动暂停和音量加重启轮询，交回标准 BDS。Elden 专用
DeviceBootManagerLib 注册控制台并刷新启动项；无可启动项时打开
FrontPage，正常退出 FrontPage 后冷重启。日志保留，USB/5V 仍暂停。
这是首次验证 BDS 全路径，尚不能保证所有启动项、EFI 或内核可用。

EldenKeys 提供标准 SimpleTextIn/InputEx，仅读取已核对的 SPMI 状态：
SID1 GPIO6 RT_STS 0x8d10 bit0 低有效为音量加；SID0 PON 0x1310
bit6/7 高有效分别为音量减/电源。未写 PON、IRQ 或充电配置。
短按释放后触发：音量加/减为上/下，电源为 Enter；长按约0.8秒：
音量加为 Tab、音量减为 Esc，长按不再产生短按。电源长按仍由硬件处理。
首次上电必须先释放各按键，读取失败清除待处理按键，避免误触发。

键盘与重启策略位于 Elden 设备目录，不改公共 FrontPage 输入逻辑。
此前 DFCI/ConnectAll/BootNext 的兼容改动也移入 EldenFrontPage 副本，
恢复公共 FrontPage.c/FrontPageUi.c。安全设置仍禁用，boot_b 写测试关闭。

只刷 boot_a，从 slot A 启动。请测试无 EFI 时能否进入完整界面、
上下/Tab/Esc/Enter 导航，以及 Exit 页 Restart now 是否立即重启。
若启动失败，拍最后日志，尤其 `ELDEN BDS`。
stock/boot.img 是原厂恢复镜像；本版不刷其他分区。

## 历史镜像 v13

## 当前镜像：延后调度表单显示引擎

9492 确认 v12 仅画出侧栏后，因缺少 FormDisplayEngine 而返回。
本版在 WindowManager 就绪后再次运行 DXE 调度，遵守原始 DEPEX，
确认表单引擎发布才启动 FrontPage；仅定向连接 OSK 软件句柄。
不执行 ConnectAll，USB 电源锁、UFS 保护和禁用写测试均保留。
OSK 初始化不等于已有触摸输入；没有新增触摸或 USB 键盘驱动。
请拍完整界面或 `ELDEN UI v13`、`FormDisplayEngine` 后最后日志。
本版仍需真机验证，位图错误及界面交互尚未确认解决。

## 历史镜像 v12

## 当前镜像：实际启动 FrontPage

9491 已确认 v11 的显示绑定和 WindowManager 成功。本版在 GPT 检查后
从固件卷加载 FrontPage，不再在 UI 启动前主动停止。不放行完整 BDS，
不调用 FrontPage 的 ConnectAll，不处理 BootNext，禁止密码、安全启动
配置及重启到设置的修改回调。DFCI 缺失不再在入口断言。
USB/5V 暂停，boot_b 写测试关闭；没有新增触摸或 USB 键盘支持。
这是界面显示验证版，不代表可以操作全部菜单或启动系统。

只刷 boot_a，从 slot A 启动。请拍界面；若未出现，拍
`ELDEN FRONTPAGE v12 LoadImage`、`StartImage` 后的最后日志。
进入界面后原来的音量加轮询暂不运行，可用设备原有强制重启方式退出；
若应用返回，仍回到音量加重启的诊断等待。保留 stock/boot.img 用于恢复。

## 历史镜像：渲染接管 v11

### 显示 DevicePath + 延后渲染接管

补齐 Elden GOP 的设备路径；UFS 检查后，按 Mu GOP Override 的方式
把原始 GOP 交给 RenderingEngine，只连接现有显示句柄，不调用 ConnectAll。
若未重新发布 GOP 则尝试恢复原始 GOP。控制台非空不代表已有实体按键输入。
此版尚不启动完整 FrontPage/BDS；保留音量加重启。USB/5V 移植暂停，
保留 v9 redriver 禁用和 PMIC 安全限制，boot_b 写测试仍关闭。

只刷 boot_a，从 slot A 启动。拍 `ELDEN RENDER`、`ELDEN CONSOLE`、
`FRONTPAGE WindowManager` 行；若黑屏或重启，报告最后一行。
下列 v8 及更早说明仅为历史记录，不代表本版新增测试。

## 历史镜像：USB CFG v8（配置核对，禁止 redriver I2C open）

只将 UsbConfig 原厂 AE7C 的 I2C 初始化调用替换为读取六个配置字段，
输出 `ELDEN USB CFG v8 words[0..5]=...; I2C OPEN BLOCKED`，然后进入
原有失败路径。不返回假成功，不进入 redriver I2C open/传输，不做
该路径的 FIFO 重载。其它原厂初始化仍存在，因此不是整机零寄存器写入。
Host模式/IOMMU/PMIC供电策略未套用参考补丁。此版不能使用U盘。
只刷boot_a，从slot A启动；不刷qupfw。首测不接扩展坞/U盘/第二电源。
请拍上述单行及最终USB协议状态。这是为了核对配置，不是USB已修复。
指令模型验证三组字段准确输出、不写内存、不调用AB58、走AE94失败分支；
镜像构建检查同时验证实际嵌入的PE与生成器一致。

## 历史源码保护检查

源码已撤回 v7 的零通道 GSI 接管分支，标记为 HOLD。
9472 确认该试验重启；现有 v7 镜像未改写，不要重复刷写。
本次仅源码保护和指令测试，未生成新镜像。

## 历史失败版本：USB-FIFO v7（9472卡死重启，不再使用）

9469 的 AP0..4 context/scratch 均为零，status=0 dma=0 gsi=F。
v7 仅当全部20个字段为零且事件值严格为F时允许继续；紧接着重新检查
GENI/DMA活动，调用已有原厂加载流程。任何非零字段保留reason=6。
不发送GPI STOP/RESET，不改PMIC/VBUS。零值不证明跨EE独占或排除竞争，
这是受限工程试验，不是安全认证或USB Host已完成。
16项指令模型测试通过。只刷boot_a，不刷qupfw；首测不接外设或第二电源。
请拍I2C v7 recovery行及其后错误；0表示FIFO恢复检查通过，不代表USB可用。

## 历史版本：USB-FIFO v6（9469/9470已反馈）

9465确认v5停在recovery=6：status=0 mode=1 dma=0 gsi=F。
本版保留拒绝接管，新增五行`ELDEN GPI AP0`至`AP4`，格式为
`tx=通道context/scratch rx=通道context/scratch`。仅读取BSP对应
GPI1 AP静态/动态掩码内的实例；不向GPI发送STOP/RESET，不改PMIC/VBUS。
SE5在原厂DT中为共享总线、动态GPI分配，不能把SE5误当GPI实例5。
这不是USB Host修复完成版；需要快照判断后续接管条件。

13项指令模型测试通过，包括20个只读地址白名单、五行字段、拒绝路径
无MMIO写入、时钟平衡及ABI保存。不能代替真机验证。
首测不接U盘、扩展坞或第二路电源。只刷boot_a，不刷qupfw。
请拍完整五行GPI日志、I2C v6行和最终USB协议状态。

## 历史版本：USB-FIFO v5（9465/9466到达终点，recovery=6）

9461显示v4 recovery=5。v4把DMA_MODE_EN当成拒绝条件过于保守：它是
模式配置，不是活动状态。v5改查BSP定义的SE_DMA_DEBUG_REG0(+E40)
bit0/1（DMA_TX_ACTIVE/DMA_RX_ACTIVE）。DMA模式但TX/RX均不活动时
允许继续检查；主/从GENI忙、非I2C、GSI事件启用等拒绝条件保留。
不强制关闭GPI通道，不增加PMIC/VBUS写入，不刷qupfw。

单行 `ELDEN I2C v5 recovery=N before: status=... mode=... dma=... gsi=...`
记录重载前的状态快照。1/2时尚未读取硬件，快照的0是占位值。
0=恢复读回通过；5=DMA TX/RX确实活动；6=GSI事件启用，未接管。
其它编号沿用v4。GSI事件启用本身也不是传输活动证明，但仍作为
尚未核实GPI通道所有权时的保守边界，不通过逐次删保护强行接管。
指令仿真13项通过，包括DMA模式开启但空闲、TX/RX忙、未开时钟禁止
MMIO读取、快照及寄存器保存。仍不是USB Host实机验证。
首测不接U盘、扩展坞或第二路电源；只刷boot_a，拍这一行及后续错误。

## 历史版本：USB-FIFO v4（9461/9462到达终点，recovery=5）

仅在原FIFO拒绝分支处理WRAP1 SE5/A94000。先用原厂Clock服务开启资源，
检查主/从命令均不活跃、协议为I2C、DMA模式关闭、GSI事件关闭；不满足
则拒绝，不强制接管活跃GPI。满足时调用原厂固件加载函数体，跳过其
“协议相同直接返回”的捷径，保留RAM/时钟/FIFO接口初始化时序。
原厂内嵌I2C固件与用户qupfw_a.img的完整0x6d1字节payload一致。
不刷qupfw，不修改原厂payload，不增加PMIC/充电/VBUS供电操作。

日志新增 `ELDEN I2C FIFO recovery=N (0=ready)`：
0=FIFO及协议读回通过；1=非目标SE；2=开时钟失败；3=SE忙；4=非I2C；
5=DMA开启；6=GSI事件开启；7=加载函数失败；8=读回不符；9=关时钟失败。
0不等于repeater已通信，也不等于USB主机已可用。电源服务仍未补齐。
GSI事件关闭不是跨执行环境所有权的完整证明；这是有限条件的工程测试。
指令仿真通过不证明硬件时序、电气安全或板上稳定性。

保持UsbPwrCtrl FALSE锁、PMIC驱动排除、boot_b写测试关闭、pre-BDS停止。
首测不接U盘/扩展坞/第二路电源。只刷boot_a，保留1.1.11.263周边固件。
拍恢复状态及后续repeater/USB错误，不要重复刷v3。

## 历史版本：USB-FIFO v3（9458/9460已确认A94000、disable=1）

9454-9456确认Clock/TLMM/I2C协议可用，但QUP打开时在FIFO检查拒绝。
原厂59ec/59f0读取context+0x18所指SE基址+0x64，59f4检查bit0。
这与主线GENI_IF_DISABLE_RO匹配；主线FIFO不可用时走GPI DMA。
不能通过跳过条件恢复FIFO。v3仅在原拒绝分支输出SE基址与已读寄存器值，
不增加MMIO读写、不更改FIFO/GPI模式、不改返回值或电源策略。
请拍 `ELDEN I2C FIFO base=... disable=...` 和终点USB状态。
沿用v2的供电锁及刷写限制，不接U盘/扩展坞/第二路电源。

## 历史版本：USB-FRAMEWORK v2（9454-9456到达终点）

9452/9453确认v1到达终点、UFS分区仍正常，但UsbConfig提前于Clock加载，
并且缺少I2C。v2加入原厂TLMMDxe和I2CDxe，保留其原始PE及DEPEX；
UsbConfig额外等待真实Clock和I2C协议。I2CDxe原始依赖为DALSys、Clock、
TLMM、Timer，不用FDF行顺序代替依赖关系。

预期终点新增Clock/Tlmm/I2c状态，UsbConfig不应再先于这些服务启动。
这不保证repeater硬件访问成功，也尚未解决PMIC/NPA电源资源。
VBUS自动供电锁、PmicDxe排除、boot_b写测试关闭及pre-BDS停止保持不变。
沿用下面v1的刷写前提，不接U盘/扩展坞/第二路电源。拍USB初始化错误和
末尾ELDEN USB状态；勿把协议存在当作USB主机已可用。

## 历史版本：USB-FRAMEWORK v1（9452/9453到达停止点）

加入原厂 UsbConfig/UsbInit/xHCI/UsbBus/UsbMassStorage 驱动和真实 ACPI SDT
服务。保留 UsbConfig 原有五项依赖并补上其入口直接使用的 HALIOMMU 依赖。
这是驱动框架首测，不是完整 USB Host/U盘启动支持：不自动 ConnectAll、
不进入 FrontPage/BDS、不声称设备枚举或长短边切换已经可用。

原厂 UsbPwrCtrl 入口包含潜在 SetVbusEn 动作，因此本版用 FALSE DEPEX
锁住其自动调度，不依赖“PMIC协议当前缺失”作保护。PmicDxe、充电驱动、
PMIC离线候选补丁均不装入镜像；不新增猜测的升压、充电电压或双口VBUS控制。
不能因此保证整个未验证固件零硬件风险；USB初始化仍可能挂起/重启。

首次只检查启动框架，不接U盘、键盘、扩展坞或第二个Type-C电源。
沿用已验证的1.1.11.263环境，从slot A启动，仅刷本包boot.img到boot_a。
请拍最终 ELDEN USB 段（8项协议状态）、UFS BlockIO及Vol+终点；若提前
挂起则拍最后日志。UsbPower=Not Found在本版是预期，不需反复换口尝试。
保持boot_b写测试关闭、GPT修复禁用以及Vol+重启。UFS成功的细粒度
COLD END日志已过滤，错误/断言/不完整日志保留。

## 历史版本：UEFI-PARTITION v2（9451分区句柄验证通过）

9450 已出现118个BlockIO及5条FAT安装日志；v1错误地对总句柄数应用16上限，
导致GPT及FrontPage检查未执行。v2先筛选固定整盘父句柄（最多16），总数上限512，
只对父句柄执行读盘/GPT。无论GPT是否完成，FrontPage服务检查都会运行。
独立汇总SimpleFileSystem句柄，不调用OpenVolume；不宣称此前没有FAT挂载。
新增BlockIO重装触发递归连接的日志，用于确认提前连接来源，不改变连接行为。
仍未启动FrontPage；写测试保持关闭，GPT修复禁用，保留BDS前停止及Vol+重启。

## 历史版本：UEFI-PARTITION v1（9450分区句柄已出现）

9449 六个LUN的112个GPT分区均通过校验。此版接入源代码DiskIoDxe/PartitionDxe，
仅Elden编译禁用GPT自动修复；通过自有主备GPT检查后，仅非递归连接原UFS父句柄。
输出原父句柄连接状态及新增逻辑分区BlockIO数量。不递归连接FAT、不启动系统。
boot_b写测试仍编译关闭。保留Vol+和BDS前停止。
同时输出FrontPage的ConIn/ConOut、HiiFont、FormBrowser2、WindowManager、OSK、
DfciSettings可用性。此版尚不启动FrontPage：其入口会ConnectAll并依赖上述服务，
协议存在也不等于界面可用。预期ELDEN PARTITION END及FRONTPAGE preflight。

## 历史版本：UFS-GPT-INVENTORY v1（9449验证通过）

9448 已实机确认 test=1 restored=1。本版本编译时关闭 boot_b 写入测试，
不会重复写测试块。复用主备 GPT 校验，输出各介质有效分区数量及关键分区边界。
按 ESP 类型 GUID 查找 EFI 系统分区，只读其首块检查55AA与FAT标记；
标记存在不代表文件系统完整，尚不挂载、不读取EFI文件、不启动BDS。
没有ESP也正常报告 esp=0，不创建分区、不格式化、不自动修复GPT。
PartitionDxe 存在自动修复写回路径，故本版不新增该驱动或连接控制器。
预期 ELDEN GPT INVENTORY END partitions=... esp=... boot_b=...。
保留正常 Vol+ 重启与 pre-BDS 停止；刷入 boot_a 并从 A 槽启动。

## 历史版本：UFS-BOOTB-RW v2（9448 测试及恢复成功）

9447 的 v1 在写入前拒绝，未给出具体检查项。静态核对原厂六份 GPT 后发现：
备份表预留4块，有效表项仅1或3块；v1 强制有效表项紧邻末尾头的条件错误。
v2 检查备份数组位于可用空间之后、完整落在备份头之前，允许保留区中的空隙；
CRC、主备一致、重叠和唯一目标保护不变。拒绝日志包含 media/stage/matches，
另输出头位置、表项数及 boot_b 边界。此静态问题尚不能证明是9447唯一原因。
主机测试新增32/96表项、4块保留区成功路径和备份数组越界拒绝路径。

用户明确授权使用 boot_b。必须在 fastboot 中确认 current-slot 为 a，再从 boot_a
启动本包；程序不具备可靠活动槽查询，不能自行验证这一前提。
本版检查运行时主备 GPT 头 CRC、分区表 CRC、主备数组一致、分区边界和重叠，
要求所有句柄中只有一个 boot_b，且起止 LBA 为 321824..346399（与提供的
rawprogram4.xml 一致，块大小4096）。不匹配就拒绝，不盲写固定偏移。
只写 boot_b 最后一个4096字节块（绝对 LBA346399）；不修改 GPT、boot_a 或其他分区。
先读两遍原块一致后保存 RAM 备份，写入与原块每字节不同的模式，FlushBlocks，
读回逐字节比较；无论测试成功还是返回错误，都尝试恢复原块、刷新并读回复核。
成功标准：ELDEN BOOTB END test=1 restored=1。若 restored=0 或中途卡死/重启，
不得启动 B 槽，应使用已知正确镜像恢复 boot_b。RAM 备份不能跨掉电保存。
刷新/读回成功不等于跨掉电持久性验证；驱动调用可能卡住。每次启动都会重跑测试，
完成后应换回非写入测试版本。本包不自动刷机、不切槽、不做 RPMB 密钥操作。
保留 pre-BDS 停止及 Vol+ 重启。此版本允许只读 GPT 解析，不启动分区驱动或 BDS。

## 历史版本：UFS-READONLY v1（9446 六个介质读取成功）

9443 已确认 UFSDxe 返回 Success、错误计数0、BlockIO 句柄8个。
本版仅在驱动成功后，对固定整盘介质各读取 LBA 0 的一个块，输出状态和 CRC32，
不输出扇区内容、不解析分区、不调用 WriteBlocks/FlushBlocks/Reset。
检查块大小和缓冲区对齐，跳过无介质、逻辑分区及可移动设备；首次读取失败即停止，
不重试。同步 ReadBlocks 仍可能在驱动内部卡住，尚无实机读取成功证据。
保留原厂初始化、ShmBridge、继承前级 SMMU 映射及 pre-BDS 停止。
预期 ELDEN READONLY 各介质结果和最终 read/skipped/total 汇总。
只刷本包 boot.img 到 boot_a；不新增 RPMB 密钥操作或媒体写入。

## 历史版本：UFS-SHMBRIDGE v1（9443 驱动成功）

9442：BlockIO 句柄8个，但 UFSDxe 因 RPMB 共享内存协议 Not Found 返回失败。
本版补入原厂 ShmBridgeDxeLA（GUID 41EBB6A6-2C19-4C02-841D-848CE004A282），
其安装协议与 UFSDxe 请求一致。保留失败返回保护，不伪造 RPMB 成功。
LINK-PROFILE 删除逐条 BEGIN/正常数据，只留电源模式摘要和错误证据。
仍停在 BDS 前，不新增分区扫描、RPMB 密钥配置或读写测试。

## 历史版本：UFS-QUIET-RETURN v1

9440 已多次越过 doorbell，最后在 RpmbListener.c:160 的协议查找失败处断言。
本版删除逐请求 UFS 插桩，仅保留 OPEN_INIT/HOST_INIT/IOMMU_INIT、配置摘要和
RPMB_LISTENER_ALLOC 阶段。原始请求执行不变；保留原厂错误日志。
协议缺失时返回真实错误，不断言后解引用无效指针，不伪造 RPMB 初始化成功。
缺失协议 GUID：9c1eb71f-dd6c-4ed5-9f6a-5cc0ca789f16。
共享内存/安全固件依赖尚未接通，不宣称 RPMB 或完整 UFS 启动已成功。

## 历史版本：UFS-SMMU-INHERIT v1

CT1 单独修改仍然卡死。本版在 domain 配置成功后跳过 attach，保留前级固件的
UFS stream 映射，不重新绑定 CB5。配置失败仍返回原错误；CT1、冷初始化保留。
预期标记：ATTACH_SKIPPED_KEEP_FIRMWARE。会执行真实 DMA，仍可能 crashdump。
不新增分区扫描，保留 pre-BDS 停止；仅刷打包的 boot.img 到 boot_a。
参考补丁的来源和限制见包内 new-source/custom-build/elden-ufs-deep-review.md。

## 历史版本：UFS-CT1-FIX v1（实机仍卡死）

根据主线和 BSP ufshcd 的 HCI 版本处理，将原厂 10c9c 的 UTRD CT
构造改为 UFS_STORAGE=1，取代 NOP 的旧类型2及 SCSI 的旧类型0。
适用范围仅此 SM8750/HCI4.0 固件；不是通用旧 HCI 补丁。
恢复原厂 doorbell 写入，保留阶段日志、错误处理与 pre-BDS 停止。
不新增分区扫描或媒体写入，原厂初始化设备命令会真正提交。
尚未真机确认修复 crashdump，仍可能崩溃。若失败不要反复运行。
请记录 CT1_TRIAL_REAL_SUBMIT、REQUEST_SUBMIT/WAIT 和最终 BlockIO。
只刷 boot_a；需要回退可用已验证的 elden-aloha-ufs-doorbell-gate-v2。
详细证据和未解决项见 elden-ufs-deep-review.md。

## 历史版本：UFS-IOMMU-DOMAIN v1

9433 确认 create/config(1)/attach 返回 0。原厂配置 1 清零页表字段并
设置控制字 0A9F00E0；返回成功不代表已构造页表。本版在 attach 成功
后读取已知域对象的 64 字节及首个非空挂接记录的 32 字节，打印 packed
64-bit 值。只读内存对象，不读额外 SMMU MMIO、不修改映射，仍拦截提交。
请拍 DOMAIN_* 和 ATTACH_NODE_* 段。仅刷 boot_a。

## 历史版本：UFS-IOMMU-TRACE v1

9415 MMIO 基址 1D80000、偏移 4058；队列 D5D1B000、命令 D5D1B400。
对齐与 CPU 地址一致不等于设备 DMA 可访问。本版记录原厂 IOMMU
create/config/attach 的参数与返回值，以及队列分配返回值；不新增
任何域操作。仍拦截 doorbell。请拍驱动开头 IOMMU_* 和末尾 GATE_*。
只刷 boot_a。尚未证明 DMA 映射正确，也未恢复提交。

## 历史版本：UFS-DOORBELL-GATE v2

9414 拦截后正常返回，peer GET 成功。v1 的 GATE_BASE 错误地打印
NZCV（日志桩的 x9 临时值），不是 MMIO 基址；寄存器恢复未受影响。
v2 从保存槽读取原始寄存器，并打印已验证结构偏移的队列/描述符地址。
仍拦截所有该处 doorbell 写入，不会尝试修复或跳过 DMA 错误。
请拍 GATE_* 至失败返回。只刷 boot_a。

## 历史版本：UFS-DOORBELL-GATE v1

9412 在 SUBMIT_DOORBELL BEGIN 后 crashdump。此版不执行该 doorbell
写入，打印 x8 掩码、x9 基址、x10 偏移和上下文/请求指针，然后跳至
原有提交函数 -5 失败出口。原厂清理/有限重试仍可能执行，每次提交
均被此门拦截。不是完整只读测试，冷初始化仍会写控制器/PHY。
预期看到 GATE_BLOCKED_NO_DOORBELL 和最终失败返回。不能保证不崩溃。
仅刷 boot_a，拍 GATE 参数及末尾结果。

## 历史版本：UFS-COLD-TRACE v4

9411 最后为 REQUEST_SUBMIT BEGIN，REQUEST_PREP END x0=0。
本版覆盖提交函数的描述符首尾写入、编码、同步调用、屏障及 doorbell
写入前后。保留全部原厂动作，不跳过缓存同步或 IOMMU；尚未修复。
仍可能崩溃，仅刷 boot_a，记录最后 SUBMIT 标记及之后是否重启。

## 历史版本：UFS-COLD-TRACE v3

9409 已通过 QUEUE_RUN、HOST_INIT、CONTEXT_INIT、DEVICE_D864。
OPEN_INIT 尚未返回。反汇编下一段尾调用 c5d0，再调用 f6f4。
本版一次覆盖其请求准备、提交、等待、响应解析与清理调用；不在等待
循环逐次打印，不改变原厂重试及错误处理。仍可能崩溃，不能视为修复。
请记录最后 EXCHANGE / REQUEST / RESPONSE 阶段。仅刷 boot_a。

## 历史版本：UFS-COLD-TRACE v2

9403 已看到 LINK_STARTUP、PEER_CAP 返回 0，随后 crashdump。
本版增加四个队列地址写入、QUEUE_RUN 的前后标记，以及主机初始化
返回、设备初始化调用链的分层标记。保留原厂行为及失败返回，不增加
驱动重试、分区扫描或媒体写入。仍可能 crashdump，不是修复完成版。
QUEUE 标记的 x0 不是写入值；DEVICE 地址标签不代表已确认的函数名。
请录像从 UFS gate 开始到黑屏；拍最后阶段即可，不需要拍全屏历史。
保留 context-v1 恢复包，仅刷本包 boot.img 到 boot_a。

## 历史版本：UFS-COLD-TRACE v1

9400 完整初始化版进入 crashdump；9401 回退版可正常返回。本版仍是
冷初始化试验，不是修复版，仍可能 crashdump。保留 context-v1 恢复包。
新增独立可执行 PE 节，拦截十处已核对的整数参数函数调用，使用原厂
DebugPrint 打印 `ELDEN COLD <阶段> BEGIN/END x0=...`。不改变目标函数、
参数及整数返回值，不删减错误检查，不记录高频轮询的每次迭代。
END x0 是原始寄存器值；void 函数的 x0 不能解释为错误码。
日志改变执行时序，标记只能缩小故障范围，不是无扰动硬件证明。

请录像从 `UFS gate Success` 开始的过程，或拍最后出现的 `ELDEN COLD`。
只有 BEGIN 没有 END 指向该阶段调用内部或相关异步故障，不能直接
断言其中某条寄存器写入出错。不反复运行崩溃版本。
只刷 boot_a 的本包 boot.img，恢复方式及风险同下方完整初始化版。

以下为历史记录。

## 当前版本：UFS-FULL-INIT v1（控制器/PHY 实际重初始化试验）

9399 已验证默认继承标志 00/01/01/00、override=0、tune=0 和错误链
11/3b/55。本版补齐 DT 定义的 GCC MMIO 窗口，并将原厂 PE RVA 19894
的 STRB 来源从 w9 改为 wzr，使 context+0x49 为零，走原厂完整初始化。
不调用 PHY 调参 API，不忽略 peer 错误；NULL-handle 失败返回补丁保留。
这不是只读版本：会执行原厂 GCC 控制器复位、PHY 表写入、LINK_STARTUP
及后续原厂设备初始化。保留原厂内部重试，但外层只启动驱动一次。
不请求分区扫描或媒体写入，不进入 BDS；不修改 PON 或电源键。

风险：这一分支尚未真机验证，可能停在原厂 ASSERT/等待或自动重启；
不要循环刷写。保留 boot_a 原厂备份及已验证的恢复方式。
若未到终点，请拍最后日志；若返回，请拍 UFS CONTEXT、错误和 BlockIO。
预期 flags 第二字节为 00；成功与否以实际状态为准，不能只看 flags。
只刷本包 boot.img 到 boot_a，不刷原始 lenovo-elden.img 或其他分区。

以下均为历史版本说明，以本段为准。

## 当前版本：UFS-CONTEXT v1

本版保留原厂 UFS 单次初始化及失败返回保护，不强制完整初始化、不重试。
新增的诊断仅复制正在运行的 UFSDxe 自身数据，在入口返回后、CoreExit
卸载映像之前打印。通过目标 ImageHandle、映像大小及三处指令校验限制范围；
不解引用 vendor context 内的指针，不新增 UIC 或寄存器写入。

请拍 `ELDEN UFS CONTEXT v1 before unload` 到 `error[7]`，以及最后终点。
flags 的第二字节对应 context+0x49；结合 controller、override、tune 和
错误环判断默认继承条件。它是返回时状态，不是逐分支执行轨迹。
platformLatch 是驱动内部锁存值，不等同于完整 PlatformInfo 平台编号。
若显示 layout mismatch，停止解释这些偏移，不应绕过校验。
保留先前电源配置读取、链路快照、Vol+ 正常重启和 pre-BDS 停止。
只刷本包 boot.img 到 boot_a；不刷原始 lenovo-elden.img，不动其他分区。

以下旧版本段落仅为历史记录，以本段为准。

## 本次DXE分段诊断版

当前HALIOMMU-INIT v1：9384 APPS配置7/7通过。本版首次加入原厂HALIOMMUDxe。
自定义DEPEX等待Env与DALSys；不修改PE、不启用UFSDxe，不调用域创建/配置/附加。
初始化会运行原厂配置解析与内存分配路径，不能再将整版称为只有只读查询。
请拍HALIOMMUDxe加载期间的错误及终点IOMMU lookup；协议先安装再初始化，
所以present=1不能单独证明初始化完成。卡住拍最后BEGIN，用硬件组合键恢复。
保留Vol+、GOP和队列快照，不改电源键；新驱动待真机。
以下为历史说明。

当前IORT-APPS v1：9383根配置3/3通过；UTRLRSR=1，不能假定队列停止。
新增APPS_MMU500_SMMU_APP节点七项配置读取，不访问SMMU寄存器。
预期离线值Type=3/BaseAddress=15000000/Span=80000/Flags=3/Model=3/
NumContextInterrupts=70/NumberofMappings=0（均十六进制）。
请拍IORT-APPS七项及END 7/7，保留Vol+；不启动HALIOMMU或DMA。
卡住拍最后BEGIN，用硬件组合键恢复。新增查询待真机。
以下为历史说明。

当前IORT-CONFIG v1：9382传统队列四项为0，IOMMU协议Not Found。
读取/soc/iort的Signature/Revision/Checksum配置，复用已测试Env +30/+a8，
不调用HALIOMMU的初始化或域接口，不修改SMMU。目标IORT-CONFIG END 3/3。
请拍Find/Read结果及Vol+终点；字段成功不等于IOMMU已经能工作。
卡住拍最后BEGIN，用硬件组合键恢复。新增配置查询待真机。
以下为历史说明。

当前UFS-QUEUE-SNAPSHOT v1：9381验证完整父映射有效，VER=400、HCS=10F、HCE=1。
新增只读UTRLDBR/UTRLRSR/UTMRLDBR/UTMRLRSR四项，检查传统队列快照。
只查原厂UFS所需IOMMU协议是否存在，不调用其方法、不新增HALIOMMU驱动。
请拍八个HCI读值与IOMMU lookup END，然后确认Vol+仍能重启。
这些值不证明DMA可用或MCQ队列空闲；不写门铃、不停止队列、不读写介质。
卡住拍最后BEGIN，用硬件组合键恢复。新增读取待真机。
以下为历史说明。

当前UFS-HCI-READ v2：9380在HWIO SetMemorySpaceAttributes阶段失败，未执行HCI读取。
将单页映射替换为日志要求的01D80000+40000连续NS_DEVICE/GCD设备区域。
只调整映射范围，仍只读取HCI的CAP/VER/HCS/HCE，不读ICE，不写寄存器或介质。
请确认HWIO-MAP v2、两个映射结果、四个HCI读值、Vol+终点。
若仍失败请拍完整错误；若卡在读取BEGIN，用硬件组合键恢复。实际修复待真机。
以下为历史说明。

当前UFS-HCI-READ v1：9379两个映射成功，UFS=01D84000、ICE=01D88000。
新增01D84000单页NS_DEVICE映射；只在HWIO返回此地址时读取CAP/VER/HCS/HCE。
每次读前后打印，全1提前停止；不读ICE、不写寄存器、不复位、不提交存储传输。
本版HWIO日志标注guarded HCI probe enabled，以HCI-READ标记识别测试版本。
请拍四个HCI值及最终Vol+；卡住拍最后BEGIN，用硬件组合键恢复。
新MMIO读取待真机；外层日志不能防止总线访问挂死。
以下为历史说明。

当前UFS-HWIO-MAP v1：9378确认IMEM版本4、raw=8、converted=8。
仅在有效branch8时请求+UFS_MEM_UFS、+UFS_MEM_ICE两个HWIO映射。
严格revision=0x10000，输出初始化为0，接口失败或零地址停止；不解引用返回地址。
映射调用进入原厂DAL后端，不称为无副作用查询；不启动UFS、不做存储读写。
请拍HWIO Map BEGIN/END及地址，目标UFS-HWIO-MAP END 2/2，然后Vol+ armed。
接口卡住用已验证硬件组合键恢复，拍最后BEGIN。新增映射调用待真机。
以下为历史说明。

当前UFS-BOOT-PATH v1：9375确认状态查询9/9，画面可见3至8项value=1。
新增Shared IMEM 0x14680000的32位读取，先校验magic，再版本>=4，最后读+0x50。
按原厂转换表打印converted及branch8（未应用可选override），不修改IMEM。
请拍UFS-BOOT-PATH BEGIN至END；若卡住，拍最后BEGIN并用硬件组合键恢复。
不启动UFS、不写存储，保留Vol+和快速滚屏。新增读取尚待真机验证。
以下为历史说明。

当前为UFS-CLOCK-STATE v1：9374已确认全部9个ID为class 1。
新增原厂Clock +0x20状态查询，每项打印BEGIN/END及value，目标最终9/9。
value=0/1是接口返回状态，0不算调用失败；255表示未写输出，停止后续查询。
重要：原厂查询内可能临时申请/释放资源，不能称为纯读或无硬件副作用。
未显式调用Enable/SetFrequency，不加载UFS、不访问存储，保留GOP、快速滚屏、Vol+。
接口卡住无法由外层计数超时救回，请拍最后BEGIN并用已验证硬件组合键恢复。
以下为历史说明。

当前为UFS-CLOCK-ID v1，基于9373确认的9项配置，新增Clock +8名称查找。
严格检查当前原厂revision=0x1000b，使用64位ID输出；错误、零ID或类别不在1至6时停止查询。
名称查找可能建立内存中的客户端记录，不调用Enable/SetFrequency，不新增Mailbox/SCMI/UFS。
请拍GetId BEGIN/END、id和class，预期最终UFS-CLOCK-ID END 9/9，随后Vol+ armed。
若停在BEGIN，请用硬件组合键恢复并拍最后画面。快速滚屏、GOP和Vol+保持不变。
以下为历史说明。

UFS-CONFIG v1：保留已由用户确认速度可接受的FAST-SCROLL，新增UFS时钟配置读取。
不加载UFSDxe，不启用时钟、不写分区；仍停在BDS前，保留Vol+重启。
按原厂UFSDxe调用读取/soc/ufs0/ufs_clk0至8的clk_name和config六项，新增Env方法尚待真机验证。
名称最多95字节，缓冲清零保留终止符；任一步失败停止本轮查询并回到Vol+循环。
预期UFS-CONFIG END 9/9。请拍9个时钟名称和配置；若卡在BEGIN，拍最后日志，用硬件组合键恢复。
以下是历史说明。

FAST-SCROLL v1：保持精简日志策略，优化每次输出的成本。
滚屏采用支持重叠的CopyMem及SetMem32；普通文字记录脏行，只在日志写入末尾写回对应缓存范围。
取消换行/滚屏内部重复刷新，不再主动失效整个framebuffer缓存。初始化清屏也改批量填充。
不改framebuffer地址、MMU属性、字体、GOP或Vol+。仍可能需要整屏搬运，实际提速待真机。
请观察FAST-SCROLL v1、ENV-CONFIG END、Vol+ armed；检查滚动是否更快、有无残影/文字损坏，Vol+是否有效。
本版编译及主机测试不等于缓存一致性已通过真机验证；出问题可回刷上一版boot_a。

以下为历史说明。COMPACT-LOG v1基于9372通过的ENV-CONFIG版本，只精简屏幕日志。
过滤单行普通进度码、驱动加载地址、内存分配表和驱动Success结束行；保留驱动BEGIN、错误、断言与测试输出。
非完整单行不丢弃；Critical输出不走过滤器。屏幕模式没有开启内存日志，隐藏的消息不是另存到了日志文件。
驱动集合、硬件初始化、GOP测试、Env查询和Vol+保持不变；本轮不新增Mailbox/SCMI/UFS。
请确认日志滚动是否明显减少、仍到COMPACT-LOG v1、ENV-CONFIG END与Vol+ armed。
此版本只有显示输出变化，不代表UFS移植进度增加。以下为历史说明。

ENV-CONFIG v1基于9371已验证的CLOCK-NPA版本，仅增加终点Env配置查询。
不新增Mailbox/SCMI/UFS，不改电源键，仍在BDS前停止并保留Vol+普通重启。
观察ELDEN ENV-CONFIG v1及Find/Count/Children/Parse的BEGIN和END；raw=0表示配置接口调用成功。
预期原厂有3个子节点，但当前启动链版本可能不同，不将3写死；只接受1至8项。
每项打印tx/rx及其余80字节缓冲区的原始64位值，用于和离线配置对照。
这不是存储测试，不读写分区；新增查询不调用Mailbox寄存器映射、初始化或SCMI发送。
接口按原厂Mailbox调用点还原，尚待真机验证；如停在BEGIN而无END，请拍照并使用硬件组合键恢复。
查询顺利完成后应出现ENV-CONFIG END和Vol+ armed；只刷boot_a，配套仍为1.1.11.263。

以下版本说明仅为历史背景。CLOCK-NPA v1：
在9369/9370确认PwrUtils缺失刷屏消失后，只增加原厂NpaDxe。
NPA调度等待DALSys及其日志协议，Clock等待NPA；不修改PE、不加SCMI/Mailbox/UFS/Pmic/Buttons。
预期NPA/Clock返回Success，终点NPA Success、RGBmatch=1、Vol+ armed。
SCMI报错仍可能保留；NPA资源请求开始运行后也可能出现新的缺失资源或断言，尚未真机验证。
如卡在NpaDxe或Clock，拍最后日志，用已验证硬件组合键返回bootloader恢复旧包。

当前为CLOCK-PWRUTILS v1，以下旧版说明仅为历史背景。
在9365–9367的Clock单模块结果上，仅新增原厂PwrUtilsDxe，并让Clock依赖其协议。
不新增NPA/SCMI/UFS/Pmic/Buttons；NPA与SCMI报错仍可能出现，不代表本次增量无效。
预期PwrUtils与Clock入口返回Success，终点PwrUtils查询Success，pwr_utils缺失刷屏消失，RGBmatch=1且Vol+ armed。
尚未真机验证，仍可能出现初始化故障；只在此前配套环境刷boot_a，保留硬件组合键恢复方式。

本包为CLOCK-ONLY v1：只新增原厂ClockDxe，保留其DALSys/ChipInfo/CmdDb依赖，补上构造器需要的EnvServices依赖。
Clock初始化可能操作硬件，尚未真机验证，可能卡死或重启；不是纯协议查询版。
不新增UFS/PwrUtils/Pmic/Buttons，不进入BDS、不主动读写分区。保留GOP和Vol+普通重启。
预期终点显示CLOCK-ONLY v1，HWIO/Clock/EnvServices三项Success、GOP RGBmatch=1，最后Vol+ armed。
若停在ClockDxe BEGIN附近或出现断言，拍最后日志并使用已验证的组合键回bootloader，恢复旧boot。
下文为累计诊断背景，以本段的Clock单模块变化为准。

9362的RGBmatch=0已找到VideoToVideo倒序复制偏移错误：末行应为(Height-1)*stride，而非Height*stride。
本包修复共享FrameBufferBltLib的两行偏移；实际函数5005项主机复制测试旧版2345项失败，修复后0失败。
终点保持同一往返测试以便对照，预期write/copy/read均Success且RGBmatch=1。真机结果仍待确认。

9361已经真机确认标准GOP持续可见、三次Fill成功且颜色顺序红绿蓝正确。
本次新增终点API测试：QueryMode(0)、非法QueryMode/SetMode(1)、2x2像素写入→屏幕复制→读回并逐RGB比较、越界Fill拒绝。
预期roundtrip三项Success及RGBmatch=1，三个rejected=1；这些拒绝测试的Invalid Parameter/Unsupported是预期结果。
不调用SetMode(0)以免清屏；不在像素往返事务中打印，避免日志滚屏干扰比较。
只改终点诊断，不新增存储/BDS功能，保留原GOP驱动及Vol+。本包尚未真机验证。

本包针对9360的“立即可查、稍后消失”：仅对Elden从FV排除GopOverrideDxe。
该模块会安装私有override协议后卸载标准GOP；本阶段不接管到完整MU渲染链，直接保留EldenGop。
预期D7 GOP Success handles=1，late两项Success/match=1及RGB色块。保留全部诊断、Vol+和BDS前停止。

9359已显示stage6 Success，但D7仍无GOP。本包改用单协议InstallProtocolInterface，
并保存安装后的HandleProtocol/LocateProtocol返回值，终点再次按句柄和GUID读回。
请拍SUMMARY、immediate saved、late HandleProtocol/LocateProtocol和D7。
这是一项等价安装路径调整与诊断，不代表已证实原变参接口存在ABI故障；不改绘图参数。

本包是9358 GOP失败的诊断增量，不宣称已修复根因。终点新增ELDEN GOP SUMMARY。
stage：1入口，2查已有GOP，3查询绘图配置大小，4分配配置，5配置绘图库，6安装GOP。
status/raw保存该阶段实际返回值；stage3的Buffer Too Small本身是正常的大小查询结果。
摘要保存在BootServicesData配置表中，即使驱动入口返回错误被卸载仍可读取，不使用已卸载驱动的静态内存。
若摘要unavailable，需找更早的EldenGop加载/入口/trace table日志；不能直接认定是显存参数错误。

本次接入EldenGop单模式1904x3040，复用FC800000 framebuffer，步长7616。
初始化不清屏、不重新初始化面板；调用SetMode(0)按GOP语义会清屏。
D7预期GOP Success，终点通过GOP画三个80x80色块，从左到右应为红、绿、蓝。
后续日志滚屏可能使色块上移。BGRA颜色顺序须真机确认。
图形组件可能因GOP出现而继续运行，若日志停止/被覆盖请录像；仍保留Vol+和BDS前停止。
不新增存储驱动，不代表标准控制台/BDS或显示完整功能已验证。

本次新增D7只读协议清点：GOP、BlockIO、SimpleFS的状态与句柄数。
不连接控制器、不打开卷、不读写磁盘。SimpleFS可能来自固件FV，不能直接当作UFS/FAT磁盘证据。
上一版9356照片已验证D6 services: Success，用户确认Vol+普通重启有效；本版保留相同按键路径。

本包新增源码ResetSystemRuntimeDxe（PSCI SMC）及原厂SPMIDxe，仍在BDS之前停止。
Vol+：到测试终点后先松开至少0.5秒，看到armed，再按住约0.5秒尝试普通重启，**不是进入bootloader**。
仅读取PM8550 GPIO6状态，不加载原厂ButtonsDxe/PmicDxe，不修改PON或电源键配置。
SPMI缺失、版本不匹配、读失败或Reset服务缺失会禁用快捷键；本次增量镜像仍需真机回归。
若没有armed或再次卡住，继续用原来已测试的硬件组合键恢复。

本包保留Elden原厂RNG GetInfo拒绝NULL查询的兼容修复，限定ELDEN_DXE_TRACE构建。
预期出现 `ELDEN RNG GetInfo buffer retry: Success, size=16`，继续观察变量驱动构造器。
不改变算法选择策略、不使用假随机数。

本包新接入原厂CmdDbDxe与RNGDxe，保持原厂PE/DEPEX不变。
RNG等待CmdDb协议1C34F691-D33D-4E14-B8F8-32C6C029E95B。
EFI RNG协议是变量服务及已有BaseCryptoDriver的共同依赖，可能释放后续驱动调度。
请录像观察CmdDbDxe、RngDxe、BaseCryptoDriver、VariableRuntimeDxe的BEGIN/END及错误。
协议成功安装不等于随机数质量或安全认证验证通过；这是开发测试固件。
不新增Clock/PMIC/存储驱动，仍在BDS Entry前主动停止。

本包补入已在DSC编译但遗漏于FV的EmbeddedMonotonicCounter。
它仅提供本次启动内的内存计数，不持久保存，也不是安全防回滚计数器。
预期D6的Monotonic Counter缺失项消失，其他缺失项仍可能存在，仍主动停在BDS前。
未新增存储、BDS、Reset或PMIC驱动。变量/加密协议依赖仍在核对，不能视为完整基础服务已完成。

本包增加ULog的DALSys协议DEPEX：AE37B942-457F-4C91-A196-D9669FD347A3。
不修改原厂ULog/DALSys二进制，不强行绕过DALSys自己的SMEM依赖。
观察DALSys（8E9BD160-B184-11DF-94E2-0800200C9A66）是否先返回成功，再观察ULog。
如果DALSys没有安装协议，ULog应等待而不是再次进入其Not Found断言。
这不保证后续其他驱动或自动重启问题已经解决。

本包为有序日志版：PrePi C入口清屏、清零光标一次，后续源码模块共享有效光标，
满屏按32位像素向上滚动一行（3倍字体36像素），底部保留最新日志。
不改变看门狗、超时、电源键或原厂二进制驱动；不能保证原厂独立输出不覆盖本控制台。
请从首条日志开始录像，重点拍黑屏前最后的DRIVER BEGIN/END。

本包加入PHIT边界修复：HobConstructor总范围改用DXE Free长度，
顶端从0xD7C00000改为0xD7BC0000；栈和FD物理地址不变。
启动打印 `ELDEN PHIT arena=... (stack excluded)`。
修复针对之前G4处End>Start断言；尚需真机验证能否越过该点。

本版开启ELDEN_DXE_TRACE，尚未真机验证。D0是内存服务初始化；G1～G4是其内部
的memory-type HOB、PHIT资源查找、候选内存范围、首次内存描述符步骤。
之前警告实际在CoreInitializeMemoryServices中，不是CoreInitializeGcdServices。
D1为GCD服务，D2为系统表分配，D3为库构造器，D4为调度初始化，D5为驱动调度，
D6打印必需协议检查状态。每个驱动入口也打印BEGIN/END及GUID、返回值。

若走到D6，本诊断版将打印 `ELDEN TEST COMPLETE: intentional STOP before BDS`，
然后有意CpuDeadLoop停住，不进入BDS，不搜索EFI文件。这个结束标记不是所有协议正常的保证，
须看D6状态；也不是实时心跳。若没有结束标记，最后一个BEGIN没有对应END可帮助定位。
不能保证CPU异常或日志路径损坏时还能打印，因此无标记不等于正常结束。
仍不修改存储驱动、电源键或PMIC。包内mu-basecore.patch保存子模块诊断差异。

本版真正加载Aloha FD，不是停住的独立棋盘格探针。
仅适用已解锁、当前a槽、原厂1.1.11.263配套vendor_boot/init_boot/dtbo/vbmeta完整的Elden。
不混用1.5.10.259，不刷或擦其他分区。stock/boot.img是已核实1.1.11.263原厂boot副本。
屏幕库按1904×3040/32bpp/stride7616使用Display Reserved，沿用已有扫描输出，
不初始化面板、不控制背光/电源键/PMIC，不保证USB或GRUB可用。

## 观察方式

请从启动前录像。代码原生坐标(64,160)的32×16小白块为S0：
已到FD的PrePi汇编，完成关闭MMU/cache调用，但尚未设置向量和栈。
文字使用上游5×12字体的3倍整数放大：字形15×36，含字间距18×36。
1904像素宽约可显示105列，较原版减少；长行会自动换行。横向拿平板时需旋转照片阅读。

- S1：到达PrePi C入口。
- S2：PrePi串口日志初始化路径，尚未建立HOB/MMU。
- S3：MemoryPeim返回；同时看返回状态，不能仅凭有S3认为成功。
- S4：FV解压返回；同时看状态。
- S5：即将调用LoadDxeCoreFromFv，不代表DXE已经成功执行。

日志可能覆盖白块或被后续日志覆盖，故建议录像。没有白块/文字不能单独断定具体失败点。
原厂二进制驱动内的DEBUG不会因重编源码串口库而自动转到屏幕。
该包只验证了构建和镜像自哈希，AVB为NONE，不是联想签名。

## 刷入与恢复

在本包目录，逐条执行并核对槽位：

```bash
sha256sum -c SHA256SUMS
fastboot getvar current-slot
# 仅当为a且满足以上版本和原厂配套前提：
fastboot flash boot_a boot.img
fastboot reboot
```

测试后沿用已实测组合键：音量减＋电源长按重启，立即改音量加＋电源进入bootloader。
此方法不保证所有故障都可恢复。恢复只刷同包原厂boot：

```bash
fastboot flash boot_a stock/boot.img
fastboot reboot
```
# HALIOMMU GET-TABLE v1 (9385 follow-up)

Repeat saved StartImage returned/status at the end, then conditionally retrieve
the stock interface table. Expected: returned=1 status=Success, GET-TABLE END
present=1. A missing GET-TABLE line with failed initialization is an intentional
guard, not a hang. No domain calls, storage I/O or DMA takeover. Preserve Vol+.
Package: elden-aloha-iommu-table-v1. Hardware validation pending.
# DOMAIN-ALLOC v1 (9386 follow-up)

9386 confirms successful StartImage and interface retrieval. New bounded test:
CREATE END raw=0 present=1, DESTROY END raw=0 for a fresh software domain.
No domain configuration/attachment or storage access. Preserve Vol+ and BDS
stop. Package elden-aloha-iommu-domain-alloc-v1; hardware validation pending.
# Combined software stage v1

Supersedes the unflashed domain-allocation-only package. Tests create, both
configuration modes, invalid-mode rejection/unchanged object, destroy and
five SID mapping fields in one boot. Expected checks=4/4 and match=5/5.
No domain attach or UFS start. Keep Vol+; photograph errors or terminal.
Package: elden-aloha-iommu-software-stage-v1. Hardware validation pending.
# UFS-START v1 (9387 passed)

Stock UFSDxe is now included but held behind terminal preflight DEPEX.
Expected: UFS-START preflight 1/1/1, gate Success, driver BEGIN/END,
dispatch END returned=1 driver=Success, BlockIO handle inventory.
BlockIO count is enumeration evidence, not disk read or boot proof.
Hardware initialization can hang/reset. Vol+ only works after return;
capture last BEGIN on failure and use previously tested hardware keys.
No BDS or explicit partition read/write. Package elden-aloha-ufs-start-v1.
# UFS-FAILRETURN v1

9388 reached stock NULL-open ASSERT. This diagnostic preserves failure by
returning EFI_DEVICE_ERROR on that exact branch, then prints register state.
Original imported PE remains unchanged; generated PE patch is hash-pinned.
Photograph driver status and post HCS/HCE/UICCMD/ARG1..3. Vol+ if loop reached.
Reboot may still happen; no assertion-independent reset fix claimed.
Package elden-aloha-ufs-failreturn-v1.
# UIC-COMPARE v1 (9392 follow-up)

9392 returned Device Error without reboot. New bounded diagnostic prints one
local GET and three spaced peer GETs after failure. Expected output is raw
result/data/HCS, not necessarily success. Result a means DME_FAILURE.
No driver retry or block I/O. Timeout aborts remaining commands. Preserve
fail-return patch and Vol+. Package elden-aloha-uic-compare-v1; pending hardware.
# LINK-PROFILE v1

Capture BEFORE and AFTER groups (9 requests each) around UFS initialization.
Includes power mode, gear, lanes, HS series and local/peer attributes.
valid=0 means data is not an attribute value. No SET commands. A timed-out
BEFORE profile prevents UFS start. Preserve fail-return and Vol+.
Package elden-aloha-link-profile-v1. Pending hardware; photograph both groups.

# AUTO trial v1

Package elden-aloha-auto-trial-v1. After UFSDxe fails, test FASTAUTO44 and
restore FAST11, with bounded completion/status checks and local/peer profiles.
Capture AUTO SET, LINK AUTO, LINK RESTORED and AUTO END lines. Any timeout
stops further commands; use tested reboot keys. No driver retry or block I/O.
Power-mode changes are experimental; successful build is not hardware proof.

# UIC evidence v1 (9395)

AUTO experiment disabled after SET returned a without power completion.
Package elden-aloha-uic-evidence-v1 captures EVIDENCE BEFORE and AFTER:
AHIT/IS/IE, command arguments, PA/DL/N/T/DME error registers, CAP/VER/MEMCFG.
Photograph EVIDENCE groups, preferably video through the driver start.
Error register reads may clear latches. No new power-mode writes, controller
reset, driver retry or partition scan. Existing recovery keys retained.

# Topology read v1 / 9396

AHIT0 and zero layer error latches observed. Package elden-aloha-topology-read-v1
adds TOPO BEFORE/AFTER for TCSR_UFS_CLKREF, dedicated UFS_RESET_CFG and OUT.
No register writes added. Capture both TOPO groups and final driver result.

# Rail configuration v1

Package elden-aloha-rail-config-v1: capture RAIL-CONFIG and six RAIL groups
before driver start. Reads active Env supply names and on-voltage/en/mode
properties; not a PMIC voltage measurement. No additional hardware writes.
# UFS-SMMU-INHERIT v1 — candidate, hardware unverified

This supersedes the CT1-only trial, which still crashed at real submission.
Keep boot firmware stream mapping by skipping UFSDxe attach only after successful
fresh-domain configuration. Preserve the original configure failure path.
Reference pattern: GUF296 8650-test commit 4234f922, available common Sm8550
patched UFSDxe; the checkout's missing SM8650 directory prevents claiming this
is the actual validated Y700 binary. No reference binary is transplanted.

Expected: ATTACH_SKIPPED_KEEP_FIRMWARE, then real submit. CT1 and cold init remain.
This can still crash: inherited DMA mapping has not been proved usable.
No new partition scan; intentional pre-BDS stop retained. Recovery is the known
elden-aloha-ufs-doorbell-gate-v2 or the packaged stock boot backup, using only boot_a.
See new-source/custom-build/elden-ufs-deep-review.md in the package for evidence.
