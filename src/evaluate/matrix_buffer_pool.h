#pragma once
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

class MatrixBufferPool {
  public:
    static MatrixBufferPool &instance() {
        static MatrixBufferPool pool;
        return pool;
    }

    /// Acquire a buffer of at least count doubles from the pool. Reuses hot cached memory if available.
    std::shared_ptr<std::vector<double>> acquire(size_t count) {
        auto &free_list = pool_[count];
        if (!free_list.empty()) {
            auto buf = std::move(free_list.back());
            free_list.pop_back();
            return buf;
        }
        return std::make_shared<std::vector<double>>(count);
    }

    /// Recycle a buffer back into the pool for future matrix evaluations.
    void release(size_t count, std::shared_ptr<std::vector<double>> buf) {
        if (buf && buf.use_count() == 1) {
            pool_[count].push_back(std::move(buf));
        }
    }

    void clear() { pool_.clear(); }

  private:
    MatrixBufferPool() = default;
    std::unordered_map<size_t, std::vector<std::shared_ptr<std::vector<double>>>> pool_;
};
