#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_file_protocol.h>
#include <velk/interface/intf_file_system.h>

#include <gtest/gtest.h>
#include <cstdio>
#include <string>

using namespace velk;

namespace {

// Helper: write a temp file and return its path.
std::string write_temp_file(const char* name, const void* data, size_t size)
{
    std::string path = std::string(testing::TempDir()) + name;
    FILE* f = fopen(path.c_str(), "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
    }
    return path;
}

string_view sv(const std::string& s)
{
    return string_view(s.c_str(), s.size());
}

std::string file_uri(const std::string& path)
{
    return "file://" + path;
}

} // namespace

class FileSystemTest : public ::testing::Test
{
protected:
    IFileSystem& fs_ = ::velk::instance().file_system();
};

TEST_F(FileSystemTest, ReadTextFile)
{
    const char* content = "hello velk";
    auto uri = file_uri(write_temp_file("fs_test_text.txt", content, strlen(content)));

    ::velk::string out;
    auto rv = fs_.read_text_file(sv(uri), out);
    EXPECT_EQ(Success, rv);
    EXPECT_EQ(string_view("hello velk"), string_view(out));
}

TEST_F(FileSystemTest, ReadBinaryFile)
{
    uint8_t data[] = {0x00, 0x01, 0xFF, 0xFE, 0x42};
    auto uri = file_uri(write_temp_file("fs_test_bin.dat", data, sizeof(data)));

    ::velk::vector<uint8_t> out;
    auto rv = fs_.read_file(sv(uri), out);
    EXPECT_EQ(Success, rv);
    ASSERT_EQ(sizeof(data), out.size());
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(data[i], out[i]) << "mismatch at byte " << i;
    }
}

TEST_F(FileSystemTest, FileExists)
{
    auto uri = file_uri(write_temp_file("fs_test_exists.txt", "x", 1));
    EXPECT_TRUE(fs_.file_exists(sv(uri)));
}

TEST_F(FileSystemTest, FileDoesNotExist)
{
    EXPECT_FALSE(fs_.file_exists(string_view("file://nonexistent_path_12345.txt")));
}

TEST_F(FileSystemTest, FileSize)
{
    const char* content = "twelve chars";
    auto uri = file_uri(write_temp_file("fs_test_size.txt", content, strlen(content)));

    auto size = fs_.file_size(sv(uri));
    EXPECT_EQ(static_cast<int64_t>(strlen(content)), size);
}

TEST_F(FileSystemTest, FileSizeNonExistent)
{
    EXPECT_EQ(-1, fs_.file_size(string_view("file://nonexistent_path_67890.txt")));
}

TEST_F(FileSystemTest, UnknownScheme)
{
    ::velk::string out;
    EXPECT_EQ(InvalidArgument, fs_.read_text_file(string_view("unknown://something"), out));
}

TEST_F(FileSystemTest, MissingScheme)
{
    ::velk::string out;
    EXPECT_EQ(InvalidArgument, fs_.read_text_file(string_view("no_scheme_here"), out));
}

TEST_F(FileSystemTest, ReadNonExistentFile)
{
    ::velk::string out;
    auto rv = fs_.read_text_file(string_view("file://does_not_exist_abc.txt"), out);
    EXPECT_TRUE(failed(rv));
}

TEST_F(FileSystemTest, ReadEmptyFile)
{
    auto uri = file_uri(write_temp_file("fs_test_empty.txt", "", 0));

    ::velk::string out;
    auto rv = fs_.read_text_file(sv(uri), out);
    EXPECT_EQ(Success, rv);
    EXPECT_EQ(0u, out.size());
}

// Custom protocol for testing discovery.
class MockProtocol : public ext::ObjectCore<MockProtocol, IFileProtocol>
{
public:
    VELK_CLASS_UID("f17e5000-0002-4000-8000-000000000099", "MockProtocol");

    string_view scheme() const override { return "mock"; }

    ReturnValue read_file(string_view, ::velk::vector<uint8_t>& out) const override
    {
        out.push_back(0xAA);
        out.push_back(0xBB);
        return Success;
    }

    ReturnValue read_text_file(string_view, ::velk::string& out) const override
    {
        out = ::velk::string("mock data");
        return Success;
    }

    bool file_exists(string_view) const override { return true; }
    int64_t file_size(string_view) const override { return 42; }
};

TEST(FileSystemCustomProtocol, DiscoversMockProtocol)
{
    auto& v = ::velk::instance();

    // Register the mock protocol.
    v.type_registry().register_type(MockProtocol::get_factory());

    auto& fs = v.file_system();

    ::velk::string out;
    auto rv = fs.read_text_file(string_view("mock://anything"), out);
    EXPECT_EQ(Success, rv);
    EXPECT_EQ(string_view("mock data"), string_view(out));

    EXPECT_TRUE(fs.file_exists(string_view("mock://test")));
    EXPECT_EQ(42, fs.file_size(string_view("mock://test")));

    // Cleanup.
    v.type_registry().unregister_type(MockProtocol::get_factory());
}
