#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>



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

   
    void Producer(int id) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            queued_tags.push_back({id});
        }

        queue_cv.notify_one();
    }


    void Consumer(std::atomic<int>& processed_count) {
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex);

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

    std::thread consumer([&]() { queue.Consumer(processed_count); });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    queue.Producer(1);
    queue.Producer(2);
    queue.Producer(3);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(processed_count, 3);
    {
        std::lock_guard<std::mutex> lock(queue.queue_mutex);
        queue.running = false;
    }
    queue.queue_cv.notify_all();

    if (consumer.joinable()) consumer.join();
}