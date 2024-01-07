#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std;

namespace Common {
    template <typename T, size_t N>
    requires requires { (N & (N - 1)) == 0; }
    class LFQueue final {
    public:
        LFQueue() : store_(N, T()) {}

        void push(T element) {
            while(full()) {
                ;
            }
            auto idx = next_write_index_.fetch_add(1, memory_order_seq_cst);
            store_[idx & (N - 1)] = std::forward<T>(element);
            num_elements_.fetch_add(1, memory_order_seq_cst);
        }


        T front() {
            auto expected = num_elements_.load(memory_order_seq_cst);
            while (expected > 0 &&
                   !num_elements_.compare_exchange_strong(
                           expected, expected - 1, memory_order_seq_cst)) {
                ;
            }
            if (expected == 0) {
                return nullptr;
            }
            auto ri = next_read_index_.fetch_add(1, memory_order_seq_cst);
            return store_[ri & (N - 1)];
        }

        auto size() const noexcept { return num_elements_.load(); }

        bool empty() const noexcept { return num_elements_ == 0; }

        bool full() const noexcept { return num_elements_ >= N-10; }
        /*
            I meet some bugs when the queue is full.
            Due to my limited knowledge of computer architecture, I can't locate the bug.
            Please give us some issues if you have any idea.
        */
        // Deleted copy & move constructors and assignment-operators.

        LFQueue(const LFQueue &) = delete;

        LFQueue(const LFQueue &&) = delete;

        LFQueue &operator=(const LFQueue &) = delete;

        LFQueue &operator=(const LFQueue &&) = delete;

    private:
        std::vector<T> store_;
        alignas(64) std::atomic<size_t> next_read_index_ = {0};
        alignas(64) std::atomic<size_t> next_write_index_ = {0};
        alignas(64) std::atomic<size_t> num_elements_ = {0};
    };
} // namespace Common
template<int N = 1024>
class ThreadPool {
public:
    ThreadPool(int num_threads) : stop(false) {
        for (int i = 0; i < num_threads; i++) {
            workers.emplace_back([this]() {
                while (true) {
                    function<void()> f;
                    {
                        while (q.empty() && !stop) {
                            ;
                        }
                        if (stop && q.empty())
                            return;
                        f = std::move(q.front());
                    }
                    if (f) {
                        f();
                    }
                }
            });
        }
    }

    template <typename F, typename... Args> auto enqueue(F &&f, Args &&...args) {
        using return_type =
                decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        auto task = make_shared<packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> res = task->get_future();
        {
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            q.push([task = std::move(task)] { (*task)(); });
        }
        return res;
    }
    ~ThreadPool() {
        stop = true;
        for (auto &th : workers) {
            th.join();
        }
    }

private:
    bool stop;
    Common::LFQueue<function<void()>, N> q;
    vector<thread> workers;
};
#endif