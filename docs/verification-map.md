# 验证路线｜并发接纳、完成义务和关闭竞争

本页仅索引仓库现有代码、测试与负载；**不声称形式化证明完整Executor无锁**。

## 核心合同

1. **接纳线性化点：** `Submit`的成功返回应对应一项被系统接纳的任务，失败返回不可默默占用队列资源。
2. **结果完成义务：** 已接纳的任务需要有可解释的执行、拒绝、取消或异常终态；`std::future`不应永久悬挂。
3. **槽位所有权：** MPMC队列的槽位序号与原子可见性约束，应与测试中的重复、遗漏和竞争记录一同审查。
4. **停止协议：** `Submit/Stop`交错、重复Stop、容量耗尽和Shutdown必须分别验证；短时间未前进仅是停滞观察，不能直接证明死锁。

## 可检查的代码与实验

| 问题 | 入口 |
|---|---|
| 有界MPMC槽位与内存序 | [`src/mpmc_ring.hpp`](../src/mpmc_ring.hpp) |
| 任务生命周期与结果传递 | [`src/mpmc_fast_pool.hpp`](../src/mpmc_fast_pool.hpp)、[`src/task_results.hpp`](../src/task_results.hpp) |
| 任务竞争测试 | [`tests/`](../tests/) |
| 不同Executor实现比较 | [`benchmarks/compare_real_executors.cpp`](../benchmarks/compare_real_executors.cpp)、[`benchmarks/targeted_envelope.py`](../benchmarks/targeted_envelope.py) |
| 已保存的实验条件和记录 | [`evidence/showcase_envelope_current.json`](../evidence/showcase_envelope_current.json) |

## 最短复核

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

注意：Empty、Busy、Sleep等任务负载不能混成一个全局“最快”结论；吞吐量、排队延迟、完成比例和超时边界应分别报告。Linux CTest与其他平台执行语义不能直接划等号。
