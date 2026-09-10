# Elden UFSDxe 入口审查（2026-09-12，未完成）

## 最新：9384与HALIOMMU驱动试运行

9384 APPS7/7与离线表一致。加入HALIOMMUDxe原PE，DEPEX等待Env与DALSys。
5070至5e40初始化读取IORT头、枚举子节点、解析Type1/3并生成内部记录；
主路径直接调用包括内存分配2f68/释放2fa8和Env间接接口。
入口1548安装协议后再调用5070；失败会返回NOT_READY，但协议可能仍存在。
本轮不调用取域函数后的create/config/attach，不加载UFSDxe，保留硬件恢复。
此为原厂模块运行测试，不宣称已证明其所有库构造器及间接调用无硬件副作用。

## 最新：9383与APPS IORT

9383根字段3/3通过；UTRLRSR=1，其他三项0。此前“全部停止”不能作为长期假设。
HALIOMMU 5550取子节点数、55d8取16字节节点列表，564c读Type；
Type3在5698经Env+58解析56字节结构，然后保存到内部IORT记录。
本轮只复用已验证Env数组读取APPS节点七个关键字段，核对基址和范围。
不调用未完成审查的初始化/解析方法，不访问SMMU寄存器，不接管队列。

## 最新：9382与IORT配置

9382传统队列58/60/78/80均0，IOMMU Not Found，仍不可推断MCQ或DMA可接管。
HALIOMMU 5070查b098的Env GUID，50dc经+30查b198字符串/soc/iort，
5144/5194/51e4经+a0读取Signature/Revision/Checksum。
本轮不调用尚未验证的+a0标量接口，改用已验证+a8数组读取每项4字节；
读取错误原样停止，不伪造缺省成功。字段位宽与离线FDT对照需保留。
未加入HALIOMMU，其后续初始化及域附加还需审查。

## 最新：9381成功与IOMMU边界

9381：CAP=D5C7031F、VER=400、HCS=10F、HCE=1，HWIO2/2，Vol+ armed。
本地ufshci.h的HCS低四位为设备存在/transfer ready/task ready/UIC ready。
这些值不证明DMA域可接管。UFSDxe b350->1b080查20258处GUID
54b6d3b4-5d33-4f91-8600-6c41d5deb19a，经协议+8取函数表，
表+0创建域、+40配置、+10附加UFS_MEM，错误字符串可交叉确认。
HALIOMMU入口1548先安装协议，再5070初始化；因此协议存在也不等于初始化成功。
本轮不加入该模块。仅LocateProtocol并读取传统队列58/60/78/80快照。
不写DMA/门铃、不复位；也不将传统队列门铃为零等同于所有MCQ活动停止。

## 最新：9380单页映射回归与父区域修正

9380：HWIO报告PhysicalAddr=1D80000,size=40000,SetMemorySpaceAttributes失败；
Map返回Protocol Error/address0，HCI四次读均未执行。Vol+终点正常。
单页AddDev区域把DAL所需父范围拆分；Gcd.c CoreConvertSpace对涉及各项检查能力，
不支持请求属性则EFI_UNSUPPORTED。照片未打印底层EFI状态，故不能独断具体失败返回码。
v2用单一01D80000+40000 NS_DEVICE/AddDev区域替换单页，结束恰接01DC0000 CRYPTO区域。
不跳过属性设置、不伪造HWIO成功。仍仅在准确HCI地址上读取四项，不读ICE，不写寄存器。
主机测试固定父范围和属性，并检查整体无重叠；真正HWIO恢复待设备验证。

## 最新：9379与HCI寄存器读取

9379 HWIO 2/2，UFS=01D84000、ICE=01D88000，Vol+ armed。
当前PlatformMemoryMap未列UFS区域，不能把HWIO返回成功当作页表覆盖证明。
新增01D84000大小1000的NS_DEVICE区域，仅用于首个寄存器页。
本地SM8750 BSP msm-kernel/include/ufs/ufshci.h定义CAP=0、VER=8、HCS=30、HCE=34。
调用在成功HWIO映射且地址严格匹配后，四次volatile32读取；全1提前停止。
不读ICE及错误寄存器，不写寄存器，不启动UFSDxe或DMA，不读取介质数据。
新增MMIO仍可能总线异常/挂死；每项BEGIN/END定位，硬件组合键恢复。

## 最新：9378与HWIO映射测试

9378：magic=C1F8DB40、version=4、raw=8、converted=8，Vol+ armed。
UFSDxe 4158的非零分支于41a0/422c请求+UFS_MEM_UFS/+UFS_MEM_ICE。
参数x0=This、x1=名称、x2=8字节输出；两者成功后42a0调用b350。
该分支跳过41d4至4204的Clock/Env初始化；不推断以后所有函数均不需要这些服务。
HWIODxe 152c安装协议表60d0，revision10000，+8=1428。
1428替换This为6260缓存DAL对象，调用对象方法表+48；保持名称和输出参数。
DAL返回0映射EFI_SUCCESS，其他返回EFI_PROTOCOL_ERROR。后端副作用未完整证明。
因此本轮只做两个映射请求，失败或零地址停止，不读取返回地址，不启动UFS。
不伪造映射，也不强制原厂实际分支；日志IMEM转换仍标注before override。

## 最新：9375通过，UFS-BOOT-PATH v1

9375确认Clock State 9/9有效输出；照片中3至8项为1，未拍到0至2项。
SharedIMEMBaseAddr=14680000，当前内存图覆盖为NS_DEVICE。
UFSDxe 6898读magic，68ac读版本，68bc在magic正确且版本>=4后读+50。
67f4接受Raw<=28且掩码18001dff对应位为1，1c92c表转换后67ac比较8。
本轮复现字段读取和转换，不执行原UFSDxe，不写IMEM或控制器。
打印标注before override：当前配置表没有OverRideSharedImemBootCfg，
但静态缺失不证明运行时服务一定无覆盖值。分支结果不证明存储可读。

对象：`Platforms/PakalaPkg/Device/lenovo-elden/Binaries/QcomPkg/Drivers/UFSDxe/UFSDxe.efi`

SHA256：`dac620aca276e735361d8b747fe0218b6125eae004116391270f32a72e6ce750`
PE32+ AARCH64，ImageBase=0，EntryPoint=0x1000，SizeOfImage=0x25000。
以下均为该二进制RVA，不是加载后的绝对地址。用llvm-objdump -d --start-address/--stop-address复查。

## 已追踪路径

1. 0x1000是包装入口，经0x10bc跳0x1148。
2. 0x118c调用0x1230构造器列表，随后0x1204调用0x151c，再跳真实主体0x40dc。
3. 构造器列表调用0x89a4、0x8bec、0x8d20、0x8fa0、0x9508、0x9548、0x96dc、0x97f4、0x99c0、0x99c4。
   这些调用不能全部视为无副作用；尚未逐个核实。
4. 0x99c4是定时器相关构造器：0x99cc→0x6fc8返回1；0x99d4→0x18164→0x1b244检查频率。
   0x1d66e字符串为ArmArchTimerLib.c，0x1d680为ArmGenericTimerGetTimerFreq () != 0。
   失败路径0x9a14报告断言后在0x9a2c自循环；未绕过这些检查。
5. 真实主体0x40dc在0x4140调用0x8ac8，传入字符串UFS_DRVR（RVA0x1c04d）。
6. 0x4144→0x143d4固定返回1；接着0x4150→0x67ac决定分支。
   0x67ac调用0x684c获取一个值并与8比较，返回布尔值。该值具体语义尚未完整确认。
7. 若该布尔值不为0，进入0x415c分支，在0x417c通过BootServices偏移0x140调用LocateProtocol。
   GUID位于0x201f8：9e61de7c-9927-4f28-9f3d-320705d81801。
   对应失败字符串0x1c089明确写Failed to locate HWIO Protocol。
8. 协议找到后0x41a0调用接口偏移8，请求+UFS_MEM_UFS（0x1c0b8）；
   0x422c再请求+UFS_MEM_ICE（0x1c103）。未据此推断映射已存在或接口调用完全只读。
9. 另一分支0x41d4调用0xaf28及0xaf2c两次，参数分别(0,0)、(0,1)、(0,0)。
   0xaf28跳0x18820，0xaf2c跳0x19c6c，尚需审查其硬件影响。
   随后以UfsSmmuConfigForOtherBootDev（0x1c056）调用0x7088。
10. 后续0x4290/0x42a0调用0xb350→0x1b080；再往后的枚举、DMA、写保护及控制器初始化未完成审查。

## 当前结论和限制

- 原DEPEX=TRUE并不表示无构造器或HWIO依赖。
- 存在硬件初始化前的分支判定，不能直接强制其中一条分支或照搬SM8550补丁。
- HWIO依赖由调用点与错误字符串交叉确认，不是仅从二进制GUID命中推断。
- 不能宣称已证明初始化无写盘、写保护、电源或SMMU副作用。
- 尚未修改UFSDxe二进制/DEPEX/FDF，尚未启用Clock/PwrUtils/UFS，无新刷机包。

下一步：解析0x684c所用配置项及0x7088协议来源，核对实际分支；追0x18820、0x19c6c、0x1b080；
审完剩余构造器再决定最小服务集合。继续保留GOP/Vol+及BDS前停止点。

## 第二轮补充

- 0x684c中0x6878传入字符串SharedIMEMBaseAddr（0x1c8fd）；0x68dc传入OverRideSharedImemBootCfg（0x1c910）。
  当前设备PlatformConfigurationMapLib声明SharedIMEMBaseAddr=0x14680000；尚未读取真机IMEM确认其内容。
- 该函数检查返回地址处magic=0xc1f8db40和版本>=4，读取偏移0x50，经0x67f4转换；可选override值<=11时覆盖结果。
  0x67ac最终比较转换结果是否8。未凭数字8直接命名启动介质。
- 配置查询0x7088通过缓存接口偏移0x18尾调用；接口获取0x6fd8从0x15e20返回对象的偏移0x10调用，
  使用字符串UEFI Config Lib（0x1ca4b）。配置服务来源仍需完整核对，未假定其可直接替换。
- 0x18820分支使用0x201e8 GUID 241afae6-885f-4f6c-a7ea-c28eab79c3e5；
  0x19c6c在0x19cd4准备通过LocateProtocol偏移0x140查询0x20278 GUID 0389b776-625f-11eb-83be-c741a913de34。
  暂以RVA标签记录，不臆造协议结构或调用接口。
- 新增终点UFS preflight只LocateProtocol查询以上两项及HWIO，不启动原UFSDxe，不访问IMEM、UFS或未知接口。
  协议存在也不代表硬件初始化/配置正确；协议缺失也不代表所有分支必然会使用它。

## 第三轮：9364真机结果与协议提供者

- 9364确认HWIO存在、201e8协议缺失、20278协议存在；GOP往返RGBmatch=1，Vol+仍armed。
- 201e8已确认是EFI_CLOCK_PROTOCOL_GUID：本树Platforms/AndromedaPkg/Include/Protocol/EFIClock.h及
  AndromedaPkg.dec均定义241afae6-885f-4f6c-a7ea-c28eab79c3e5。
  原厂ClockDxe在RVA0x1e30..0x1e4c以GUID 0x28068、接口0x28148调用
  BootServices+0x148（InstallMultipleProtocolInterfaces），交叉确认提供者，不只是字符串命中。
  现有EFIClock.h对大部分方法使用Unknown占位，不能直接拿它调用未定义的方法。
- 原厂EnvDxeEnhanced在0x2c88..0x2ca8安装GUID 0x101c8（0389b776-625f-11eb-83be-c741a913de34），
  接口位于0x10500。故20278的提供者已定位为EnvDxeEnhanced，协议正式名称和完整ABI仍未确认。
- UFSDxe在0x188a8确实调用LocateProtocol查询Clock；0x188ec..0x188fc随后对
  0x0017707c作32位读-置bit14-写。尚未命名该寄存器，也未证明本机执行这条分支。
  后续0x18970/0x189a4分别调用Clock接口+8/+0x10；不能把该函数当成纯查询。
- UFSDxe在0x19cf4查询Env提供的协议，0x19d34调用其+0x30方法，后续0x19de8调用+0x90方法。
  不以存在性结果推断这些方法已可安全调用。
- ClockDxe安装Clock协议前在0x1db4/0x1dc0/0x1dc4/0x1dc8依次调用
  0x50e8、0x392c(0)、0x4060、0x8d38，并注册事件/协议通知。
  0x392c通过0x39ec查询另一协议（GUID在0x280d8），调用其+8方法；
  0x8d38条件调用0xb164。剩余初始化及构造器必须继续核对，不能认定只安装协议。

本轮仅更新逆向记录，未启用Clock/UFS，未生成新镜像。下一步审查Clock初始化及配置服务，
再决定Clock单模块增量测试；保留现有BDS前停止及Vol+，不改电源键或分区。

## 第四轮：Clock单模块工程测试

Clock构造器0x14b8调用0x3d58，后者在0x3d90 LocateProtocol查询0x280e8的Env服务，缓存于0x3f6e0。
查询失败会清空缓存但函数返回成功；随后初始化0x4060调用0x11394→0x3ddc，
缓存缺失时返回-1013，调用者在0x4154/0x418c有无条件自循环。
因此仅保留原DEPEX的DALSys/ChipInfo/CmdDb不能保证构造器顺序；加Env依赖具有具体调用链依据。
0x39ec查询0x280d8已解码为DALSys；0x113d8查询0x280f8已解码为CmdDb。
0x8d38条件调用0xb164，其字符串包含Clock BIST Log/BISTDependencies/ExcludedPowerDomains；
不能由这些字符串认定真机执行BIST，也未禁用或修改该分支。

本轮进入有风险的增量运行验证，不宣称静态审查已覆盖Clock全部硬件操作：
仅在Elden的DXE.inc加入原Clock PE，FFS使用四项AND依赖；不改原厂二进制、不强制APRIORI。
不加入UFS/PwrUtils/Pmic/Buttons，保留既有显示/恢复与BDS前停止点。
输出检查校验实际生成的四项DEPEX与Clock PE原始字节，确认最终FV含Clock、不含UFSDxe。
Docker DEBUG及GOP复制/边界/Vol+主机测试通过；真机Clock初始化是否返回仍待测试。
包：porting-artifacts/elden-aloha-clock-only-v1（相对Elden根目录），
boot SHA256 da166c3884918f1450bc27ba303f198d1e82875aa563e66bd5bccfda0b891a27。

## 第五轮：9365–9367及PwrUtils增量

真机Clock最终返回Success并安装协议，但出现NPA/PwrUtils/SCMI缺失；GOP往返及终点仍通过。
这不是完整Clock初始化成功，也不是已验证CPU调频或UFS。

- Clock RVA0x11464查询0x28108 GUID 3c50e0f7-7d40-4a86-9067-11a2372bfc3d，
  失败在0x11480返回0，不是向上传播EFI_NOT_FOUND。成功调用接口+0x18。
- PwrUtilsDxe在0x141c→0x21a0先初始化，0x1454安装0x6058处同GUID，接口0x60a8。
  接口+0x18=0x14f0→0x2524→0x2478；该路径做名称/等级表查询，不是直接写PMIC。
  0x21a0和0x2314遍历24项，通过0x4020/0x40a0访问CmdDb辅助数据，分配内存并建表。
  0x4020查询GUID 0x6088已确认是CmdDb；原DEPEX亦为CmdDb。
  表长度不一致存在断言/自循环，故仍须真机验证，不称完整无风险审计。
- NPA GUID 79d6c870-725e-489e-a0a1-27e7a5d0cb35，Clock在0x11510查询，失败返回NULL；
  NpaDxe在0x1454安装同GUID（0x11058），原DEPEX=TRUE。尚未审完其初始化，暂不启用。
- Clock_InitCPU查询SCMI GUID 09b8ba84-3dd3-49a6-a05a-3134a5f07bad（0x28138），
  失败走0x48f4日志后跳0x4968；不能将该CPU路径失败泛化为所有UFS时钟均失效。
  同GUID在QcomScmiDxe命中；其DEPEX依赖330ed179-7c94-407c-af99-ddcc6244a031，
  后者也在MailboxDxe命中。这里只记录候选服务链，未据此证明安装点及完整ABI。

本次只增加原厂PwrUtilsDxe和Clock的PwrUtils调度依赖，不改PE，不新增NPA/SCMI/Mailbox/UFS。
终点标签CLOCK-PWRUTILS v1，多查询一项PwrUtils；原GOP/Vol+和BDS前停止保留。
预期pwr_utils缺失日志消失，NPA/SCMI缺失可能保留。Clock成功仍不能证明所有电源路径可用。

## 第六轮：9369/9370及NPA单模块增量

9369中PwrUtils缺失刷屏消失，NPA/SCMI仍缺失；9370终点PwrUtils/Clock均Success，RGBmatch=1且Vol+ armed。
不据此声称CPU调频或UFS可用。

原NpaDxe DEPEX为TRUE。真实入口0x141c调用0x24c4后安装协议。
0x24c4初始化缓存、锁及池，调用0xd9f4；后者通过0xdab4查询DALSys（GUID在0x11098）。
0x25bc调用0xd7bc日志包装器，查询0x11088 GUID 93a824b1-f77c-4880-ad95-e7bc634f9051，
成功时使用接口+0xf0；该GUID也存在于原ULogDxe的0x11098。
后续0x2748/0x274c初始化池/锁，0x2764及0x2780调用资源注册路径0x6dcc。
这些路径和内存/日志初始化存在断言；未证明整个NPA及Clock资源请求无硬件副作用。

本轮只加入原NPA PE，调度依赖DALSys及上述日志服务；Clock的既有五项依赖再加NPA。
不加SCMI/Mailbox/UFS/Pmic/Buttons，不改PON，不绕过断言，保留终点及Vol+。
输出检查验证NPA原PE及两项DEPEX、Clock六项DEPEX和排除模块。
Docker DEBUG、GOP复制/边界及Vol+主机回归通过；真机验证仍待完成。

## 第七轮：9371结果与SCMI/Mailbox入口审查

9371确认终点NPA、Clock、PwrUtils、EnvServices、HWIO均Success，GOP RGBmatch=1，Vol+ armed。
照片未包含Clock入口期间日志，不能确认NPA缺失提示全部消失或SCMI错误现状。

Mailbox原DEPEX为DALSys。真实主体0x1530在0x156c安装GUID 0x9068的协议，随后0x1570调用0x2d30初始化。
因此协议安装与通道就绪必须分开验证，尤其不能仅据LocateProtocol成功认定初始化完整。
0x2d30调用0x4314、0x4648、0x3690等；0x3690通过0x3f1c获取通道配置，逐通道调用0x37b4、0x4668。
0x37b4将64位值拆为两次0x3b54调用，第二次偏移加4。配置地址及实际后端还需继续核对。
0x3f1c通过GUID 0x9078的Env服务取配置，故原DALSys DEPEX不足以表达此运行依赖。

QcomScmiDxe的0x1428入口先调用0x2008，安装其自己的基础协议d7e5abe9-33ab-418e-9f91-72dae2ba8e2f，
随后再次Locate该协议，并查询ChipInfo（0x7078），调用其+0x98方法。
所以d7e5abe9并非需要另找一个外部提供者的缺失协议；不能仅依据GUID扫描盲加模块。
后续基础协议方法调用及Mailbox传输/超时逻辑尚未完成审查。
本轮只记录真机结果与逆向证据，不启用Mailbox/SCMI，不生成新镜像，不操作设备。

## 第八轮：Mailbox写后端与SCMI响应超时

Mailbox 0x37b4把64位值拆为两次0x3b54，后者依通道结构+0x10字节选择后端：
- 0x44f4是直接32位store（0x44fc str w1,[x8]），返回1；地址来自配置结构的映射基址加偏移。
- 0x45b0通过缓存协议+0x38调用，命令0x02000502，传地址和值；该协议完整身份/ABI尚待核对。
  该分支还会拒绝零值，故不能将它与直接store视为完全等价，也未修改原代码。

配置读取0x3f1c使用Env协议路径/sw/mailbox/mailbox-config，枚举子项后在0x40c8经接口+0x58读取80字节属性。
0x4054分配每通道0x78字节结构，0x40d0起复制地址、偏移和标志；0x4214根据标志决定是否调用0x46c0映射。
未取得Elden实际属性内容，因此尚不能给出安全可用的物理地址列表。
在当前导入Binaries内按完整路径做二进制文本搜索，仅命中MailboxDxe；这不证明配置不存在，
因为配置可能采用分节点路径、压缩或由Env从其他来源加载。

SCMI响应路径0x36b8将1000000传入0x32e8；轮询共享区+4的bit0，未就绪时调用BootServices.Stall(50)。
每轮计数加50，超过参数返回EFI_TIMEOUT（0x8000000000000012）。计数比较使用<=，故可多一次50us等待。
这是名义约1秒的有界轮询，不是所有调用的总体截止时间；Stall故障、异常地址和安全调用挂起仍不受其保护。
本轮继续保留clock-npa-v1，无新增可执行固件。下一步定位Env实际配置来源并还原通道属性，
再判断是否适合Mailbox单模块初始化测试；不按猜测地址写寄存器。

## 第九轮：找到原厂Mailbox配置及Env接口来源

在1.5.10.259刷机包的image/xbl_config.elf中找到配置，而非导入驱动目录。
此前提取的porting-artifacts/xbl-config/fdt-000080dc.dtb与ELF从0x80dc起的141369字节逐字节相同。
DTB SHA256为a7d84636caefcb1476b852db81fdfb7bc1d76c0454d7abe6df0881e9c7be15c1。
其/sw/mailbox/mailbox-config包含：

| remote | tx_pa | rx_pa | irq两单元 | is_pdp |
| --- | --- | --- | --- | --- |
| pdp0 | 0x18980000 | 0x16420000 | 0x40, 0 | 1 |
| pdp1 | 0x19980000 | 0x16420000 | 0x40, 0 | 1 |
| cpucp | 0x17830000 | 0x16430000 | 0x3a, 0 | 0 |

三者tx_size=0x300、rx_size=0x4c08，tx/rx的idr_offset=0、cmd_offset=0x100；
rx_map/stat/clear/en_offset依次为0x4000/0x4400/0x4800/0x4c00。
tx_sec_access与rx_sec_access均为单字节0。
这些地址及大小均落在当前Elden APSS_HM映射[0x16000000,0x1c000000)内，属性NS_DEVICE。
映射覆盖不等于外设可安全访问，更不证明当前运行时采用上述配置。

Mailbox RVA0x7d76的字符串实际为格式串ddwwwwwwwwwwwbbbb，而非单个属性名；
前轮“读取80字节属性”的表述仅描述调用缓冲区，不能据此推定接口ABI或字段编码。
需继续还原Env +0x58参数/字段描述表，不能用猜测结构体调用。

EnvDxe RVA0x2c44路径查询GUID 3a06e1e8-61f6-11eb-bbed-4b476e2ff6a7的HOB，
取HOB+0x18保存的指针，将其指向的0x148字节接口表复制后发布EnvServices协议。
源码交叉验证：Sm8750 PlatformPeiLib.c的InstallPlatformHob调用InitProtocolFinder，
将DTBExtnProtocol指针放入gEfiSecDtbGuid的新HOB。
AndromedaPkg/SecProtocolFinderLib先搜索保留FD中的XBLCore TE，再通过GUID引用附近的
AArch64指令定位接口表。不是直接沿用整个旧HOB列表，也不是加载仓库中的stock.dtb。

这说明运行时配置服务依赖当前启动链留下的XBL接口。测试配套为1.1.11.263，
本次离线配置来自1.5.10.259，二者不能直接画等号。
本轮仅推进来源审查，未启用Mailbox/SCMI/UFS，未修改MMIO映射、按键逻辑或生成新固件。
下一步应先核实Env查询ABI并做只读运行时配置核对，再评估Mailbox初始化。

## 第十轮：ENV-CONFIG v1只读运行时检查

由原Mailbox调用点确认四个接口均不传This：
- +0x30：0x3f74传16字节节点输出指针x0及路径x1，返回w0，零成功。
- +0x28：0x3fb0传节点指针x0、UINT32计数输出x1。
- +0xf8：0x4018–4028传父节点x0、子节点数组x1、UINT32数量w2；每节点16字节。
- +0x58：0x40b4–40c8传子节点x0、格式串x1、80字节缓冲x2、大小w3，返回w0。

在Elden既有DxeCore终点增加上述查询，计数必须为1至8；任一步非零返回即跳过其后续调用。
每次调用前后打印，80字节缓冲清零且8字节对齐。只将前两项按原厂解析的tx/rx输出，
其他字段保留原始64位值，不凭未确认的结构体布局强行解码。
不调用Mailbox映射/初始化/IRQ注册或SCMI事务，不增加驱动，不动Vol+/电源键。
本诊断不能防止被调用配置接口内部异常或卡死；BEGIN/END用于定位，硬件恢复键仍需保留。

## 第十一轮：9372通过；Mailbox初始化发现线程依赖

9372真机确认Find/Count/Children及三项Parse均raw=0，count=3。
TX/RX分别为18980000/16420000、19980000/16420000、17830000/16430000，
与离线1.5.10.259表对应；不能将这一局部一致扩大为两个版本全部配置一致。
ENV-CONFIG END、GOP RGBmatch=1、Vol+ armed均出现。

本轮没有直接加入Mailbox：其0x4668调用0x4de4注册中断处理路径。
0x4de4分配上下文，0x4e38调用事件接口，0x4e7c通过0x28c0创建线程，
名字格式IsrThr_%03d，线程入口0x4f24，栈大小参数0x1000。
0x28c0通过缓存表0x91f0的+0x18子表首函数调用；事件通过+0x20子表。
线程入口循环等待事件并调用回调。相关子表完整来源及运行时调度状态还需核对。
0x4eb4注册中断包装回调，0x4ebc调用线程操作；0x4668还调用触发配置及中断操作。
这些不属于纯协议安装，不能因为SCMI响应有轮询就忽略Mailbox自身的线程初始化。

重要错误传播问题：0x3730调用注册后，失败仅在0x374c记录日志，仍走0x375c的写入，
循环最终0x3774返回0。0x4668内0x469c/0x46a4的返回值亦未用于最终失败判断。
因此入口Success和LocateProtocol成功都不能独立证明中断通路可用。

当前Elden配置EnableMultiThreading=0；Pakala PrePi只在其非零时调用StartUpScheduler。
这证明本工程没有走该调度器启动路径，不证明继承的XBL线程接口一定不可用。
暂不为Mailbox盲目开启多线程，也不删掉其IRQ路径后伪装原厂驱动成功。

回查UFS 0x18820：0x18970/+8查询Clock，0x189a4/+0x10调用另一Clock方法，
0x189e4/+0x20及0x18a20/+0x10仍有后续调用。该调用点不是直接查SCMI；
是否间接需要SCMI必须继续沿Clock方法实现追踪，不能从CPU路径报错推断UFS必需SCMI。
下一步优先收敛UFS实际分支/时钟路径；本轮仅分析记录，无新固件。

## 第十二轮：UFS Clock接口与屏幕日志收敛

Clock协议表0x28148的revision=0x1000b，+8指向0x156c，+0x10指向0x15ac。
0x156c替换This为内部对象后调用0x5a40；后者检查名称/输出指针并遍历六个后端查询函数。
0x15ac调用0x8e38，第三参数置零；还需审查其后端，不能仅凭包装器声称不需要SCMI。
UFSDxe包含/soc/ufs%d/ufs_clk%d路径，配置错误日志区分节点、名称和配置读取失败。
下一步从这些运行时配置及Clock后端收敛UFS实际需求，不强制先开启Mailbox线程。

按用户要求新增Elden专用ELDEN_COMPACT_LOG编译开关，普通屏幕串口写入只过滤完整单行：
PROGRESS CODE、Loading driver at、Memory Allocation以及ELDEN DRIVER END以Success结尾的行。
驱动BEGIN、失败END、错误/断言、关键测试均保留；分片、多行、Critical输出不丢弃。
当前屏幕配置未开内存日志，不宣称隐藏消息另有持久副本。
主机测试编译真实过滤函数，ASan/UBSan覆盖边界长度、分片、多行及错误保留。
本次固件只修改显示日志策略，不增加硬件调用或修改Vol+路径。

## 第十三轮：UFS时钟配置读取诊断

用户确认FAST-SCROLL速度可接受，保留该实现，不继续更改日志路径。
UFSDxe 0x18bd8–0x18cb0固定循环9项，路径/soc/ufs%d/ufs_clk%d。
0x18c38通过Env+0x90读取clk_name字节数到UINT32；0x18c78通过+0x98读取字符串，
参数为节点、属性名、输出缓冲、字节数，无This。0x18c9c通过+0xa8读取config到24字节缓冲。
后续0x18cc0起缓存每项40字节结构，才返回Clock方法调用路径。
离线表有9项：axi、ice_core、unipro_core、aggre_axi、ahb、phy_aux、tx_symbol0、rx_symbol0/1。
前三项config=0,0,0x5f5e100,0xc02a560,0x18054ac0,0x18054ac0，其余六项全零。

新增终点UFS-CONFIG v1仅复用这些Env调用，不调用0x18820（其中包含MMIO写入）。
名称长度限制1至95，96字节清零缓冲保证打印终止；固定9次循环及24字节配置，失败立即停止。
打印每次接口前的BEGIN、错误原码及最终完成数，保留已验证GOP和Vol+。
尚未调用Clock GetId/Enable，不将配置读到等同于时钟、UFS或SCMI可用。

## 第十四轮：9373确认配置，GetClockId诊断

### 9374后续：状态查询

9374的9个ID依次为4010082、4010083、4010088、4010081、4010046、
4010084、4010087、4010085、4010086，全部class 1，域4。
Clock +0x20 -> 1634 -> 8f14仅接受class 1 -> 7318。
7318经5ae0解码、390c/391c锁定；7360及73c8/73fc为单字节输出。
有条件分支在读HAL前后调用b71c，参数状态从0x30回到0，不能称纯读。
HAL f79c间接转到描述符方法表+0x10；具体硬件读及资源后端仍待实机验证。
本轮使用原UFSDxe同一调用ABI，输出预置255检测旁路成功却未写入；
只接受class 1和0/1输出，错误停止，保留每次调用前日志及终点Vol+。
此实验不显式Enable、不启动UFS；不是时钟资源完整可用的证明。

9373确认9/9名称/配置，前三项0,0,100000000,201500000,403000000,403000000，其余全零。
照片末尾Vol+ armed；不据此声称Enable或存储可用。

Clock 0x254f8查询函数表依次为7f18、554c、5710、51e4、82bc、78e4。
六者均经e5c0字符串比较，匹配后构造类别6、3、2、1、5、4的ID。
类别字段位于16至23位；部分ID还含24位以上的域索引及低16位项索引。
554c/5710/51e4匹配后进入5128，为内部对象查找/分配24字节客户端链表记录；
不是纯粹无内存副作用的查询，但所审分支未执行Clock启用或SCMI发送。
5128的分配失败可导致查询失败；不要忽略返回状态仅使用已写入的ID。
5a40在0x5ac8向输出执行64位store，故本轮不能用常见的UINT32 ID缓冲。

新增UFS-CLOCK-ID v1按原UFSDxe 0x18964–18970的参数形式传This、名称、64位输出。
revision必须0x1000b；每次调用前后打印，失败/零ID/类别非法停止。
只有GetId，没有Enable、SetFrequency、Mailbox、SCMI或存储I/O。
下一步根据真机ID类别追对应启用分支，避免将CPU的SCMI依赖泛化到这些UFS时钟。
# 9385: HALIOMMU GET-TABLE v1

Photo confirms protocol lookup Success/present=1 and terminal Vol+ loop,
not the driver's StartImage result. Capture the exact HALIOMMU file GUID's
StartImage return in Dispatcher and repeat it at the terminal checkpoint.
Only on a successful returned StartImage and successful lookup, call protocol
offset +8 with a single output pointer. Stock disassembly 0x152c..0x1538
stores internal table 0xb0d0 into [x0] and returns; no This/status argument.
Do not dereference the returned table or create/configure/attach domains.
UFSDxe remains excluded. Build and hardware validation are separate.
# 9386: DOMAIN-ALLOC v1

Hardware confirms saved StartImage returned=1 status=Success and GET-TABLE
present=1. Next test creates and destroys one fresh software domain only.
Stock table 0xb0d0: +0 -> 0x1644, +8 -> 0x16a0.
Create allocates 64 bytes (0x2f68 -> BootServices AllocatePool), zeroes them,
sets first word to 1, returns pointer and INT32 zero. Destroy on this fresh
object sees zero attachment count at +0x38 and zero list at +0x30, then
calls 0x2fa8 -> FreePool. The attached-object destroy branch is not exercised.
Call signatures: INT32 create(VOID **), INT32 destroy(VOID *), no This.
Only destroy on raw=0/non-null; no configure, attach, MMIO or DMA submission
added by this test. UFSDxe remains excluded. Hardware result pending.
# Combined IOMMU software stage (supersedes unflashed DOMAIN-ALLOC)

Batch create, configure mode 0 and 1, reject mode 2 without object mutation,
destroy, and verify all five UFS SID mapping fields against stock FDT.
Configure table +0x40 -> 0x20f4 accepts only 0/1 and updates object; 0x2030
returns without register writes when attachment list +0x30 is empty.
Invalid mode 2 returns INT32 2 before writes. Fresh object size is 64 bytes.
Attach +0x10 -> 0x1744 reaches SMMU context allocation/register writes at
0x1a60 onward; deliberately NOT called independently of UFS ownership.
Queue snapshots (including MCQ-capable CAP) do not prove DMA quiescence.
SID path /soc/iort/NAMEDNODE_UFS_95MEM/SIDMappings/SIDMappings_0:
InputBase=03000000 NumIDs=1 OutputBase=60 Flags=0 OutputReference=56.
Expected terminal SOFTWARE-STAGE checks=4/4 and SID-MAP match=5/5.
This is pre-attachment validation, not complete hardware IOMMU/UFS support.
# 9387 passed: gated stock UFS start v1

Photo confirms software checks=4/4 and SID mapping match=5/5.
Next package includes unchanged stock UFSDxe with a private DEPEX gate,
released only after successful software stage, SID mapping and two HWIO maps.
Initial DXE dispatch cannot start it. Terminal releases gate and dispatches,
records actual UFSDxe StartImage result, enumerates BlockIO handles only.
No ConnectController, partition scan, ReadBlocks/WriteBlocks or BDS added.

This is a HARDWARE INITIALIZATION trial, not a read-only register test.
Stock branch8 maps UFS/ICE then 42a0 -> b350 -> 1b080 creates/configures/
attaches IOMMU, followed by a910(0,0xd0) device initialization. Attach is
owned by this stock path; no independent diagnostic domain is attached.
The previous software-test domain is destroyed before releasing the gate.
Stock original DEPEX is TRUE; compiled gate and unchanged PE are checked.
No claim that queue snapshots prove quiescence, nor that stock entry is free
of device configuration changes. Device-internal commands and all polling
paths are not fully audited. A hung stock call cannot be preempted by this
diagnostic; capture last DRIVER BEGIN and use hardware recovery keys.
Vol+ remains available if control returns; no automatic flashing performed.
# 9388: UFS failed-open diagnostic

Photo: ufs_error_log 3b then 35, Ufs.c +2326 ASSERT, reboot seconds later.
Do not identify reboot cause solely from timing. 112b8 logs 3b after f850
fails for UIC opcode3/argument20000. 102dc logs35 after HCS bits10:8 >=3
following power-mode completion; returns -5. Device open a910 returns NULL,
42d0 falls through to assertion at42fc (line916hex=2326).
Generate hash-pinned diagnostic PE from untouched stock source. Single
instruction at42d4 branches to existing42b8 EFI_DEVICE_ERROR return path.
No assertion globally disabled; no continued use of NULL; success unchanged.
This avoids this specific ASSERT, not a UFS fix or guarantee against reset.
After driver return, read HCS/HCE/UICCMD/ARG1..3/doorbells only, no retry.
SMMU/device configuration may persist after failure. No cleanup/detach guessed.
Package elden-aloha-ufs-failreturn-v1; hardware validation pending.
# 9392: bounded UIC local/peer comparison

Fail-return worked on hardware: no reboot, driver Device Error, BlockIO=0.
UICCMD3 ARG1=20000 ARG2=a ARG3=2. BSP ufshci.h defines cmd3 PEER_GET,
result a DME_FAILURE (NOT peer communication failure8 or busy9).
Do not interpret ARG3 as valid attribute data when command failed.
After failed driver return only, issue local GET then three spaced peer GETs
of the same attribute. Guard HCE=1, UIC ready, traditional doorbells zero.
No MCQ-quiescence claim. Clear only completion bit10; command args then fence
then command; poll at most100x1ms; abort all further commands on timeout or
Stall failure. 100ms spacing. No DME_SET, power-mode change, driver retry,
block command, partition access or data write. This is a diagnostic, not fix.
Raw result+data+HCS logged for each. Hardware validation pending.
# 9393: before/after link profile

Local attribute2 succeeds(data5); three peer attempts return a. Invalid peer
data ignored. Do not infer physical link failure from DME_FAILURE alone.
Local BSP auto-mode workaround is restricted to controller major1 or legacy
major2; no evidence justifies applying it here. Add before/after profiles:
local PA_PWRMODE/TXGEAR/RXGEAR/ACTIVETX/RX/HSSERIES/attribute2, peer attribute2
and PA_CONNECTEDTXDATALANES. No SET or power change. Same bounded polling.
Preflight timeout/guard/Stall failure prevents driver gate release. Completed
UIC errors are logged and allow original driver trial. GETs can affect timing;
this is a controlled diagnostic comparison, not a passive trace.
Package elden-aloha-link-profile-v1. Hardware validation pending.

# 9394: bounded AUTO compatibility experiment

Photo shows peer reads already fail with result a BEFORE UFSDxe. AFTER local
PA_PWRMODE=11, TX/RX gear=5, active lanes=2, HS series=2; peer reads still a.
This does not establish the legacy Qualcomm AUTO quirk on SM8750.
Package elden-aloha-auto-trial-v1 tests that hypothesis AFTER failed driver
return only: require verified mode11, DME_SET PA_PWRMODE to44, wait up to1s
for BOTH UIC completion and power completion, require ARG2=0 and UPMCRS=1.
Read full local/peer profile, restore11 with the same checks, profile again.
Timeout/incomplete profile stops subsequent UIC commands, including restoration
when command state is unknown. A reboot is then required. No driver retry,
partition scan or block I/O. This DOES change link power mode; it is not a
read-only test. Traditional doorbells zero is not proof of MCQ quiescence.
Existing firmware owns the inherited controller state; no DMA takeover is added.
Hardware validation pending. Preserve original PE, fail-return patch and Vol+.

# 9395: AUTO rejected; UIC evidence v1

AUTO SET44 returned seen400/resulta/HCS10f/polls0. No power-completion bit4
was observed and no AUTO readback or restore ran. This is DME_FAILURE, not
proof of entering AUTO or a successful transition. Disable the experiment.
Capture AHIT, IS/IE, HCS/HCE, CMD/ARG1-3, five UIC layer error registers,
CAP/VER/MEMCFG before each BEFORE/AFTER profile and on every failed GET or
timeout, before acknowledging completion. Each register is captured once
before printing; error registers may clear on read, so this is diagnostic
instrumentation, not a non-invasive trace. No extra duplicate peer batch.
AHIT has not yet been measured; inherited auto-hibernation is a hypothesis,
not a root cause. Do not disable it or reset the controller without evidence.
Register offsets use local msm-kernel/include/ufs/ufshci.h. No new power or
DMA writes; original single driver trial retained. Package
elden-aloha-uic-evidence-v1, pending hardware validation.

## Original UFSDxe static initialization-path audit (2026-09-13)

Disassembled the pinned original UFSDxe.efi (SHA256
dac620aca276e735361d8b747fe0218b6125eae004116391270f32a72e6ce750)
with llvm-objdump. Addresses below are PE RVAs, not runtime addresses.
This is static evidence, not proof of which branch ran on Elden.

- Entry opens controller 0 / LUN 0xd0 through a910 -> a87c -> b630.
  b630 initializes the 0x4e0-byte context at RVA 22d80 and calls
  197fc(context + 0x48), then 1148c. State 2 is only set after success.
- 197fc locates GUID 157a5c45-21b2-43c5-ba7c-822fee5fe599 (RVA
  20308), matching EFIPlatformInfo.h, and calls protocol slot +8.
  Platform 0x0f matches RUMI in EFIPlatformInfoTypes.h. If lookup/call
  fails or platform is above 0x29, it also checks physical 0x01fc8004
  low two bits against 1. Do not add a speculative read of this address.
- 19878..19898 sets context[0x48] to (mode == 0x0f), and
  context[0x49] and [0x4a] to (mode != 0x0f); [0x4b] is zero.
  Thus ordinary non-RUMI defaults differ from the emulation path.
  This selector is NOT the previously logged IMEM boot selector 8.
- b6a0..b6d4 can clear context[0x49] when the saved context[0x4d6]
  is 1 or global byte RVA 23255 is nonzero. The b454..b480 path
  explicitly sets context[0x4d6], saves a pointer at +0x4d8 and
  reopens LUN 0xd0. Its external caller/contract still needs tracing.
- 114dc..114e8 skips to 118c0 for controller 0 when context[0x49]
  is nonzero. It calls 18820(0,4), then joins at 118fc. This is not
  a no-hardware-writes path: 18820 includes clock/register operations.
- The non-shortcut path includes HCE=0 at 11584, PHY table application
  via 11ba8, and UIC command 0x16 via f850 at 11864. Do not force
  this branch merely because the shortcut exists; inherited DMA/state,
  reset, power and table prerequisites must be understood first.
- Both branches join before 1191c -> 11284. That routine calls
  f850 with command 3, ARG1 0x00020000. Failure logs 0x3b at
  112b8; the caller logs 0x55 at 11928. f850's nonzero UIC result
  path logs 0x11. This identifies the photographed 11/3b/55 chain
  as a peer capability query failure, before a successful open.

Consequence: local GET success and identical BEFORE/AFTER link profiles
do not establish that full link startup ran. Prioritize tracing normal
stock predecessor state and the explicit reinitialization API, plus
confirming the live selector/override bytes, over more isolated GET tests.
No binary patch, controller reset, power-mode SET, build or flash is made
by this audit. Existing fail-return patch remains unchanged; errors must
not be converted into success. Full early-XBL loader analysis is not yet
complete; this section describes its extracted UFSDxe module only.

### Early XBL comparison and correction of the tuning entry

The UFSDxe caller at RVA 2a5c calls b428 at 2ad8. Its failure string at
1b93c is `ufs_phy_tune_init failed with return value %d`. It requires
an existing valid driver object/handle and a non-null tuning argument.
Therefore b428 must not be treated as a generic safe reset/retry API.

Analyzed original `TB322_ZUXOS_1.5.10.259_Tool/image/xbl_s.melf`, SHA256
4bce63da71141e8b8bf0e7e68d79716ee9e466a3e7bcaeebf1229b9205e344b8.
This container has ELF headers at file offsets 0 (ELF32 RISC-V), 0x904
(ELF32 RISC-V), and 0x2e574 (ELF64 AArch64). The UFS implementation
below is in the THIRD ELF, not the outer RISC-V header. Embedded ELF
program-header offsets are relative to that embedded header.

- ARM64 entry: 0x14825fa8. Executable segment file offset 0x2f574,
  VA 0x14825000, size 0x90000. Read-only data: file offset 0xbf574,
  VA 0x148b5000, size 0x13000.
- `ufs_error_log` string: file offset 0xd1541, VA 0x148c6fcd;
  ADRP/ADD reference 0x148af83c/0x148af844. Logger entry 0x148af828.
- Early equivalents: UIC helper 0x148b02e4, peer capability query
  0x148b16cc, setup 0x148b17b8, initialization 0x148b18d4.
  The peer failure logs 0x3b at 0x148b1700 and propagates as 0x55
  at 0x148b1cc0, matching the DXE error-chain structure.
- Crucial default difference: early XBL's flag initializer 0x148b487c
  receives context+0x48. At 0x148b4908 it unconditionally stores ZERO
  to argument+1 (context+0x49). By contrast DXE 19894 stores the
  non-RUMI predicate there. Early XBL thus defaults to full initialization,
  while ordinary-platform DXE defaults to the shortcut.
- Early branch at 0x148b1924 checks that same byte. Full path issues
  LINK_STARTUP command 0x16 at 0x148b1bec..0x148b1bfc; shortcut
  0x148b1c58 calls 0x148b4134(0,4) then joins at 0x148b1c94.
- Early context initializer additionally calls 0x148b49ec at
  0x148ad0d0 before initialization. Its strings resolve to `/soc/ufs%d`,
  `LinkStartupRetryCount`, `BatteryThresholdMv`, `RefClock`, `RefClock2x`,
  and `/soc/ufs%d/timeout_values`. These are configuration inputs,
  not measured rail/clock status. The initializers are not interchangeable.

These mappings were obtained with bounded ELF program-header parsing,
ADRP/ADD string-reference decoding, BL target decoding and AArch64
disassembly of temporary extracted ranges. No live physical-memory access
was performed. The stock package version is 1.5.10.259; equivalence to
the device's currently installed earlier stages remains unverified.

Next evidence needed is the actual inherited state after later stock
stages and the live DXE selector/override values. Do not infer that simply
clearing context+0x49 fixes the failure: full initialization changes PHY,
HCE, clock and DMA state. No new flash image is produced by this comparison.

### UFS-CONTEXT v1 implementation

Added a GUID-selected ImageHandle marker around the existing dispatcher trial.
Image.c copies the pinned PE's context and selector bytes immediately after
the entrypoint returns, before CoreExit may unload it. Guarded by ELDEN_DXE_TRACE,
minimum image size and instruction words at 114dc, 19894 and patched 42d4.
No vendor pointers are dereferenced. Captures context state, flags 48..4b,
override 23255, platform latch 23be8, tune 4d6, retry 4cd, error index/count
and all eight ring slots. This is post-return data, not an executed-branch trace.
The platform latch is only a RUMI latch in this implementation; do not label
zero as a verified platform type. No initialization branch or UIC command changes.
DEBUG build and existing memory, Vol+ and fast-scroll host tests passed;
hardware validation pending. Package: elden-aloha-ufs-context-v1.

### Photo 9399: context capture validated

Hardware returned Device Error without reboot. Snapshot:
controller=0, state=0, flags=00/01/01/00, platformLatch=0,
override=0, tune=0, retry=5; error index=3, count=3,
slots[0..2]=11/3b/55 and slots[3..7]=0.
The post-return flags satisfy the inherited-path condition at 114dc..114e8.
No force override/tuning flag is set. Combined with the static call chain,
this supports the default shortcut followed by the failing peer query;
it is not a complete instruction trace or proof of the underlying link fault.

Cold-path prerequisite found while reviewing the next experiment:
11528 calls 1974c, which writes 1 then 0 to physical 0x00177000
(19778/19788), separated by a 500-us Boot Services stall. This is the
GCC UFS reset address consistent with the local topology audit. The current
PlatformMemoryMapLib table does not cover that physical address. Dynamic
mapping has not been established by this audit; do not assume the cold path
can access it merely because HCI MMIO and Clock protocol lookup worked.
Do not clear context[49] or call the PHY tuning API as an immediate fix.
Full-path MMIO coverage, configuration tables and inherited DMA ownership
must be checked before a reset-bearing trial. Existing firmware unchanged.

### UFS-FULL-INIT v1 trial

Added GCC 0x00100000+0x001f5000 as NS_DEVICE/AddDev/MMAP_IO, page-rounded
from local sm8750.dtsi's 0x1f4200 aperture. Memory host tests verify no overlap.
UFS PHY/HCI parent remains 0x01d80000+0x40000. Table loader 191ec uses Env
Find(+30), size(+90), U16-array read(+f0); it converts triples into six-byte
records. Writer 11ba8 filters flags, terminates on flag 8000, then writes the
byte value as a 32-bit MMIO store at parent+16-bit offset. Existing parent
mapping covers these offsets. Offline XBL config has Init/End/Lane/EOM tables;
this does not prove their live Env contents or safe hardware timing.

Pinned patch 19894: 39000669 -> 3900067f, clearing only context+49 as early
XBL does. Retain fail-return at 42d4 and all stock failure handling. Adjust
context-snapshot instruction guard to this precise new instruction.
Existing pre-driver profile requires HCE=1, UIC ready and zero traditional
doorbells; these checks are not universal DMA-quiescence proof. The original
driver remains responsible for its IOMMU setup and resets/queue programming.
This is a reset-bearing hardware experiment, not a proven fix or read-only
probe. No additional outer retry, BDS/partition scan or media-write request.
Host memory/Vol+/scroll tests pass; hardware outcome pending.

### 9400/9401 and COLD-TRACE v1

9400 full-init stops after DRIVER BEGIN, then user reports black screen,
crashdump and reboot. No context snapshot or error-ring return was reached.
9401 restored context-v1 returns with the previous 11/3b/55 chain. Do not
attribute the crash specifically to GCC BCR without further evidence.

COLD-TRACE appends a position-independent RX PE section and redirects ten
hash-verified BL sites through before/after logging wrappers. Original callee
targets and control-flow error checks remain unchanged. Saves/restores x0..x18,
x30 and NZCV across DebugPrint; wrapper preserves caller x29/LR. These sites
have integer arguments/results; no FP/vector argument ABI is claimed.
No logging inside the HOST_READY polling loop. Stage labels and RVAs live in
prepare-elden-ufs.py STAGES. No new absolute relocations. PE size/code-size,
section count and checksum updated; original imported binary is untouched.
Logging changes timing. END x0 is not a status for void-returning callees.
Host checks cover deterministic output, branch destinations, section bounds,
marker count and absence of per-poll logging; hardware validation pending.

### 9403 and COLD-TRACE v2

Photo 9403 reaches LINK_STARTUP END x0=0 and PEER_CAP END x0=0;
user reports subsequent crashdump/reboot. This run passed peer query, unlike
the inherited-path 11/3b/55 failures. Timing sensitivity remains possible.
No main driver return was observed. Do not infer DMA or power root cause.
Disassembly after 1191c programs queue bases at 11944/54/64/74, polls HCS
bit 0, calls local GET 1561 at 119f4 and writes UTRLRSR at 11a14.
v2 wraps those five verified STR instructions with integer/flags-preserving
markers and relocates only these non-PC-relative instructions. Plain B
enters/exits the stubs, preserving original LR. No polling-loop hooks.
Additional BL wrappers bracket host/context return and the a910 device path;
address-based DEVICE labels intentionally avoid guessing vendor symbols.
All original target calls, stores and error branches remain intact. These
are reset-bearing hardware diagnostics, not a safe read-only or fixed driver.

### 9409 and COLD-TRACE v3

Photo reaches all four queue-store END markers, LOCAL_1561 END, QUEUE_RUN
END, HOST_INIT END, CONTEXT_INIT END and DEVICE_D864 END x0=0. OPEN_INIT
END is absent. The image alone does not establish whether it later rebooted.
a8d4 tail-branches to c5d0, which calls f6f4 at c620 (at most two attempts).
v3 brackets that call plus f73c request preparation, f780 submission, f790
wait, f7b0 response parsing, f810 abort and f81c release. All BL targets
verified against the pinned stock disassembly; no inner-poll hooks added.
This narrows the remaining path without declaring DMA/SMMU the root cause.

### 9411 and COLD-TRACE v4

REQUEST_PREP END x0=0 is visible, followed by REQUEST_SUBMIT BEGIN without
END. No REQUEST_WAIT BEGIN. Focus is 10c0c, not the completion wait yet.
v4 brackets TRD first/last stores (10cbc/10d10), encoder 10d18, IS store
10db4, sync calls 10dd4/10de8/10e34, barrier calls 10e38/10e4c and
doorbell store 10e48. All original instructions/targets retained. No extra
MMIO reads or loop logging; no cache/IOMMU bypass. Runtime outcome pending.
### 9412: doorbell gate

User confirms crashdump/reboot after SUBMIT_DOORBELL BEGIN, without END.
TRD stores, encoder, IS write, both sync calls and pre-barrier returned.
The boundary is the 10e48 UTRLDBR store or an associated asynchronous fault;
no SMMU root cause is proven. Gate v1 logs x8/x9/x10/x19/x20 without pointer
dereferences and replaces this store with a branch to existing 10e20 -5
submit error epilogue. Original caller cleanup/retries remain, but every
attempt at this store is blocked. No fake success, no DMA submission here.
Cold-init writes still occur; this does not promise crash-free execution.
### 9414: gate returns; correct diagnostic x9

User confirms no reboot. Gate-induced submit -5 propagates through 142/228
and DeviceError; these are deliberate gate errors, not new hardware failures.
After-profile peer GET succeeds. v1 GATE_BASE=80000000 is INVALID evidence:
log() sampled x9 after MRS NZCV overwrote its scratch copy. Saved x9 was
restored, so the error affects reported value, not original register state.
v2 samples x0..x18 from saved slots and adds fixed-offset reads of known
live x19 context and x20 request structures at the same blocked gate.
No dereferences of the contained DMA addresses and no doorbell write.
### 9415: correct base, audit original domain operations

Gate base/context MMIO=1d80000, offset=4058, TRL=d5d1b000,
TMRL=d5d1c000, request TRD CPU/sync=d5d1b000, CMD CPU/sync=d5d1b400.
These are aligned and internally consistent, not proof of DMA translation.
Add wrappers at original BLR x8 sites 1b0f0 (table slot0), 1b16c (slot40),
1b1c0 (slot10), plus b358 init and 11404 allocation. Wrapper logs saved
integer arguments and restores x8 before the original indirect call; no new
domain calls, no guessed SMMU register reads, doorbell remains blocked.
Targets/return codes will distinguish actual domain path from preflight.
### 9433/9434: domain configuration audit

Actual create/config(1)/attach return zero. HALIOMMU 20f4 clears domain
08/10/18/1c, stores 0A9F00E0 at20 and zero at24, then calls2030.
Attach1aa8 forwards domain20 through22f8 to selected context-bank control.
This is not evidence of populated translation tables. No mode change made.
Domain trial snapshots 64 bytes from original attach argument only on return
zero; first non-null attachment head at domain30 is a 32-byte allocation
(189c..18c8), read once, no list walking or MMIO probe. Node00 is selected
SMMU identifier/base, node08 packs type and context index; node10 list head,
node18 next. Original x19/x20 saved/restored, original x0 status preserved.
Doorbell gate remains active. Hardware validation required.
