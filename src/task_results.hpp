#pragma once
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace wg {
// Nonblocking, typed admission layer for any executor exposing
// bool submit(std::function<void()>). Rejection returns nullopt immediately.
// Accepted task exceptions reach the future AND the executor failure counter.
// Callers must retain the pool until task completion and owner shutdown.
template<class Executor, class F>
auto try_submit_result(Executor& executor, F&& callable)
    -> std::optional<std::future<std::invoke_result_t<std::decay_t<F>&>>> {
    using Fn=std::decay_t<F>;
    using Result=std::invoke_result_t<Fn&>;
    auto job=std::make_shared<Fn>(std::forward<F>(callable));
    auto promise=std::make_shared<std::promise<Result>>();
    auto future=promise->get_future();
    const bool accepted=executor.submit([job,promise]{
        try {
            if constexpr(std::is_void_v<Result>) {
                std::invoke(*job);
                promise->set_value();
            } else {
                promise->set_value(std::invoke(*job));
            }
        } catch(...) {
            promise->set_exception(std::current_exception());
            throw; // Preserve executor's own failed task metric.
        }
    });
    if(!accepted) return std::nullopt;
    return std::optional<std::future<Result>>(std::move(future));
}
} // namespace wg
