#include <velk/string.h>

#include <gtest/gtest.h>

#include <sstream>

using namespace velk;

// Construction

TEST(String, DefaultConstructEmpty)
{
    string s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_NE(s.data(), nullptr);
    EXPECT_STREQ(s.c_str(), "");
    EXPECT_EQ(s.capacity(), string::sso_capacity);
}

TEST(String, ConstructFromCString)
{
    string s("hello");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, ConstructFromNullCString)
{
    string s(static_cast<const char*>(nullptr));
    EXPECT_TRUE(s.empty());
}

TEST(String, ConstructFromEmptyCString)
{
    string s("");
    EXPECT_TRUE(s.empty());
}

TEST(String, ConstructFromPointerAndSize)
{
    string s("hello world", 5);
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, ConstructFromStringView)
{
    string_view sv("hello");
    string s(sv);
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, ConstructFromEmptyStringView)
{
    string_view sv;
    string s(sv);
    EXPECT_TRUE(s.empty());
}

TEST(String, ConstructWithCountAndChar)
{
    string s(5, 'x');
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "xxxxx");
}

TEST(String, ConstructWithZeroCount)
{
    string s(0, 'x');
    EXPECT_TRUE(s.empty());
}

// Copy/move

TEST(String, CopyConstruct)
{
    string a("hello");
    string b(a);
    EXPECT_STREQ(b.c_str(), "hello");
    // Verify independent storage.
    b[0] = 'H';
    EXPECT_EQ(a[0], 'h');
}

TEST(String, CopyConstructEmpty)
{
    string a;
    string b(a);
    EXPECT_TRUE(b.empty());
}

TEST(String, MoveConstruct)
{
    string a("hello");
    string b(std::move(a));
    EXPECT_TRUE(a.empty());
    EXPECT_STREQ(b.c_str(), "hello");
}

TEST(String, MoveConstructHeap)
{
    // Use a long string to force heap allocation.
    string a("this string is longer than 22 chars!");
    auto* old_data = a.data();
    string b(std::move(a));
    EXPECT_TRUE(a.empty());
    EXPECT_STREQ(b.c_str(), "this string is longer than 22 chars!");
    EXPECT_EQ(b.data(), old_data);
}

TEST(String, CopyAssign)
{
    string a("hello");
    string b("world!");
    b = a;
    EXPECT_STREQ(b.c_str(), "hello");
}

TEST(String, MoveAssign)
{
    string a("hello");
    string b("world!");
    b = std::move(a);
    EXPECT_TRUE(a.empty());
    EXPECT_STREQ(b.c_str(), "hello");
}

TEST(String, SelfCopyAssign)
{
    string s("hello");
    s = s;
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, SelfMoveAssign)
{
    string s("hello");
    s = std::move(s);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, AssignFromCString)
{
    string s("old");
    s = "new";
    EXPECT_STREQ(s.c_str(), "new");
}

TEST(String, AssignFromStringView)
{
    string s("old");
    s = string_view("new");
    EXPECT_STREQ(s.c_str(), "new");
}

// Element access

TEST(String, BracketAccess)
{
    string s("abc");
    EXPECT_EQ(s[0], 'a');
    EXPECT_EQ(s[1], 'b');
    EXPECT_EQ(s[2], 'c');
    s[0] = 'A';
    EXPECT_EQ(s[0], 'A');
}

TEST(String, FrontBack)
{
    string s("abc");
    EXPECT_EQ(s.front(), 'a');
    EXPECT_EQ(s.back(), 'c');
    s.front() = 'A';
    s.back() = 'C';
    EXPECT_STREQ(s.c_str(), "AbC");
}

TEST(String, DataPointer)
{
    string s("abc");
    EXPECT_EQ(s.data()[0], 'a');
    const auto& cs = s;
    EXPECT_EQ(cs.data()[2], 'c');
}

TEST(String, CStr)
{
    string s("hello");
    EXPECT_STREQ(s.c_str(), "hello");
    EXPECT_EQ(s.c_str()[s.size()], '\0');
}

// Iterators

TEST(String, BeginEnd)
{
    string s("abc");
    std::string result;
    for (auto it = s.begin(); it != s.end(); ++it) {
        result += *it;
    }
    EXPECT_EQ(result, "abc");
}

TEST(String, RangeFor)
{
    string s("hello");
    std::string result;
    for (char c : s) {
        result += c;
    }
    EXPECT_EQ(result, "hello");
}

// Capacity

TEST(String, Reserve)
{
    string s;
    s.reserve(100);
    EXPECT_GE(s.capacity(), 100u);
    EXPECT_TRUE(s.empty());
}

TEST(String, ReserveSmallerNoop)
{
    string s("hello");
    size_t cap = s.capacity();
    s.reserve(1);
    EXPECT_EQ(s.capacity(), cap);
}

TEST(String, ReservePreservesContent)
{
    string s("hello");
    s.reserve(100);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, ShrinkToFitToInline)
{
    string s;
    s.reserve(100);
    s.append("hi");
    EXPECT_GT(s.capacity(), string::sso_capacity);
    s.shrink_to_fit();
    EXPECT_EQ(s.capacity(), string::sso_capacity);
    EXPECT_STREQ(s.c_str(), "hi");
}

TEST(String, ShrinkToFitHeap)
{
    // String too long for inline stays on heap but shrinks.
    string s;
    s.reserve(200);
    s.append("this string is longer than 22 chars!");
    size_t len = s.size();
    s.shrink_to_fit();
    EXPECT_EQ(s.capacity(), len);
    EXPECT_STREQ(s.c_str(), "this string is longer than 22 chars!");
}

TEST(String, ShrinkToFitEmptyToInline)
{
    string s;
    s.reserve(100);
    s.shrink_to_fit();
    EXPECT_EQ(s.capacity(), string::sso_capacity);
    EXPECT_STREQ(s.c_str(), "");
}

TEST(String, ShrinkToFitInlineNoop)
{
    string s("hello");
    size_t cap = s.capacity();
    s.shrink_to_fit();
    EXPECT_EQ(s.capacity(), cap);
}

// Modifiers

TEST(String, PushBack)
{
    string s;
    s.push_back('a');
    s.push_back('b');
    s.push_back('c');
    EXPECT_STREQ(s.c_str(), "abc");
}

TEST(String, PopBack)
{
    string s("abc");
    s.pop_back();
    EXPECT_STREQ(s.c_str(), "ab");
    EXPECT_EQ(s.size(), 2u);
}

TEST(String, Clear)
{
    string s("hello");
    size_t cap = s.capacity();
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.capacity(), cap);
    EXPECT_STREQ(s.c_str(), "");
}

TEST(String, ClearEmpty)
{
    string s;
    s.clear();
    EXPECT_TRUE(s.empty());
}

TEST(String, AppendStringView)
{
    string s("hello");
    s.append(string_view(" world"));
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(String, AppendCString)
{
    string s("hello");
    s.append(" world");
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(String, AppendChars)
{
    string s("hi");
    s.append(3, '!');
    EXPECT_STREQ(s.c_str(), "hi!!!");
}

TEST(String, AppendEmpty)
{
    string s("hello");
    s.append(string_view());
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, PlusEqualsStringView)
{
    string s("hello");
    s += string_view(" world");
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(String, PlusEqualsChar)
{
    string s("abc");
    s += 'd';
    EXPECT_STREQ(s.c_str(), "abcd");
}

TEST(String, InsertMiddle)
{
    string s("helo");
    s.insert(2, string_view("l"));
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, InsertBegin)
{
    string s("world");
    s.insert(0, string_view("hello "));
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(String, InsertEnd)
{
    string s("hello");
    s.insert(5, string_view(" world"));
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(String, InsertEmpty)
{
    string s("hello");
    s.insert(2, string_view());
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, EraseMiddle)
{
    string s("hello world");
    s.erase(5, 6);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, EraseFromPos)
{
    string s("hello world");
    s.erase(5);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, EraseBeginning)
{
    string s("hello");
    s.erase(0, 2);
    EXPECT_STREQ(s.c_str(), "llo");
}

TEST(String, EraseCountExceedsSize)
{
    string s("hello");
    s.erase(3, 100);
    EXPECT_STREQ(s.c_str(), "hel");
}

TEST(String, ResizeGrow)
{
    string s("hi");
    s.resize(5, 'x');
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hixxx");
}

TEST(String, ResizeShrink)
{
    string s("hello");
    s.resize(2);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_STREQ(s.c_str(), "he");
}

TEST(String, ResizeDefault)
{
    string s("hi");
    s.resize(5);
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s[0], 'h');
    EXPECT_EQ(s[1], 'i');
    EXPECT_EQ(s[2], '\0');
}

TEST(String, Swap)
{
    string a("hello");
    string b("world!");
    a.swap(b);
    EXPECT_STREQ(a.c_str(), "world!");
    EXPECT_STREQ(b.c_str(), "hello");
}

TEST(String, SwapInlineAndHeap)
{
    string a("short");
    string b("this string is definitely longer than 22 characters");
    a.swap(b);
    EXPECT_STREQ(a.c_str(), "this string is definitely longer than 22 characters");
    EXPECT_STREQ(b.c_str(), "short");
}

// Substr

TEST(String, Substr)
{
    string s("hello world");
    string sub = s.substr(6, 5);
    EXPECT_STREQ(sub.c_str(), "world");
}

TEST(String, SubstrToEnd)
{
    string s("hello world");
    string sub = s.substr(6);
    EXPECT_STREQ(sub.c_str(), "world");
}

TEST(String, SubstrClamped)
{
    string s("hello");
    string sub = s.substr(3, 100);
    EXPECT_STREQ(sub.c_str(), "lo");
}

// Find/rfind

TEST(String, Find)
{
    string s("hello world hello");
    EXPECT_EQ(s.find(string_view("world")), 6u);
    EXPECT_EQ(s.find(string_view("hello")), 0u);
    EXPECT_EQ(s.find(string_view("hello"), 1), 12u);
    EXPECT_EQ(s.find(string_view("xyz")), string::npos);
}

TEST(String, Rfind)
{
    string s("hello world hello");
    EXPECT_EQ(s.rfind(string_view("hello")), 12u);
    EXPECT_EQ(s.rfind(string_view("world")), 6u);
    EXPECT_EQ(s.rfind(string_view("hello"), 5), 0u);
}

// Conversion

TEST(String, ImplicitStringViewConversion)
{
    string s("hello");
    string_view sv = s;
    EXPECT_EQ(sv.size(), 5u);
    EXPECT_EQ(sv, string_view("hello"));
    EXPECT_EQ(sv.data(), s.data());
}

// Comparison

TEST(String, EqualityString)
{
    string a("hello");
    string b("hello");
    string c("world");
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(String, EqualityStringView)
{
    string s("hello");
    EXPECT_EQ(s, string_view("hello"));
    EXPECT_NE(s, string_view("world"));
}

TEST(String, EqualityCString)
{
    string s("hello");
    EXPECT_TRUE(s == "hello");
    EXPECT_TRUE(s != "world");
}

TEST(String, EmptyEquality)
{
    string a;
    string b;
    EXPECT_EQ(a, b);
}

// Concatenation

TEST(String, ConcatStringAndView)
{
    string a("hello");
    string result = a + string_view(" world");
    EXPECT_STREQ(result.c_str(), "hello world");
}

TEST(String, ConcatViewAndString)
{
    string b(" world");
    string result = string_view("hello") + b;
    EXPECT_STREQ(result.c_str(), "hello world");
}

// Stream output

TEST(String, StreamOutput)
{
    string s("hello");
    std::ostringstream oss;
    oss << s;
    EXPECT_EQ(oss.str(), "hello");
}

// Growth

TEST(String, GrowthOnAppend)
{
    string s;
    for (int i = 0; i < 100; ++i) {
        s.push_back('a');
    }
    EXPECT_EQ(s.size(), 100u);
    for (size_t i = 0; i < s.size(); ++i) {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(String, NullTerminatedAfterAllOperations)
{
    string s("hello");
    EXPECT_EQ(s.c_str()[5], '\0');

    s.push_back('!');
    EXPECT_EQ(s.c_str()[6], '\0');

    s.pop_back();
    EXPECT_EQ(s.c_str()[5], '\0');

    s.append(" world");
    EXPECT_EQ(s.c_str()[s.size()], '\0');

    s.erase(5);
    EXPECT_EQ(s.c_str()[s.size()], '\0');

    s.insert(0, string_view("say "));
    EXPECT_EQ(s.c_str()[s.size()], '\0');

    s.resize(3);
    EXPECT_EQ(s.c_str()[s.size()], '\0');

    s.resize(10, 'z');
    EXPECT_EQ(s.c_str()[s.size()], '\0');

    s.clear();
    EXPECT_STREQ(s.c_str(), "");
}

// SSO-specific tests

TEST(String, SsoShortStringStaysInline)
{
    string s("hello");
    EXPECT_EQ(s.capacity(), string::sso_capacity);
    EXPECT_EQ(s.size(), 5u);
}

TEST(String, SsoMaxInlineLength)
{
    // Exactly sso_capacity characters should fit inline.
    string s(string::sso_capacity, 'x');
    EXPECT_EQ(s.size(), string::sso_capacity);
    EXPECT_EQ(s.capacity(), string::sso_capacity);
    EXPECT_EQ(s.c_str()[string::sso_capacity], '\0');
}

TEST(String, SsoOneOverInlineGoesToHeap)
{
    string s(string::sso_capacity + 1, 'x');
    EXPECT_EQ(s.size(), string::sso_capacity + 1);
    EXPECT_GT(s.capacity(), string::sso_capacity);
}

TEST(String, SsoGrowthTransitionsToHeap)
{
    string s;
    for (size_t i = 0; i <= string::sso_capacity; ++i) {
        s.push_back('a');
    }
    EXPECT_EQ(s.size(), string::sso_capacity + 1);
    EXPECT_GT(s.capacity(), string::sso_capacity);
    // Verify content survived the transition.
    for (size_t i = 0; i < s.size(); ++i) {
        EXPECT_EQ(s[i], 'a');
    }
}

TEST(String, SsoCopyInlineIsIndependent)
{
    string a("hello");
    string b(a);
    // Both should be inline with independent buffers.
    EXPECT_NE(a.data(), b.data());
    b[0] = 'H';
    EXPECT_EQ(a[0], 'h');
}

TEST(String, SsoSwapBothInline)
{
    string a("aaa");
    string b("bbbbb");
    a.swap(b);
    EXPECT_STREQ(a.c_str(), "bbbbb");
    EXPECT_STREQ(b.c_str(), "aaa");
}

TEST(String, SsoAppendWithinInline)
{
    string s("hello");
    s.append(" world");
    // "hello world" is 11 chars, still fits inline.
    EXPECT_EQ(s.capacity(), string::sso_capacity);
    EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(String, SsoAppendBeyondInline)
{
    string s("hello world hello wo");
    EXPECT_EQ(s.size(), 20u);
    EXPECT_EQ(s.capacity(), string::sso_capacity);
    s.append("rld");
    // 23 chars, beyond sso_capacity.
    EXPECT_EQ(s.size(), 23u);
    EXPECT_GT(s.capacity(), string::sso_capacity);
    EXPECT_STREQ(s.c_str(), "hello world hello world");
}

TEST(String, SsoInsertWithinInline)
{
    string s("helo");
    s.insert(2, string_view("l"));
    EXPECT_EQ(s.capacity(), string::sso_capacity);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, SsoEraseStaysInline)
{
    string s("hello world");
    s.erase(5);
    EXPECT_EQ(s.capacity(), string::sso_capacity);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(String, SsoMoveInline)
{
    string a("hello");
    string b(std::move(a));
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(a.capacity(), string::sso_capacity);
    EXPECT_STREQ(b.c_str(), "hello");
}
