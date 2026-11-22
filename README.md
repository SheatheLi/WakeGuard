# WakeGuard

[**在线交互展示**](https://sheatheli.github.io/WakeGuard/) · [**工程证据与验证索引**](docs/verification-map.md) · [![CMake Smoke](https://github.com/SheatheLi/WakeGuard/actions/workflows/smoke.yml/badge.svg)](https://github.com/SheatheLi/WakeGuard/actions/workflows/smoke.yml)

> C++20有界MPMC任务执行器：围绕提交受理、槽位所有权、结果交付、Submit/Stop竞争与Shutdown协议建立可运行的并发合同，而不是简单宣传“无锁更快”。

**工程状态：** 可构建的原生C++实验性Runtime，含测试、示例与多负载基准。不承诺所有操作路径Lock-free，也不将有限时间内的停滞直接称为死锁证明。

## 工程问题

一个任务成功入队，并不意味着它的`std::future`最终一定得到结果。并发关闭过程中，接受、排队、执行和停止需要明确的状态约束，否则任务可能丢失、重复执行或悬挂。

```text
并发Submit ──> 接纳/拒绝判定 ──> 有界MPMC队列 ──> Worker执行
                    │                              │
                    └──── 完成义务 / Promise <─────┘
                                  ↑
                        Stop / Shutdown竞争
```

- **数据结构：** 基于槽位序号的有界MPMC Ring，审查生产者/消费者所有权。
- **生命周期：** 定义任务接纳点与每条终结路径，检查Promise/Future一致性。
- **并发竞争：** Submit/Stop交错、容量用尽、Shutdown与重复处理。
- **成本比较：** 原子实现、Mutex、Condition Variable、Semaphore在不同任务负载下的表现。

## 构建和测试

需要支持C++20的编译器、CMake和CTest。

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Windows环境需要已配置的MSVC或其他C++20工具链，具体命令应与所选CMake生成器一致。上面是默认跨平台CMake形式，不代表所有Windows工具链已经实测通过。

## 代码阅读路径

| 入口 | 说明 |
|---|---|
| [`src/mpmc_ring.hpp`](src/mpmc_ring.hpp) | 槽位序号与MPMC队列 |
| [`src/mpmc_fast_pool.hpp`](src/mpmc_fast_pool.hpp) | 原子队列任务执行器 |
| [`src/pool.hpp`](src/pool.hpp) · [`src/task_results.hpp`](src/task_results.hpp) | 实现比较、任务结果与完成路径 |
| [`tests/`](tests/) | 容量、交错与终态测试 |
| [`benchmarks/`](benchmarks/) | 空任务、Busy与Sleep等负载比较 |

## 测试与结果怎样解释？

历史基线记录**14项CTest合同通过**，并比较4生产者/4消费者及多种任务负载下的Accepted、Missing、Duplicate与Enqueue-to-start P99。不同负载的最优实现不相同，不能概括为“Lock-free始终更快”。有限20ms观察窗口内没有进展，不能直接推出永久性死锁；有界Ring的Lock-free特性也不能自动推广到整个Executor。

测试和负载数据分别见[`tests/`](tests/)、[`benchmarks/`](benchmarks/)及[`evidence/`](evidence/)。

## 可视化

[任务生命周期与关闭竞争的交互展示说明](docs/README.md) · [机制图（SVG）](docs/assets/figures/WakeGuard.svg)

**授权状态：** 根目录暂无LICENSE；公开前需要决定授权方案。
