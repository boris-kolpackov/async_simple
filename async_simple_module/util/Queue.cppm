/*
 * Copyright (c) 2022, Alibaba Group Holding Limited;
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// The file implements a simple thread safe queue.
export module async_simple:util.Queue;
import std;

namespace async_simple::util {

export template <typename T>
class Queue {
public:
    Queue() = default;

    void push(T &&item) {
        {
            std::scoped_lock<std::mutex> guard(_mutex);
            _queue.push(std::move(item));
        }
        _cond.notify_one();
    }

    bool try_push(const T &item) {
        {
            std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock/*_in_modules*/);
            if (!lock)
                return false;
            _queue.push(item);
        }
        _cond.notify_one();
        return true;
    }

    bool pop(T &item) {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [&]() { return !_queue.empty() || _stop; });
        if (_queue.empty())
            return false;
        item = std::move(_queue.front());
        _queue.pop();
        return true;
    }

    // non-blocking pop an item, maybe pop failed.
    // predict is an extension pop condition, default is null.
    bool try_pop(T &item, bool (*predict)(T &) = nullptr) {
        std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock/*_in_modules*/);
        if (!lock || _queue.empty())
            return false;

        if (predict && predict(_queue.front())) {
            return false;
        }

        item = std::move(_queue.front());
        _queue.pop();
        return true;
    }

    std::size_t size() const {
        std::scoped_lock<std::mutex> guard(_mutex);
        return _queue.size();
    }

    bool empty() const {
        std::scoped_lock<std::mutex> guard(_mutex);
        return _queue.empty();
    }

    void stop() {
        {
            std::scoped_lock<std::mutex> guard(_mutex);
            _stop = true;
        }
        _cond.notify_all();
    }

private:
    std::queue<T> _queue;
    bool _stop = false;
    mutable std::mutex _mutex;
    std::condition_variable _cond;
};
}  // namespace async_simple::util
