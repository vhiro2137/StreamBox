# StreamBox 非功能性测试文档集

文档版本：1.0  
建立日期：2026-08-17  
管理边界：本目录独立于根目录的 `TESTING.md` 和 `ACCEPTANCE_TEST_REPORT.md`，不改变既有功能测试结论。

## 状态定义

| 状态 | 含义 |
|---|---|
| 计划 | 已定义但尚未开始 |
| 已执行 | 已取得证据，尚待汇总或评审 |
| 未执行 | 当前未运行 |
| 受阻 | 缺少设备、系统、权限、样本或已批准操作 |
| 完成 | 已执行、证据完整且结论经过评审 |

## 文档索引

- `NON_FUNCTIONAL_TEST_PLAN.md`：范围、阶段、风险、方法和准入/退出条件。
- `ENVIRONMENT_COMPATIBILITY_MATRIX.md`：系统、硬件、显示和音频覆盖矩阵。
- `NON_FUNCTIONAL_TEST_CASES.md`：可重复执行的详细用例。
- `METRICS_COLLECTION_TEMPLATE.csv`：性能与资源采样记录模板。
- `DEFECT_RISK_REGISTER.md`：缺陷和风险登记表。
- `PHASE_EXECUTION_SCHEDULE.md`：分阶段执行安排和依赖。
- `NON_FUNCTIONAL_TEST_REPORT_TEMPLATE.md`：结果报告模板。
- `APPROVALS_AND_RESOURCES.md`：需要审批的操作与外部资源。
- `APPROVAL_EXECUTION_LOG_2026-08-18.md`：受阻项目获批后的执行记录。

## 当前总体状态

| 项目 | 状态 | 说明 |
|---|---|---|
| 计划与模板建立 | 完成 | 仅新增描述性文档 |
| 基线采集 | 已执行 | 2026-08-17完成首轮启动、空闲、短时本地播放和媒体探针基线；状态级完整采样与独立首帧遥测受阻 |
| 压力、长稳、网络、安全测试 | 已执行/失败 | 分段累计2小时、4K/60fps、隔离弱网、TLS及重定向已执行；TLS验证失败为P0问题 |
| 跨平台、硬件兼容、发行测试 | 已执行/受阻 | 当前Windows组合及便携目录已执行；其余设备、平台和安装生命周期受阻并跳过 |

首轮执行记录见 `EXECUTION_LOG_2026-08-17.md`。已执行不等于通过；项目尚未确认性能验收阈值。

阶段性结果见 `NON_FUNCTIONAL_TEST_REPORT_2026-08-17.md`。

2026-08-18批准后执行结果见 `APPROVAL_EXECUTION_LOG_2026-08-18.md`；最新结论以更新后的阶段报告为准。
