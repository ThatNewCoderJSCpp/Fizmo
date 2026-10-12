#include "fizmo_library.hpp"
#include "strand.hpp"

namespace fizmo {
namespace networking {
namespace async {

namespace {

struct StrandScope {
    const void*  impl;
    StrandScope* prev;
};

thread_local StrandScope* t_strand = nullptr;

} // namespace

Strand::Strand(IOContext& ctx) : m_impl(std::make_shared<Impl>(ctx)) {}

bool Strand::running_in_this_thread() const noexcept {
    for (StrandScope* s = t_strand; s; s = s->prev) {
        if (s->impl == m_impl.get()) return true;
    }

    return false;
}

void Strand::enqueue(const std::shared_ptr<Impl>& impl, UniqueFunction<void()> fn) {
    bool schedule = false;

    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->queue.push_back(std::move(fn));

        if (!impl->scheduled) {
            impl->scheduled = true;
            schedule = true;
        }
    }

    if (schedule) {
        auto keep = impl;
        impl->ctx.post([keep]() { drain(keep); });
    }
}

void Strand::drain(const std::shared_ptr<Impl>& impl) {
    StrandScope scope{ impl.get(), t_strand };
    t_strand = &scope;

    struct Restore {
        StrandScope* prev;
        const std::shared_ptr<Impl>& impl;
        bool done = false;

        ~Restore() {
            t_strand = prev;

            if (!done) {
                bool reschedule = false;

                {
                    std::lock_guard<std::mutex> lock(impl->mutex);
                    if (impl->queue.empty()) impl->scheduled = false;
                    else reschedule = true;
                }

                if (reschedule) {
                    auto keep = impl;
                    impl->ctx.post([keep]() { drain(keep); });
                }
            }
        }
    } restore{ scope.prev, impl };

    for (int budget = 0; budget < 64; ++budget) {
        UniqueFunction<void()> fn;

        {
            std::lock_guard<std::mutex> lock(impl->mutex);

            if (impl->queue.empty()) {
                impl->scheduled = false;
                restore.done = true;
                return;
            }

            fn = std::move(impl->queue.front());
            impl->queue.pop_front();
        }

        fn();
    }
}

} // namespace async
} // namespace networking
} // namespace fizmo
