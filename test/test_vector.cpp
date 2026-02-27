#include <velk/vector.h>

#include <gtest/gtest.h>

#include <string>

using namespace velk;

// Tracks construction/destruction to verify proper lifetime management.
struct Tracked
{
    static int alive;
    int value;

    Tracked() : value(0) { ++alive; }
    explicit Tracked(int v) : value(v) { ++alive; }
    Tracked(const Tracked& o) : value(o.value) { ++alive; }
    Tracked(Tracked&& o) noexcept : value(o.value) { o.value = -1; ++alive; }
    Tracked& operator=(const Tracked& o) { value = o.value; return *this; }
    Tracked& operator=(Tracked&& o) noexcept { value = o.value; o.value = -1; return *this; }
    ~Tracked() { --alive; }
    bool operator==(const Tracked& o) const { return value == o.value; }
};
int Tracked::alive = 0;

// Construction

TEST(Vector, DefaultConstructEmpty)
{
    vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.data(), nullptr);
}

TEST(Vector, CountConstruct)
{
    vector<int> v(5);
    EXPECT_EQ(v.size(), 5u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 0);
    }
}

TEST(Vector, CountValueConstruct)
{
    vector<int> v(3, 42);
    EXPECT_EQ(v.size(), 3u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(Vector, PointerRangeConstruct)
{
    int data[] = {10, 20, 30};
    vector<int> v(data, data + 3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[1], 20);
    EXPECT_EQ(v[2], 30);
}

TEST(Vector, InitializerListConstruct)
{
    vector<int> v{1, 2, 3, 4};
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[3], 4);
}

TEST(Vector, ArrayViewConstruct)
{
    int data[] = {5, 6, 7};
    array_view<int> view(data, 3);
    vector<int> v(view);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 5);
    EXPECT_EQ(v[2], 7);
}

TEST(Vector, ZeroCountConstruct)
{
    vector<int> v(0);
    EXPECT_TRUE(v.empty());
}

// Copy/move

TEST(Vector, CopyConstruct)
{
    vector<int> a{1, 2, 3};
    vector<int> b(a);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[2], 3);
    // Verify independent storage.
    b[0] = 99;
    EXPECT_EQ(a[0], 1);
}

TEST(Vector, CopyConstructTightCapacity)
{
    vector<int> a;
    a.reserve(100);
    a.push_back(1);
    a.push_back(2);
    vector<int> b(a);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b.capacity(), 2u);
}

TEST(Vector, MoveConstruct)
{
    vector<int> a{1, 2, 3};
    auto* old_data = a.data();
    vector<int> b(std::move(a));
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(a.data(), nullptr);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b.data(), old_data);
}

TEST(Vector, CopyAssign)
{
    vector<int> a{1, 2};
    vector<int> b{3, 4, 5};
    b = a;
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 1);
}

TEST(Vector, MoveAssign)
{
    vector<int> a{1, 2};
    vector<int> b{3, 4, 5};
    b = std::move(a);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 1);
}

TEST(Vector, SelfCopyAssign)
{
    vector<int> v{1, 2, 3};
    v = v;
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
}

TEST(Vector, SelfMoveAssign)
{
    vector<int> v{1, 2, 3};
    v = std::move(v);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
}

// Element access

TEST(Vector, FrontBack)
{
    vector<int> v{10, 20, 30};
    EXPECT_EQ(v.front(), 10);
    EXPECT_EQ(v.back(), 30);
    v.front() = 99;
    v.back() = 88;
    EXPECT_EQ(v[0], 99);
    EXPECT_EQ(v[2], 88);
}

TEST(Vector, DataPointer)
{
    vector<int> v{1, 2};
    EXPECT_EQ(v.data()[0], 1);
    const auto& cv = v;
    EXPECT_EQ(cv.data()[1], 2);
}

// Iterators

TEST(Vector, BeginEnd)
{
    vector<int> v{1, 2, 3};
    int sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 6);
}

TEST(Vector, RangeFor)
{
    vector<int> v{10, 20, 30};
    int sum = 0;
    for (int x : v) {
        sum += x;
    }
    EXPECT_EQ(sum, 60);
}

// Capacity

TEST(Vector, Reserve)
{
    vector<int> v;
    v.reserve(100);
    EXPECT_GE(v.capacity(), 100u);
    EXPECT_TRUE(v.empty());
}

TEST(Vector, ReserveSmallerNoop)
{
    vector<int> v{1, 2, 3};
    size_t cap = v.capacity();
    v.reserve(1);
    EXPECT_EQ(v.capacity(), cap);
}

TEST(Vector, ShrinkToFit)
{
    vector<int> v;
    v.reserve(100);
    v.push_back(1);
    v.push_back(2);
    v.shrink_to_fit();
    EXPECT_EQ(v.capacity(), 2u);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

TEST(Vector, ShrinkToFitEmpty)
{
    vector<int> v;
    v.reserve(10);
    v.shrink_to_fit();
    EXPECT_EQ(v.capacity(), 0u);
    EXPECT_EQ(v.data(), nullptr);
}

// Modifiers

TEST(Vector, PushBackCopy)
{
    vector<int> v;
    int x = 42;
    v.push_back(x);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 42);
}

TEST(Vector, PushBackMove)
{
    vector<std::string> v;
    std::string s = "hello";
    v.push_back(std::move(s));
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], "hello");
}

TEST(Vector, PushBackSelfReference)
{
    vector<int> v{1, 2, 3};
    v.push_back(v[0]);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[3], 1);
}

TEST(Vector, EmplaceBack)
{
    vector<Tracked> v;
    Tracked::alive = 0;
    auto& ref = v.emplace_back(42);
    EXPECT_EQ(ref.value, 42);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].value, 42);
}

TEST(Vector, PopBack)
{
    Tracked::alive = 0;
    {
        vector<Tracked> v;
        v.emplace_back(1);
        v.emplace_back(2);
        EXPECT_EQ(Tracked::alive, 2);
        v.pop_back();
        EXPECT_EQ(v.size(), 1u);
        EXPECT_EQ(Tracked::alive, 1);
    }
    EXPECT_EQ(Tracked::alive, 0);
}

TEST(Vector, Clear)
{
    Tracked::alive = 0;
    {
        vector<Tracked> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        size_t cap = v.capacity();
        v.clear();
        EXPECT_TRUE(v.empty());
        EXPECT_EQ(v.capacity(), cap);
        EXPECT_EQ(Tracked::alive, 0);
    }
}

TEST(Vector, InsertSingle)
{
    vector<int> v{1, 3, 4};
    auto* p = v.insert(v.begin() + 1, 2);
    EXPECT_EQ(*p, 2);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
    EXPECT_EQ(v[3], 4);
}

TEST(Vector, InsertAtBegin)
{
    vector<int> v{2, 3};
    v.insert(v.begin(), 1);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

TEST(Vector, InsertAtEnd)
{
    vector<int> v{1, 2};
    v.insert(v.end(), 3);
    EXPECT_EQ(v[2], 3);
}

TEST(Vector, InsertRange)
{
    vector<int> v{1, 5};
    int data[] = {2, 3, 4};
    v.insert(v.begin() + 1, data, data + 3);
    EXPECT_EQ(v.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(v[i], i + 1);
    }
}

TEST(Vector, InsertRangeAtEnd)
{
    vector<int> v{1};
    int data[] = {2, 3};
    v.insert(v.end(), data, data + 2);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

TEST(Vector, InsertEmptyRange)
{
    vector<int> v{1, 2};
    int data[] = {9};
    v.insert(v.begin(), data, data);
    EXPECT_EQ(v.size(), 2u);
}

TEST(Vector, EraseSingle)
{
    vector<int> v{1, 2, 3, 4};
    auto* p = v.erase(v.begin() + 1);
    EXPECT_EQ(*p, 3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 3);
    EXPECT_EQ(v[2], 4);
}

TEST(Vector, EraseLast)
{
    vector<int> v{1, 2, 3};
    auto* p = v.erase(v.begin() + 2);
    EXPECT_EQ(p, v.end());
    EXPECT_EQ(v.size(), 2u);
}

TEST(Vector, EraseRange)
{
    vector<int> v{1, 2, 3, 4, 5};
    auto* p = v.erase(v.begin() + 1, v.begin() + 4);
    EXPECT_EQ(*p, 5);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 5);
}

TEST(Vector, EraseEmptyRange)
{
    vector<int> v{1, 2, 3};
    v.erase(v.begin() + 1, v.begin() + 1);
    EXPECT_EQ(v.size(), 3u);
}

TEST(Vector, ResizeGrow)
{
    vector<int> v{1, 2};
    v.resize(5);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 0);
}

TEST(Vector, ResizeShrink)
{
    Tracked::alive = 0;
    {
        vector<Tracked> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        v.resize(1);
        EXPECT_EQ(v.size(), 1u);
        EXPECT_EQ(Tracked::alive, 1);
    }
    EXPECT_EQ(Tracked::alive, 0);
}

TEST(Vector, ResizeWithValue)
{
    vector<int> v{1};
    v.resize(4, 7);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 7);
    EXPECT_EQ(v[2], 7);
    EXPECT_EQ(v[3], 7);
}

TEST(Vector, Swap)
{
    vector<int> a{1, 2};
    vector<int> b{3, 4, 5};
    a.swap(b);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_EQ(a[0], 3);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 1);
}

// Conversion

TEST(Vector, ImplicitArrayViewConversion)
{
    vector<int> v{1, 2, 3};
    array_view<int> view = v;
    EXPECT_EQ(view.size(), 3u);
    EXPECT_EQ(view[0], 1);
    EXPECT_EQ(view[2], 3);
    EXPECT_EQ(view.begin(), v.data());
}

// Comparison

TEST(Vector, Equality)
{
    vector<int> a{1, 2, 3};
    vector<int> b{1, 2, 3};
    vector<int> c{1, 2, 4};
    vector<int> d{1, 2};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(Vector, EmptyEquality)
{
    vector<int> a;
    vector<int> b;
    EXPECT_EQ(a, b);
}

// Growth

TEST(Vector, GrowthDoublesCapacity)
{
    vector<int> v;
    v.push_back(1);
    EXPECT_GE(v.capacity(), 8u);
    size_t cap = v.capacity();
    for (size_t i = 1; i <= cap; ++i) {
        v.push_back(static_cast<int>(i));
    }
    EXPECT_GE(v.capacity(), cap * 2);
}

// Tracked element lifecycle

TEST(Vector, TrackedDestructorOnDestroy)
{
    Tracked::alive = 0;
    {
        vector<Tracked> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        EXPECT_EQ(Tracked::alive, 3);
    }
    EXPECT_EQ(Tracked::alive, 0);
}

TEST(Vector, TrackedCopyConstruct)
{
    Tracked::alive = 0;
    {
        vector<Tracked> a;
        a.emplace_back(10);
        a.emplace_back(20);
        {
            vector<Tracked> b(a);
            EXPECT_EQ(Tracked::alive, 4);
            EXPECT_EQ(b[0].value, 10);
            EXPECT_EQ(b[1].value, 20);
        }
        EXPECT_EQ(Tracked::alive, 2);
    }
    EXPECT_EQ(Tracked::alive, 0);
}

TEST(Vector, TrackedMoveConstruct)
{
    Tracked::alive = 0;
    {
        vector<Tracked> a;
        a.emplace_back(10);
        a.emplace_back(20);
        vector<Tracked> b(std::move(a));
        EXPECT_EQ(Tracked::alive, 2);
        EXPECT_TRUE(a.empty());
    }
    EXPECT_EQ(Tracked::alive, 0);
}

// Non-trivial type (std::string)

TEST(Vector, StringPushAndAccess)
{
    vector<std::string> v;
    v.push_back("hello");
    v.push_back("world");
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], "hello");
    EXPECT_EQ(v[1], "world");
}

TEST(Vector, StringInsertAndErase)
{
    vector<std::string> v;
    v.push_back("a");
    v.push_back("c");
    v.insert(v.begin() + 1, "b");
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
    EXPECT_EQ(v[2], "c");
    v.erase(v.begin());
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], "b");
}

TEST(Vector, ManyPushBacks)
{
    vector<int> v;
    for (int i = 0; i < 1000; ++i) {
        v.push_back(i);
    }
    EXPECT_EQ(v.size(), 1000u);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(v[i], i);
    }
}
