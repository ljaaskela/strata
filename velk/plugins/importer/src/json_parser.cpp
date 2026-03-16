#include "json_parser.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace velk {

namespace {

class Parser
{
public:
    Parser(const char* data, size_t length) : data_(data), end_(data + length), pos_(data) {}

    bool parse(JsonValue& out, velk::string& err)
    {
        skip_whitespace();
        out = parse_value();
        if (failed_) {
            err = error_;
            return false;
        }
        skip_whitespace();
        if (pos_ != end_) {
            set_error("unexpected trailing content");
            err = error_;
            return false;
        }
        return true;
    }

private:
    const char* data_;
    const char* end_;
    const char* pos_;
    bool failed_ = false;
    velk::string error_;

    void set_error(const char* msg)
    {
        if (failed_) return;
        failed_ = true;
        size_t offset = static_cast<size_t>(pos_ - data_);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%zu", offset);
        error_ = velk::string("JSON parse error at offset ") + buf + ": " + msg;
    }

    char peek()
    {
        if (pos_ >= end_) {
            set_error("unexpected end of input");
            return '\0';
        }
        return *pos_;
    }

    char advance()
    {
        if (pos_ >= end_) {
            set_error("unexpected end of input");
            return '\0';
        }
        return *pos_++;
    }

    bool expect(char c)
    {
        if (failed_) return false;
        char got = advance();
        if (failed_) return false;
        if (got != c) {
            pos_--;
            char msg[] = "expected 'X'";
            msg[10] = c;
            set_error(msg);
            return false;
        }
        return true;
    }

    void skip_whitespace()
    {
        while (pos_ < end_ && (*pos_ == ' ' || *pos_ == '\t' || *pos_ == '\n' || *pos_ == '\r')) {
            pos_++;
        }
    }

    JsonValue parse_value()
    {
        if (failed_) return {};
        skip_whitespace();
        char c = peek();
        if (failed_) return {};
        switch (c) {
        case '"':
            return parse_string_value();
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case 't':
        case 'f':
            return parse_bool();
        case 'n':
            return parse_null();
        default:
            if (c == '-' || (c >= '0' && c <= '9')) {
                return parse_number();
            }
            set_error("unexpected character");
            return {};
        }
    }

    JsonValue parse_null()
    {
        if (failed_) return {};
        if (end_ - pos_ < 4 || std::memcmp(pos_, "null", 4) != 0) {
            set_error("invalid literal");
            return {};
        }
        pos_ += 4;
        return JsonValue::null();
    }

    JsonValue parse_bool()
    {
        if (failed_) return {};
        if (end_ - pos_ >= 4 && std::memcmp(pos_, "true", 4) == 0) {
            pos_ += 4;
            return JsonValue::boolean(true);
        }
        if (end_ - pos_ >= 5 && std::memcmp(pos_, "false", 5) == 0) {
            pos_ += 5;
            return JsonValue::boolean(false);
        }
        set_error("invalid literal");
        return {};
    }

    JsonValue parse_number()
    {
        if (failed_) return {};
        const char* start = pos_;
        if (pos_ < end_ && *pos_ == '-') {
            pos_++;
        }
        if (pos_ >= end_ || *pos_ < '0' || *pos_ > '9') {
            set_error("invalid number");
            return {};
        }
        if (*pos_ == '0') {
            pos_++;
        } else {
            while (pos_ < end_ && *pos_ >= '0' && *pos_ <= '9') {
                pos_++;
            }
        }
        if (pos_ < end_ && *pos_ == '.') {
            pos_++;
            if (pos_ >= end_ || *pos_ < '0' || *pos_ > '9') {
                set_error("invalid number: expected digit after decimal point");
                return {};
            }
            while (pos_ < end_ && *pos_ >= '0' && *pos_ <= '9') {
                pos_++;
            }
        }
        if (pos_ < end_ && (*pos_ == 'e' || *pos_ == 'E')) {
            pos_++;
            if (pos_ < end_ && (*pos_ == '+' || *pos_ == '-')) {
                pos_++;
            }
            if (pos_ >= end_ || *pos_ < '0' || *pos_ > '9') {
                set_error("invalid number: expected digit in exponent");
                return {};
            }
            while (pos_ < end_ && *pos_ >= '0' && *pos_ <= '9') {
                pos_++;
            }
        }
        // Manual strtod without exceptions
        velk::string numStr(start, static_cast<size_t>(pos_ - start));
        char* end_ptr = nullptr;
        double value = std::strtod(numStr.c_str(), &end_ptr);
        if (end_ptr == numStr.c_str()) {
            set_error("invalid number");
            return {};
        }
        return JsonValue::number(value);
    }

    velk::string parse_string()
    {
        if (failed_) return {};
        if (!expect('"')) return {};
        velk::string result;
        while (true) {
            if (failed_) return {};
            if (pos_ >= end_) {
                set_error("unterminated string");
                return {};
            }
            char c = *pos_++;
            if (c == '"') {
                return result;
            }
            if (c == '\\') {
                if (pos_ >= end_) {
                    set_error("unterminated escape");
                    return {};
                }
                char esc = *pos_++;
                switch (esc) {
                case '"':
                    result += '"';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '/':
                    result += '/';
                    break;
                case 'b':
                    result += '\b';
                    break;
                case 'f':
                    result += '\f';
                    break;
                case 'n':
                    result += '\n';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case 't':
                    result += '\t';
                    break;
                case 'u': {
                    uint32_t cp = parse_hex4();
                    if (failed_) return {};
                    // Handle surrogate pairs
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos_ + 1 < end_ && pos_[0] == '\\' && pos_[1] == 'u') {
                            pos_ += 2;
                            uint32_t lo = parse_hex4();
                            if (failed_) return {};
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            }
                        }
                    }
                    encode_utf8(result, cp);
                    break;
                }
                default:
                    set_error("invalid escape character");
                    return {};
                }
            } else {
                result += c;
            }
        }
    }

    JsonValue parse_string_value()
    {
        auto s = parse_string();
        if (failed_) return {};
        return JsonValue::string(std::move(s));
    }

    uint32_t parse_hex4()
    {
        if (failed_) return 0;
        if (end_ - pos_ < 4) {
            set_error("incomplete unicode escape");
            return 0;
        }
        uint32_t value = 0;
        for (int i = 0; i < 4; i++) {
            char c = *pos_++;
            value <<= 4;
            if (c >= '0' && c <= '9') {
                value |= static_cast<uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value |= static_cast<uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value |= static_cast<uint32_t>(c - 'A' + 10);
            } else {
                set_error("invalid hex digit in unicode escape");
                return 0;
            }
        }
        return value;
    }

    static void encode_utf8(velk::string& out, uint32_t cp)
    {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    JsonValue parse_object()
    {
        if (failed_) return {};
        if (!expect('{')) return {};
        skip_whitespace();
        velk::vector<std::pair<velk::string, JsonValue>> members;
        if (!failed_ && peek() == '}') {
            pos_++;
            return JsonValue::object(std::move(members));
        }
        if (failed_) return {};
        while (true) {
            if (failed_) return {};
            skip_whitespace();
            auto key = parse_string();
            if (failed_) return {};
            skip_whitespace();
            if (!expect(':')) return {};
            skip_whitespace();
            JsonValue value = parse_value();
            if (failed_) return {};
            members.push_back({std::move(key), std::move(value)});
            skip_whitespace();
            char c = advance();
            if (failed_) return {};
            if (c == '}') {
                break;
            }
            if (c != ',') {
                pos_--;
                set_error("expected ',' or '}'");
                return {};
            }
        }
        return JsonValue::object(std::move(members));
    }

    JsonValue parse_array()
    {
        if (failed_) return {};
        if (!expect('[')) return {};
        skip_whitespace();
        vector<JsonValue> elements;
        if (!failed_ && peek() == ']') {
            pos_++;
            return JsonValue::array(std::move(elements));
        }
        if (failed_) return {};
        while (true) {
            if (failed_) return {};
            skip_whitespace();
            elements.push_back(parse_value());
            if (failed_) return {};
            skip_whitespace();
            char c = advance();
            if (failed_) return {};
            if (c == ']') {
                break;
            }
            if (c != ',') {
                pos_--;
                set_error("expected ',' or ']'");
                return {};
            }
        }
        return JsonValue::array(std::move(elements));
    }
};

} // namespace

bool json_parse(const char* input, size_t length, JsonValue& out, string& error)
{
    if (!input || length == 0) {
        error = "JSON parse error: empty input";
        return false;
    }
    Parser parser(input, length);
    return parser.parse(out, error);
}

} // namespace velk
