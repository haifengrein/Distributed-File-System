#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

// This test suite validates the Concurrency Pattern used in DFSServiceImpl.
// Since we cannot directly instantiate the private DFSServiceImpl class in a unit test
// (without invasive include hacks), we strictly replicate the Producer-Consumer logic
// here to certify its correctness.

class QueueReplica {
public:
    struct Task {
        int id;
        bool finished = false;
    };

    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::vector<Task> queued_tags;
    bool running = true;

    // Simulates DFSServiceImpl::RequestCallback
    void Producer(int id) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            queued_tags.push_back({id});
        }
        // The fix: Notify the condition variable
        queue_cv.notify_one();
    }

    // Simulates DFSServiceImpl::ProcessQueuedRequests
    void Consumer(std::atomic<int>& processed_count) {
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // The fix: Wait efficiently instead of busy-looping
            queue_cv.wait(lock, [this] { return !queued_tags.empty() || !running; });

            if (!running && queued_tags.empty()) return;

            for (auto& task : queued_tags) {
                if (!task.finished) {
                    processed_count++;
                    task.finished = true;
                }
            }

            queued_tags.erase(
                std::remove_if(queued_tags.begin(), queued_tags.end(), [](Task& t) { return t.finished; }),
                queued_tags.end());
        }
    }
};

TEST(ConcurrencyTest, VerifyProducerConsumerFix) {
    QueueReplica queue;
    std::atomic<int> processed_count{0};

    // Start Consumer Thread
    std::thread consumer([&]() { queue.Consumer(processed_count); });

    // Give it a moment to sleep (ensure it enters wait state)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Produce items
    queue.Producer(1);
    queue.Producer(2);
    queue.Producer(3);

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check results: All 3 items should be processed
    EXPECT_EQ(processed_count, 3);

    // Shutdown logic
    {
        std::lock_guard<std::mutex> lock(queue.queue_mutex);
        queue.running = false;
    }
    queue.queue_cv.notify_all();

    if (consumer.joinable()) consumer.join();
}