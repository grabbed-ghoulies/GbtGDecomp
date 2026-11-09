#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "json_value.h"

#include <cstdlib>

namespace gbtg
{
class JsonParser
{
public:
    explicit JsonParser(std::string_view text) : text_(text)
    {
    }

    JsonValue value(int depth = 0)
    {
        if (depth > 256)
        {
            fail("nesting too deep");
        }
        skip();
        JsonValue out;
        const char c = peek();
        if (c == '{')
        {
            ++at_;
            out.kind_ = JsonValue::Kind::Object;
            skip();
            if (peek() == '}')
            {
                ++at_;
                return out;
            }
            for (;;)
            {
                skip();
                std::string key = string();
                skip();
                expect(':');
                out.object_[std::move(key)] = value(depth + 1);
                skip();
                if (peek() == ',')
                {
                    ++at_;
                    continue;
                }
                expect('}');
                return out;
            }
        }
        if (c == '[')
        {
            ++at_;
            out.kind_ = JsonValue::Kind::Array;
            skip();
            if (peek() == ']')
            {
                ++at_;
                return out;
            }
            for (;;)
            {
                out.array_.push_back(value(depth + 1));
                skip();
                if (peek() == ',')
                {
                    ++at_;
                    continue;
                }
                expect(']');
                return out;
            }
        }
        if (c == '"')
        {
            out.kind_ = JsonValue::Kind::String;
            out.string_ = string();
            return out;
        }
        if (text_.substr(at_, 4) == "true")
        {
            at_ += 4;
            out.kind_ = JsonValue::Kind::Bool;
            out.boolean_ = true;
            return out;
        }
        if (text_.substr(at_, 5) == "false")
        {
            at_ += 5;
            out.kind_ = JsonValue::Kind::Bool;
            return out;
        }
        if (text_.substr(at_, 4) == "null")
        {
            at_ += 4;
            return out;
        }
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            const size_t start = at_;
            while (at_ < text_.size() && std::string_view("+-0123456789.eE").find(text_[at_]) != std::string_view::npos)
            {
                ++at_;
            }
            const std::string digits(text_.substr(start, at_ - start));
            char* end = nullptr;
            out.kind_ = JsonValue::Kind::Number;
            out.number_ = std::strtod(digits.c_str(), &end);
            if (end == digits.c_str())
            {
                fail("bad number");
            }
            return out;
        }
        fail("unexpected character");
        return out;
    }

    void finish()
    {
        skip();
        if (at_ != text_.size())
        {
            fail("trailing characters");
        }
    }

private:
    [[noreturn]] void fail(const char* what) const
    {
        throw Error(std::string("JSON: ") + what + " at byte " + std::to_string(at_));
    }
    char peek() const
    {
        return at_ < text_.size() ? text_[at_] : '\0';
    }
    void skip()
    {
        while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\n' || text_[at_] == '\r' || text_[at_] == '\t'))
        {
            ++at_;
        }
    }
    void expect(char c)
    {
        if (peek() != c)
        {
            fail("unexpected character");
        }
        ++at_;
    }
    void utf8(std::string& out, u32 code)
    {
        if (code < 0x80)
        {
            out += char(code);
        }
        else if (code < 0x800)
        {
            out += char(0xC0 | (code >> 6));
            out += char(0x80 | (code & 0x3F));
        }
        else if (code < 0x10000)
        {
            out += char(0xE0 | (code >> 12));
            out += char(0x80 | ((code >> 6) & 0x3F));
            out += char(0x80 | (code & 0x3F));
        }
        else
        {
            out += char(0xF0 | (code >> 18));
            out += char(0x80 | ((code >> 12) & 0x3F));
            out += char(0x80 | ((code >> 6) & 0x3F));
            out += char(0x80 | (code & 0x3F));
        }
    }
    u32 hex4()
    {
        if (at_ + 4 > text_.size())
        {
            fail("bad escape");
        }
        const std::string digits(text_.substr(at_, 4));
        at_ += 4;
        return u32(std::strtoul(digits.c_str(), nullptr, 16));
    }
    std::string string()
    {
        expect('"');
        std::string out;
        for (;;)
        {
            if (at_ >= text_.size())
            {
                fail("unterminated string");
            }
            const char c = text_[at_++];
            if (c == '"')
            {
                return out;
            }
            if (c != '\\')
            {
                out += c;
                continue;
            }
            const char escape = peek();
            ++at_;
            switch (escape)
            {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u':
            {
                u32 code = hex4();
                if (code >= 0xD800 && code < 0xDC00 && text_.substr(at_, 2) == "\\u")
                {
                    at_ += 2;
                    code = 0x10000 + ((code - 0xD800) << 10) + (hex4() - 0xDC00);
                }
                utf8(out, code);
                break;
            }
            default:
                fail("bad escape");
            }
        }
    }

    std::string_view text_;
    size_t at_ = 0;
};

JsonValue JsonValue::parse(std::string_view text)
{
    JsonParser parser(text);
    JsonValue out = parser.value();
    parser.finish();
    return out;
}

const JsonValue& JsonValue::operator[](const std::string& key) const
{
    static const JsonValue null;
    if (kind_ != Kind::Object)
    {
        return null;
    }
    const auto found = object_.find(key);
    return found == object_.end() ? null : found->second;
}

const JsonValue& JsonValue::operator[](size_t index) const
{
    static const JsonValue null;
    return kind_ == Kind::Array && index < array_.size() ? array_[index] : null;
}
}
#endif
