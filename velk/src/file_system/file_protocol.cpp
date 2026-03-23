#include "file_protocol.h"

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
    // #ifdef _WIN32
    FILE* f = nullptr;
    if (fopen_s(&f, path, mode) != 0) {
        return nullptr;
    }
    return f;
    /*#else
        return fopen(path, mode);
    #endif*/
}

} // namespace

string_view FileProtocol::scheme() const
{
    return "file";
}

ReturnValue FileProtocol::read_file(string_view path, vector<uint8_t>& out) const
{
    if (path.empty()) {
        return InvalidArgument;
    }

    // string_view may not be null-terminated; copy to a buffer.
    string path_str(path);

    FILE* f = open_file(path_str.c_str(), "rb");
    if (!f) {
        return Fail;
    }

    // Get file size.
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
        size_t read = fread(out.data(), 1, static_cast<size_t>(len), f);
        if (read != static_cast<size_t>(len)) {
            fclose(f);
            out.clear();
            return Fail;
        }
    }

    fclose(f);
    return Success;
}

ReturnValue FileProtocol::read_text_file(string_view path, string& out) const
{
    vector<uint8_t> data;
    auto rv = read_file(path, data);
    if (failed(rv)) {
        return rv;
    }
    out = string(string_view(reinterpret_cast<const char*>(data.data()), data.size()));
    return Success;
}

bool FileProtocol::file_exists(string_view path) const
{
    if (path.empty()) {
        return false;
    }
    string path_str(path);
    velk_stat_t st;
    return velk_stat(path_str.c_str(), &st) == 0;
}

int64_t FileProtocol::file_size(string_view path) const
{
    if (path.empty()) {
        return -1;
    }
    string path_str(path);
    velk_stat_t st;
    if (velk_stat(path_str.c_str(), &st) != 0) {
        return -1;
    }
    return static_cast<int64_t>(st.st_size);
}

} // namespace velk
