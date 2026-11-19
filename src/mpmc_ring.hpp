#pragma once
// Bounded lock-free MPMC ring (sequence-per-slot, Vyukov design adapted for
// std::function). Reuses a known algorithm: NO originality claim. The lock-free
// queue is a separate research component, not yet a replacement for Pool.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace wg {
template<class T> class MPMCRing {
    struct alignas(64) Slot {
        std::atomic<std::size_t> sequence{0};
        T data{};
    };
    static std::size_t checked_capacity(std::size_t cap) {
        if(cap<2 || (cap & (cap-1))!=0 || cap>=(std::size_t(1)<<30))
            throw std::invalid_argument("capacity must be power of two, 2..2^29");
        return cap;
    }
    const std::size_t cap_, mask_;
    std::unique_ptr<Slot[]> slots_;
    alignas(64) std::atomic<std::size_t> enqueue_pos_{0};
    alignas(64) std::atomic<std::size_t> dequeue_pos_{0};
public:
    explicit MPMCRing(std::size_t cap):cap_(checked_capacity(cap)),mask_(cap_-1),
        slots_(std::make_unique<Slot[]>(cap_)) {
        for (std::size_t i=0;i<cap;i++) slots_[i].sequence.store(i,std::memory_order_relaxed);
    }
    MPMCRing(const MPMCRing&)=delete;
    MPMCRing& operator=(const MPMCRing&)=delete;
    bool try_push(T value) {
        auto pos=enqueue_pos_.load(std::memory_order_relaxed);
        Slot* cell;
        for(;;){
            cell=&slots_[pos & mask_];
            const auto seq=cell->sequence.load(std::memory_order_acquire);
            const auto diff=static_cast<std::int64_t>(seq)-static_cast<std::int64_t>(pos);
            if(diff==0){
                if(enqueue_pos_.compare_exchange_weak(pos,pos+1,std::memory_order_relaxed))break;
            } else if(diff<0)return false;
            else pos=enqueue_pos_.load(std::memory_order_relaxed);
        }
        cell->data=std::move(value);
        cell->sequence.store(pos+1,std::memory_order_release);
        return true;
    }
    bool try_pop(T& out) {
        auto pos=dequeue_pos_.load(std::memory_order_relaxed);
        Slot* cell;
        for(;;){
            cell=&slots_[pos & mask_];
            const auto seq=cell->sequence.load(std::memory_order_acquire);
            const auto diff=static_cast<std::int64_t>(seq)-static_cast<std::int64_t>(pos+1);
            if(diff==0){
                if(dequeue_pos_.compare_exchange_weak(pos,pos+1,std::memory_order_relaxed))break;
            } else if(diff<0)return false;
            else pos=dequeue_pos_.load(std::memory_order_relaxed);
        }
        out=std::move(cell->data);
        cell->sequence.store(pos+cap_,std::memory_order_release);
        return true;
    }
};
}
