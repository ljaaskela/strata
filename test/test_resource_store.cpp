#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/interface/resource/intf_resource.h>
#include <velk/interface/resource/intf_resource_protocol.h>
#include <velk/interface/resource/intf_resource_store.h>

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

using namespace velk;

namespace {

FILE* open_file(const char* path, const char* mode)
{
#ifdef _WIN32
    FILE* f = nullptr;
    fopen_s(&f, path, mode);
    return f;
#else
    return fopen(path, mode);
#endif
}

::velk::string write_temp_file(const char* name, const void* data, size_t size)
{
    auto tmp = testing::TempDir();
    ::velk::string path(tmp.c_str(), tmp.size());
    path.append(name);

    FILE* f = open_file(path.c_str(), "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
    }
    return path;
}

::velk::string file_uri(const ::velk::string& path)
{
    return "file://" + path;
}

} // namespace

class ResourceStoreTest : public ::testing::Test
{
protected:
    IResourceStore& store_ = ::velk::instance().resource_store();
};

TEST_F(ResourceStoreTest, ReadTextFile)
{
    const char* content = "hello velk";
    auto uri = file_uri(write_temp_file("rs_test_text.txt", content, strlen(content)));

    auto res = store_.get_resource(uri);
    ASSERT_TRUE(res);

    auto* f = interface_cast<IFile>(res);
    ASSERT_NE(nullptr, f);

    ::velk::string out;
    auto rv = f->read_text(out);
    EXPECT_EQ(Success, rv);
    EXPECT_EQ(string_view("hello velk"), string_view(out));
}

TEST_F(ResourceStoreTest, ReadBinaryFile)
{
    uint8_t data[] = {0x00, 0x01, 0xFF, 0xFE, 0x42};
    auto uri = file_uri(write_temp_file("rs_test_bin.dat", data, sizeof(data)));

    auto res = store_.get_resource(uri);
    ASSERT_TRUE(res);

    auto* f = interface_cast<IFile>(res);
    ASSERT_NE(nullptr, f);

    ::velk::vector<uint8_t> out;
    auto rv = f->read(out);
    EXPECT_EQ(Success, rv);
    ASSERT_EQ(sizeof(data), out.size());
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(data[i], out[i]) << "mismatch at byte " << i;
    }
}

TEST_F(ResourceStoreTest, ResourceExists)
{
    auto uri = file_uri(write_temp_file("rs_test_exists.txt", "x", 1));
    auto res = store_.get_resource(uri);
    ASSERT_TRUE(res);
    EXPECT_TRUE(res->exists());
}

TEST_F(ResourceStoreTest, ResourceDoesNotExist)
{
    auto res = store_.get_resource(string_view("file://nonexistent_path_12345.txt"));
    ASSERT_TRUE(res);
    EXPECT_FALSE(res->exists());
}

TEST_F(ResourceStoreTest, ResourceSize)
{
    const char* content = "twelve chars";
    auto uri = file_uri(write_temp_file("rs_test_size.txt", content, strlen(content)));

    auto res = store_.get_resource(uri);
    ASSERT_TRUE(res);
    EXPECT_EQ(static_cast<int64_t>(strlen(content)), res->size());
}

TEST_F(ResourceStoreTest, ResourceSizeNonExistent)
{
    auto res = store_.get_resource(string_view("file://nonexistent_path_67890.txt"));
    ASSERT_TRUE(res);
    EXPECT_EQ(-1, res->size());
}

TEST_F(ResourceStoreTest, UnknownScheme)
{
    auto res = store_.get_resource(string_view("unknown://something"));
    EXPECT_FALSE(res);
}

TEST_F(ResourceStoreTest, MissingScheme)
{
    auto res = store_.get_resource(string_view("no_scheme_here"));
    EXPECT_FALSE(res);
}

TEST_F(ResourceStoreTest, ReadNonExistentFile)
{
    auto res = store_.get_resource(string_view("file://does_not_exist_abc.txt"));
    ASSERT_TRUE(res);

    auto* f = interface_cast<IFile>(res);
    ASSERT_NE(nullptr, f);

    ::velk::string out;
    EXPECT_TRUE(failed(f->read_text(out)));
}

TEST_F(ResourceStoreTest, ReadEmptyFile)
{
    auto uri = file_uri(write_temp_file("rs_test_empty.txt", "", 0));

    auto res = store_.get_resource(uri);
    auto* f = interface_cast<IFile>(res);
    ASSERT_NE(nullptr, f);

    ::velk::string out;
    EXPECT_EQ(Success, f->read_text(out));
    EXPECT_EQ(0u, out.size());
}

TEST_F(ResourceStoreTest, ResourceUri)
{
    auto res = store_.get_resource(string_view("file://some/path.txt"));
    ASSERT_TRUE(res);
    EXPECT_EQ(string_view("file://some/path.txt"), res->uri());
}

TEST_F(ResourceStoreTest, SchemeAlias)
{
    // Create an "app" protocol backed by a temp directory.
    auto tmp = testing::TempDir();
    ::velk::string base(tmp.c_str(), tmp.size());

    // Write a file in the base directory.
    const char* content = "app resource";
    ::velk::string file_path = base;
    file_path.append("app_test.txt");
    FILE* f = open_file(file_path.c_str(), "wb");
    ASSERT_NE(nullptr, f);
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    // Register app:// protocol with base path.
    auto fp = ::velk::instance().create<IResourceProtocolInternal>(ClassId::FileProtocol);
    ASSERT_TRUE(fp);
    EXPECT_EQ(Success, fp->set_scheme("app"));
    EXPECT_EQ(Success, fp->set_base_path(base));
    EXPECT_EQ(Success, store_.register_protocol(interface_pointer_cast<IResourceProtocol>(fp)));

    // Access via app:// scheme.
    auto res = store_.get_resource(string_view("app://app_test.txt"));
    ASSERT_TRUE(res);
    EXPECT_TRUE(res->exists());

    auto* file = interface_cast<IFile>(res);
    ASSERT_NE(nullptr, file);

    ::velk::string out;
    EXPECT_EQ(Success, file->read_text(out));
    EXPECT_EQ(string_view("app resource"), string_view(out));
}

// Mock resource returned by MockProtocol.
class MockResource : public ext::ObjectCore<MockResource, IResource>
{
public:
    string_view uri() const override { return uri_; }
    bool exists() const override { return true; }
    int64_t size() const override { return 99; }

    ::velk::string uri_;
};

// Custom protocol for testing type_registry discovery.
class MockProtocol : public ext::ObjectCore<MockProtocol, IResourceProtocol>
{
public:
    VELK_CLASS_UID("f17e5000-0002-4000-8000-000000000099", "MockProtocol");

    string_view scheme() const override { return "mock"; }

    IResource::Ptr resolve(string_view path) const override
    {
        auto obj = ext::make_object<MockResource>();
        auto* mr = static_cast<MockResource*>(obj.get());
        mr->uri_ = "mock://" + ::velk::string(path);
        return interface_pointer_cast<IResource>(obj);
    }
};

TEST(ResourceStoreCustomProtocol, RegisterCustomProtocol)
{
    auto& v = ::velk::instance();

    auto proto = ext::make_object<MockProtocol>();
    v.resource_store().register_protocol(interface_pointer_cast<IResourceProtocol>(proto));

    auto res = v.resource_store().get_resource(string_view("mock://hello"));
    ASSERT_TRUE(res);
    EXPECT_EQ(string_view("mock://hello"), res->uri());
    EXPECT_TRUE(res->exists());
    EXPECT_EQ(99, res->size());

    // Not an IFile, so cast should fail.
    EXPECT_EQ(nullptr, interface_cast<IFile>(res));
}
