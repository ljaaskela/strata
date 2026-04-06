#include <velk/api/perf.h>
#include <velk/ext/core_object.h>

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace velk;

struct PerfRecord
{
    uint64_t key;
    std::string label;
    Duration elapsed;
};

class TestPerfSink : public ext::ObjectCore<TestPerfSink, IPerfSink>
{
public:
    void write_perf(uint64_t key, string_view label, Duration elapsed) override
    {
        records.push_back({key, std::string(label.data(), label.size()), elapsed});
    }

    std::vector<PerfRecord> records;
};

class PerfTest : public ::testing::Test
{
protected:
    IVelk& velk_ = instance();
    IPerfSink::Ptr sink_ = ext::make_object<TestPerfSink, IPerfSink>();
    TestPerfSink* ts_ = static_cast<TestPerfSink*>(sink_.get());

    void SetUp() override { velk_.perf_log().set_perf_sink(sink_); }

    void TearDown() override { velk_.perf_log().set_perf_sink({}); }
};

TEST_F(PerfTest, StartEndRecordsTiming)
{
    constexpr uint64_t key = make_hash64("test_region");
    velk_.perf_log().start_perf(key, "test_region");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto elapsed = velk_.perf_log().end_perf(key);

    EXPECT_GT(elapsed.to_microseconds(), 0);
    ASSERT_EQ(1u, ts_->records.size());
    EXPECT_EQ(key, ts_->records[0].key);
    EXPECT_EQ("test_region", ts_->records[0].label);
    EXPECT_GE(ts_->records[0].elapsed.to_milliseconds(), 1);
    EXPECT_EQ(elapsed.to_microseconds(), ts_->records[0].elapsed.to_microseconds());
}

TEST_F(PerfTest, PerfScopeRAII)
{
    {
        PerfScope ps("scoped_region");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_EQ(1u, ts_->records.size());
    EXPECT_EQ(make_hash64("scoped_region"), ts_->records[0].key);
    EXPECT_EQ("scoped_region", ts_->records[0].label);
    EXPECT_GE(ts_->records[0].elapsed.to_microseconds(), 1);
}

TEST_F(PerfTest, PerfScopeElapsed)
{
    PerfScope ps("elapsed_check");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto mid = ps.elapsed();
    EXPECT_GE(mid.to_milliseconds(), 1);
}

TEST_F(PerfTest, GetPerfRunningRegion)
{
    constexpr uint64_t key = make_hash64("running");
    velk_.perf_log().start_perf(key, "running");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    auto elapsed = velk_.perf_log().get_perf(key);
    EXPECT_GE(elapsed.to_milliseconds(), 1);

    velk_.perf_log().end_perf(key);
}

TEST_F(PerfTest, GetPerfUnknownKeyReturnsZero)
{
    auto elapsed = velk_.perf_log().get_perf(12345);
    EXPECT_EQ(0, elapsed.to_microseconds());
}

TEST_F(PerfTest, NoSinkNoCrash)
{
    velk_.perf_log().set_perf_sink({});
    constexpr uint64_t key = make_hash64("no_sink");
    velk_.perf_log().start_perf(key, "no_sink");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto elapsed = velk_.perf_log().end_perf(key);
    EXPECT_GE(elapsed.to_milliseconds(), 1);
}

TEST_F(PerfTest, EndPerfUnknownKeyReturnsZero)
{
    auto elapsed = velk_.perf_log().end_perf(99999);
    EXPECT_EQ(0, elapsed.to_microseconds());
    EXPECT_EQ(0u, ts_->records.size());
}

TEST_F(PerfTest, RestartSameKeyResetsTimer)
{
    constexpr uint64_t key = make_hash64("restart");
    velk_.perf_log().start_perf(key, "restart");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Elapsed should be >= 10ms
    auto elapsed = velk_.perf_log().get_perf(key);
    EXPECT_GE(elapsed.to_milliseconds(), 10);

    // Restart with the same key resets the timer
    velk_.perf_log().start_perf(key, "restart");
    elapsed = velk_.perf_log().end_perf(key);

    // Elapsed should be close to zero (much less than 10ms)
    EXPECT_LT(elapsed.to_milliseconds(), 5.f);
    ASSERT_EQ(1u, ts_->records.size());
}

TEST_F(PerfTest, MakeHash64Deterministic)
{
    constexpr auto h1 = make_hash64("hello");
    constexpr auto h2 = make_hash64("hello");
    constexpr auto h3 = make_hash64("world");

    EXPECT_EQ(h1, h2);
    EXPECT_NE(h1, h3);
    EXPECT_NE(h1, 0u);
    EXPECT_NE(h3, 0u);
}

TEST_F(PerfTest, PerfScopeWithPrecomputedKey)
{
    constexpr uint64_t key = make_hash64("precomputed");
    {
        PerfScope ps(key);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_EQ(1u, ts_->records.size());
    EXPECT_EQ(key, ts_->records[0].key);
    EXPECT_GT(ts_->records[0].elapsed.to_microseconds(), 0);
}
