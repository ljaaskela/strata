#include <velk/api/velk.h>
#include <velk/ext/core_object.h>
#include <velk/interface/resource/intf_resource.h>
#include <velk/interface/resource/intf_resource_decoder.h>
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
    bool is_persistent() const override { return persistent_; }
    void set_persistent(bool value) override { persistent_ = value; }

    ::velk::string uri_;
    bool persistent_{false};
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

// Decoded resource produced by MockDecoder. Wraps an inner resource.
class DecodedResource : public ext::ObjectCore<DecodedResource, IResource>
{
public:
    string_view uri() const override { return uri_; }
    bool exists() const override { return true; }
    int64_t size() const override { return 1; }
    bool is_persistent() const override { return persistent_; }
    void set_persistent(bool value) override { persistent_ = value; }

    ::velk::string uri_;
    IResource::Ptr inner_;
    int decode_id_{0};
    bool persistent_{false};
};

// Test decoder. Increments a counter on each decode call so tests can verify
// dedup (cache hit = no new decode) and reload after eviction.
class MockDecoder : public ext::ObjectCore<MockDecoder, IResourceDecoder>
{
public:
    string_view name() const override { return "mock"; }

    IResource::Ptr decode(const IResource::Ptr& inner) const override
    {
        if (!inner) {
            return nullptr;
        }
        ++decode_count_;
        auto obj = ext::make_object<DecodedResource>();
        auto* dr = static_cast<DecodedResource*>(obj.get());
        dr->uri_ = ::velk::string("mock:") + ::velk::string(inner->uri());
        dr->inner_ = inner;
        dr->decode_id_ = decode_count_;
        return interface_pointer_cast<IResource>(obj);
    }

    mutable int decode_count_{0};
};

TEST_F(ResourceStoreTest, DecoderRegisterAndFind)
{
    auto dec = ext::make_object<MockDecoder>();
    auto dec_iface = interface_pointer_cast<IResourceDecoder>(dec);

    EXPECT_EQ(Success, store_.register_decoder(dec_iface));

    auto found = store_.find_decoder(string_view("mock"));
    EXPECT_EQ(dec_iface.get(), found.get());

    EXPECT_EQ(Success, store_.unregister_decoder(dec_iface));
    EXPECT_FALSE(store_.find_decoder(string_view("mock")));
}

TEST_F(ResourceStoreTest, DecoderRoundTrip)
{
    const char* content = "decoder payload";
    auto path = write_temp_file("rs_test_decoder.txt", content, strlen(content));
    auto inner_uri = file_uri(path);
    auto outer_uri = ::velk::string("mock:") + inner_uri;

    auto dec = ext::make_object<MockDecoder>();
    auto dec_iface = interface_pointer_cast<IResourceDecoder>(dec);
    store_.register_decoder(dec_iface);

    auto* raw = static_cast<MockDecoder*>(dec.get());
    EXPECT_EQ(0, raw->decode_count_);

    auto res = store_.get_resource(string_view(outer_uri));
    ASSERT_TRUE(res);
    EXPECT_EQ(1, raw->decode_count_);
    EXPECT_EQ(string_view(outer_uri), res->uri());

    store_.unregister_decoder(dec_iface);
}

TEST_F(ResourceStoreTest, DecoderDedupAndEviction)
{
    const char* content = "dedup payload";
    auto path = write_temp_file("rs_test_dedup.txt", content, strlen(content));
    auto outer_uri = ::velk::string("mock:") + file_uri(path);

    auto dec = ext::make_object<MockDecoder>();
    auto dec_iface = interface_pointer_cast<IResourceDecoder>(dec);
    store_.register_decoder(dec_iface);
    auto* raw = static_cast<MockDecoder*>(dec.get());

    // First call: decodes once.
    auto a = store_.get_resource(string_view(outer_uri));
    ASSERT_TRUE(a);
    EXPECT_EQ(1, raw->decode_count_);

    // Second call while a still alive: cache hit, no new decode, same pointer.
    auto b = store_.get_resource(string_view(outer_uri));
    ASSERT_TRUE(b);
    EXPECT_EQ(1, raw->decode_count_);
    EXPECT_EQ(a.get(), b.get());

    // Drop both. Cache slot becomes a dead weak_ref.
    a.reset();
    b.reset();

    // Next call reloads.
    auto c = store_.get_resource(string_view(outer_uri));
    ASSERT_TRUE(c);
    EXPECT_EQ(2, raw->decode_count_);

    store_.unregister_decoder(dec_iface);
}

TEST_F(ResourceStoreTest, DecoderUriParsingDoesNotCollideWithProtocol)
{
    // "file://..." has a colon but is followed by "//", so it must NOT be
    // treated as a decoder URI even if a decoder named "file" existed.
    const char* content = "no collision";
    auto uri = file_uri(write_temp_file("rs_test_collide.txt", content, strlen(content)));

    auto res = store_.get_resource(string_view(uri));
    ASSERT_TRUE(res);
    auto* f = interface_cast<IFile>(res);
    ASSERT_NE(nullptr, f);
}

TEST_F(ResourceStoreTest, DecoderPersistencePinning)
{
    const char* content = "persistent";
    auto path = write_temp_file("rs_test_persist.txt", content, strlen(content));
    auto outer_uri = ::velk::string("mock:") + file_uri(path);

    auto dec = ext::make_object<MockDecoder>();
    auto dec_iface = interface_pointer_cast<IResourceDecoder>(dec);
    store_.register_decoder(dec_iface);
    auto* raw = static_cast<MockDecoder*>(dec.get());

    // Load and mark persistent.
    {
        auto a = store_.get_resource(string_view(outer_uri));
        ASSERT_TRUE(a);
        EXPECT_EQ(1, raw->decode_count_);
        a->set_persistent(true);
        // Touch the cache so it picks up the new persistence flag.
        auto b = store_.get_resource(string_view(outer_uri));
        EXPECT_EQ(a.get(), b.get());
        EXPECT_EQ(1, raw->decode_count_);
    }
    // Both local refs dropped, but pinned slot keeps the resource alive.
    auto c = store_.get_resource(string_view(outer_uri));
    ASSERT_TRUE(c);
    EXPECT_EQ(1, raw->decode_count_); // no reload

    // Unpin: next access should drop the strong ref. Drop c too.
    c->set_persistent(false);
    // First get_resource reconciles and drops the pinned slot but still
    // returns c (the live one).
    auto d = store_.get_resource(string_view(outer_uri));
    EXPECT_EQ(c.get(), d.get());
    c.reset();
    d.reset();
    // Now nothing keeps it alive: next access reloads.
    auto e = store_.get_resource(string_view(outer_uri));
    ASSERT_TRUE(e);
    EXPECT_EQ(2, raw->decode_count_);

    e->set_persistent(false); // tidy
    store_.unregister_decoder(dec_iface);
}

TEST_F(ResourceStoreTest, UnknownDecoderFallsThroughToProtocol)
{
    // No decoder registered named "unknown". The store should NOT treat the
    // URI as a decoder URI; with no protocol matching either, it returns null.
    auto res = store_.get_resource(string_view("unknown:something"));
    EXPECT_FALSE(res);
}
