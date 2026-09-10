# Elden 第二源码检查点：DXE调度到达明确终点

2026-09-11，用户照片9334.jpg确认ULogDxe等返回Success，D5调度返回，
D6报告缺失协议，然后显示TEST COMPLETE: intentional STOP before BDS。
用户观察期间未黑屏/重启；未给出时长，不宣称长期稳定。

- 保存分支：checkpoint/elden-dxe-dispatch-v2-working。
- 开发分支：lenovo-elden-wip。
- MU_BASECORE子模块也独立提交并建立同名保存分支；主仓库gitlink固定该提交。
- 本次包含：3倍有序屏幕日志、PHIT范围修复、DXE分段诊断及ULog的DALSys协议DEPEX。
- 对应测试包：../porting-artifacts/elden-aloha-ulog-depex-v1。
- boot SHA256：7a38dfb50150f39bd9c04f32bf842faa2aa62cf1d4e8e165539cd877539ba940。

这是本轮DXE调度完成检查点，不是完整UEFI。Security/BDS/Variable/Reset/RTC等协议仍缺失，
当前故意在BDS前CpuDeadLoop，不搜索EFI，不修改电源键或PMIC。
第一个棋盘格检查点保持原样。生成镜像及外部原厂boot/DTB不进入Git。
复现需保留本地子模块提交：尚未推送远端，从远端全新克隆不保证能取得该提交。

在干净工作区恢复主仓库检查点后，还需同步子模块到其gitlink：

```bash
git switch checkpoint/elden-dxe-dispatch-v2-working
git submodule update --init --recursive
```

如有未提交改动，先保存，不强制覆盖。后续按缺失协议及DEPEX逐项诊断，暂不扩大到存储/BDS移植。
