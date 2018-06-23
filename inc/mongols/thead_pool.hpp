#ifndef THEAD_POOL_HPP
#define THEAD_POOL_HPP

#include <thread>
#include <functional>
#include <vector>
#include <atomic>
#include <iostream>

#include "safe_queue.hpp"


namespace mongols {

    class join_thread {
    private:
        std::vector<std::thread>& th;
    public:

        join_thread(std::vector<std::thread>& th) : th(th) {
        }

        virtual~join_thread() {
            for (auto& i : this->th) {
                if (i.joinable()) {
                    i.join();
                }
            }
        }

    };

    class thread_pool {
    private:
        std::atomic_bool done;
        safe_queue<std::function<bool() >> q;
        std::vector<std::thread> th;
        join_thread joiner;

        void work() {
            while (!this->done) {
                std::function<bool() > task;
                this->q.wait_and_pop(task);
                if (task()) {
                    break;
                };
                std::this_thread::yield();
            }
        }
    public:

        thread_pool(size_t th_size = std::thread::hardware_concurrency()) : done(false), q(), th(), joiner(th) {
            try {
                for (size_t i = 0; i < th_size; ++i) {
                    this->th.push_back(std::move(std::thread(std::bind(&thread_pool::work, this))));
                }
            } catch (...) {
                this->done = true;
            }
        }

        virtual~thread_pool() {
            this->done = true;
            std::function<bool() > task;
            while (this->q.try_pop(task)) {
                task();
            }
        }

        template<typename F>
        void submit(F f) {
            this->q.push(f);
        }

        size_t size()const {
            return this->th.size();
        }

    };
}

#endif /* THEAD_POOL_HPP */

