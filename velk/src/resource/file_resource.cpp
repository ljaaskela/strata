#include "file_resource.h"

#include <cstdio>
#include <sys/stat.h>

#ifdef _WIN32
#include <sys/types.h>
#define velk_stat _stat64
#define velk_stat_t struct __stat64
#else
#define velk_stat stat
#define velk_stat_t struct stat
#endif

namespace velk {

namespace {

FILE* open_file(const char* path, const char* mode)
{
#ifdef _WIN32
    FILE* f = nullptr;
    if (fopen_s(&f, path, mode) != 0) {
        return nullptr;
    }
    return f;
#else
    return fopen(path, mode);
#endif
}

} // namespace

void FileResource::set_path(string_view path, string_view uri)
{
    path_ = string(path);
    uri_ = string(uri);
}

string_view FileResource::uri() const
{
    return uri_;
}

bool FileResource::exists() const
{
    if (path_.empty()) {
        return false;
    }
    velk_stat_t st;
    return velk_stat(path_.c_str(), &st) == 0;
}

int64_t FileResource::size() const
{
    if (path_.empty()) {
        return -1;
    }
    velk_stat_t st;
    if (velk_stat(path_.c_str(), &st) != 0) {
        return -1;
    }
    return static_cast<int64_t>(st.st_size);
}

ReturnValue FileResource::read(vector<uint8_t>& out) const
{
    if (path_.empty()) {
        return InvalidArgument;
    }

    FILE* f = open_file(path_.c_str(), "rb");
    if (!f) {
        return Fail;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return Fail;
    }
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return Fail;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return Fail;
    }

    out.resize(static_cast<size_t>(len));
    if (len > 0) {
        size_t bytes_read = fread(out.data(), 1, static_cast<size_t>(len), f);
        if (bytes_read != static_cast<size_t>(len)) {
            fclose(f);
            out.clear();
            return Fail;
        }
    }

    fclose(f);
    return Success;
}

ReturnValue FileResource::read_text(string& out) const
{
    vector<uint8_t> data;
    auto rv = read(data);
    if (failed(rv)) {
        return rv;
    }
    out = string(string_view(reinterpret_cast<const char*>(data.data()), data.size()));
    return Success;
}

} // namespace velk
