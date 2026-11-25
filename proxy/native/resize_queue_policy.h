#pragma once
#include <atomic>
#include <cstdint>
namespace rnative::resizequeue {
struct Request {void* owner=nullptr;uint32_t width=0,height=0;};
class Queue {
    std::atomic<void*> owner_{nullptr};
    std::atomic<uint32_t> generation_{1};
    std::atomic<uint64_t> pending_{0};
public:
    bool Owns(void* owner) const {return owner && owner_.load(std::memory_order_acquire)==owner;}
    void Register(void* owner) {
        owner_.store(nullptr,std::memory_order_release);generation_.fetch_add(1,std::memory_order_acq_rel);
        pending_.store(0,std::memory_order_release);owner_.store(owner,std::memory_order_release);
    }
    void Cancel() {Register(nullptr);}
    bool Enqueue(void* owner,uint32_t width,uint32_t height) {
        if(!owner || width<32 || height<32 || width>65535 || height>65535)return false;
        uint32_t generation=generation_.load(std::memory_order_acquire);
        if(owner_.load(std::memory_order_acquire)!=owner)return false;
        uint64_t packet=(uint64_t(generation)<<32)|(uint64_t(width)<<16)|height;
        if(generation_.load(std::memory_order_acquire)!=generation)return false;
        uint64_t previous=pending_.load(std::memory_order_acquire);
        do {
            if(generation_.load(std::memory_order_acquire)!=generation || owner_.load(std::memory_order_acquire)!=owner)return false;
        } while(!pending_.compare_exchange_weak(previous,packet,std::memory_order_release,std::memory_order_acquire));
        return true;
    }
    Request Take(void* current) {
        uint64_t packet=pending_.exchange(0,std::memory_order_acq_rel);
        uint32_t generation=uint32_t(packet>>32);
        if(!packet || !current || generation!=generation_.load(std::memory_order_acquire) ||
           owner_.load(std::memory_order_acquire)!=current)return {};
        return {current,uint32_t(packet>>16)&65535u,uint32_t(packet)&65535u};
    }
};
} // namespace rnative::resizequeue
