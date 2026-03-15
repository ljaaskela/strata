#include <velk/api/hierarchy.h>
#include <velk/api/thread_context.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

using namespace velk;

class IThreadTestWidget : public Interface<IThreadTestWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 0.f),
        (PROP, float, height, 0.f)
    )
};

class ThreadTestWidget : public ext::Object<ThreadTestWidget, IThreadTestWidget>
{};

TEST(ThreadContext, CreateAndDestroy)
{
    auto ctx = create_thread_context();
    ASSERT_TRUE(ctx);
    EXPECT_NE(ctx.get(), nullptr);
}

TEST(ThreadContext, ReadLock)
{
    auto ctx = create_thread_context();
    {
        auto lock = ctx.read_lock();
        // Lock acquired; should be able to acquire another read lock concurrently
        auto lock2 = ctx.read_lock();
    }
}

TEST(ThreadContext, WriteLock)
{
    auto ctx = create_thread_context();
    {
        auto lock = ctx.write_lock();
        // Exclusive lock held
    }
}

TEST(ThreadContext, TryLock)
{
    auto ctx = create_thread_context();

    // try_lock should succeed when unlocked
    EXPECT_TRUE(ctx.get()->try_lock());
    ctx.get()->unlock();

    // try_lock_shared should succeed when unlocked
    EXPECT_TRUE(ctx.get()->try_lock_shared());
    ctx.get()->unlock_shared();

    // try_lock should fail when a write lock is held
    {
        auto lock = ctx.write_lock();
        EXPECT_FALSE(ctx.get()->try_lock());
        EXPECT_FALSE(ctx.get()->try_lock_shared());
    }

    // try_lock should fail when a read lock is held, but try_lock_shared should succeed
    {
        auto lock = ctx.read_lock();
        EXPECT_FALSE(ctx.get()->try_lock());
        EXPECT_TRUE(ctx.get()->try_lock_shared());
        ctx.get()->unlock_shared();
    }
}

TEST(ThreadContext, ConcurrentReaders)
{
    auto ctx = create_thread_context();
    std::atomic<int> active_readers{0};
    std::atomic<bool> saw_concurrent{false};
    constexpr int num_threads = 4;
    constexpr int iterations = 1000;

    auto reader = [&]() {
        for (int i = 0; i < iterations; ++i) {
            auto lock = ctx.read_lock();
            int count = ++active_readers;
            if (count > 1) {
                saw_concurrent.store(true, std::memory_order_relaxed);
            }
            std::this_thread::yield();
            --active_readers;
        }
    };

    std::thread threads[num_threads];
    for (auto& t : threads) {
        t = std::thread(reader);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(saw_concurrent.load()) << "Expected concurrent readers";
}

TEST(ThreadContext, WriterExclusion)
{
    auto ctx = create_thread_context();
    std::atomic<int> active_writers{0};
    std::atomic<bool> saw_overlap{false};
    constexpr int num_threads = 4;
    constexpr int iterations = 500;

    auto writer = [&]() {
        for (int i = 0; i < iterations; ++i) {
            auto lock = ctx.write_lock();
            int count = ++active_writers;
            if (count > 1) {
                saw_overlap.store(true, std::memory_order_relaxed);
            }
            std::this_thread::yield();
            --active_writers;
        }
    };

    std::thread threads[num_threads];
    for (auto& t : threads) {
        t = std::thread(writer);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(saw_overlap.load()) << "Writers must be mutually exclusive";
}

TEST(ThreadContext, ReaderWriterExclusion)
{
    instance().type_registry().register_type<ThreadTestWidget>();

    auto ctx = create_thread_context();
    auto obj = instance().create<IThreadTestWidget>(ThreadTestWidget::class_id());

    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};

    // Writer thread: sets width and height to the same value
    auto writer = std::thread([&]() {
        float v = 0.f;
        while (!stop.load(std::memory_order_relaxed)) {
            auto lock = ctx.write_lock();
            v += 1.f;
            obj->width().set_value(v);
            obj->height().set_value(v);
            ++write_count;
        }
    });

    // Reader threads: read width and height, verify they're consistent
    auto reader = [&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            auto lock = ctx.read_lock();
            float w = obj->width().get_value();
            float h = obj->height().get_value();
            EXPECT_EQ(w, h) << "Read inconsistent state: width=" << w << " height=" << h;
            ++read_count;
        }
    };

    std::thread readers[3];
    for (auto& t : readers) {
        t = std::thread(reader);
    }

    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true, std::memory_order_relaxed);

    writer.join();
    for (auto& t : readers) {
        t.join();
    }

    EXPECT_GT(read_count.load(), 0);
    EXPECT_GT(write_count.load(), 0);
}

TEST(ThreadContext, HierarchyAttachment)
{
    auto h = create_hierarchy();
    ASSERT_TRUE(h);

    // Initially no thread context
    EXPECT_FALSE(h.thread_context());

    // Attach one
    auto ctx = create_thread_context();
    auto raw = ctx.get();
    h.set_thread_context(ctx);

    // Retrieve it
    auto retrieved = h.thread_context();
    ASSERT_TRUE(retrieved);
    EXPECT_EQ(retrieved.get(), raw);

    // Can lock through the retrieved context
    {
        auto lock = retrieved.read_lock();
    }
    {
        auto lock = retrieved.write_lock();
    }

    // Replace with a new one
    auto ctx2 = create_thread_context();
    auto raw2 = ctx2.get();
    h.set_thread_context(ctx2);

    auto retrieved2 = h.thread_context();
    ASSERT_TRUE(retrieved2);
    EXPECT_EQ(retrieved2.get(), raw2);
    EXPECT_NE(retrieved2.get(), raw);
}
