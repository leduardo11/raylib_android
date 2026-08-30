#pragma once

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Content {
namespace Json {

// Minimal streaming JSON DOM. Deliberately tiny: the atlas@2 / tilemap
// packages are shallow, so we only need objects, arrays, strings, numbers,
// bools and null — no pretty-printing, and lookups are linear scans.
struct Value;

using Node      = std::vector<Value>;
using Member    = std::pair<std::string, Value>;
using Members   = std::vector<Member>;

struct Object {
    Members members;

    const Value* find(const char* key) const;
};

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool bval = false;
    double num = 0.0;
    std::string str;
    Node  arr;
    Object obj;

    bool isNull()   const { return type == Type::Null; }
    bool isBool()   const { return type == Type::Bool; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray()  const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    // Safe accessors with defaults. Never throw for wrong types.
    bool asBool(bool def = false) const
    {
        return isBool() ? bval : def;
    }
    int asInt(int def = 0) const
    {
        return isNumber() ? (int)num : def;
    }
    double asNumber(double def = 0.0) const
    {
        return isNumber() ? num : def;
    }
    std::string asString(const char* def = "") const
    {
        return isString() ? str : std::string(def);
    }

    // Member lookup on an object; returns nullptr when absent or wrong type.
    const Value* find(const char* key) const
    {
        return isObject() ? obj.find(key) : nullptr;
    }

    // Array element; returns nullptr on wrong type or out of range.
    const Value* at(size_t index) const
    {
        if (!isArray()) return nullptr;
        return index < arr.size() ? &arr[index] : nullptr;
    }

    size_t size() const
    {
        if (isArray()) return arr.size();
        if (isObject()) return obj.members.size();
        return 0;
    }
};

inline const Value* Object::find(const char* key) const
{
    for (const auto& m : members)
        if (m.first == key) return &m.second;
    return nullptr;
}

namespace detail {

struct Parser {
    const char* p;
    const char* end;

    [[noreturn]] void fail(const char* msg) const
    {
        throw std::runtime_error(std::string("json parse error: ") + msg);
    }

    void skipWs()
    {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }

    void expect(char c)
    {
        if (p >= end || *p != c) fail("unexpected character");
        ++p;
    }

    bool tryKeyword(const char* kw)
    {
        size_t n = 0;
        while (kw[n]) ++n;
        if (end - p >= (ptrdiff_t)n && strncmp(p, kw, n) == 0)
        {
            p += n;
            return true;
        }
        return false;
    }

    std::string parseString()
    {
        expect('"');
        std::string out;
        while (true)
        {
            if (p >= end) fail("unterminated string");
            char c = *p++;
            if (c == '"') break;
            if (c == '\\')
            {
                if (p >= end) fail("unterminated escape");
                char e = *p++;
                switch (e)
                {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u':
                    {
                        if (end - p < 4) fail("bad unicode escape");
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            char h = *p++;
                            cp <<= 4;
                            if (h >= '0' && h <= '9')      cp |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                            else fail("bad unicode escape");
                        }
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800)
                        {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        else
                        {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: fail("bad escape");
                }
            }
            else
            {
                out += c;
            }
        }
        return out;
    }

    double parseNumber()
    {
        const char* start = p;
        if (p < end && (*p == '-' || *p == '+')) ++p;
        while (p < end && (*p >= '0' && *p <= '9')) ++p;
        if (p < end && *p == '.')
        {
            ++p;
            while (p < end && (*p >= '0' && *p <= '9')) ++p;
        }
        if (p < end && (*p == 'e' || *p == 'E'))
        {
            ++p;
            if (p < end && (*p == '-' || *p == '+')) ++p;
            while (p < end && (*p >= '0' && *p <= '9')) ++p;
        }
        std::string s(start, (size_t)(p - start));
        return strtod(s.c_str(), nullptr);
    }

    Value parseValue()
    {
        skipWs();
        if (p >= end) fail("unexpected end of input");

        Value v;
        char c = *p;

        if (c == '{')
        {
            ++p;
            v.type = Value::Type::Object;
            skipWs();
            if (p < end && *p == '}')
            {
                ++p;
                return v;
            }
            while (true)
            {
                skipWs();
                std::string key = parseString();
                skipWs();
                expect(':');
                Value val = parseValue();
                v.obj.members.emplace_back(std::move(key), std::move(val));
                skipWs();
                if (p >= end) fail("unterminated object");
                if (*p == ',')
                {
                    ++p;
                    continue;
                }
                if (*p == '}')
                {
                    ++p;
                    break;
                }
                fail("expected ',' or '}'");
            }
            return v;
        }

        if (c == '[')
        {
            ++p;
            v.type = Value::Type::Array;
            skipWs();
            if (p < end && *p == ']')
            {
                ++p;
                return v;
            }
            while (true)
            {
                v.arr.push_back(parseValue());
                skipWs();
                if (p >= end) fail("unterminated array");
                if (*p == ',')
                {
                    ++p;
                    continue;
                }
                if (*p == ']')
                {
                    ++p;
                    break;
                }
                fail("expected ',' or ']'");
            }
            return v;
        }

        if (c == '"')
        {
            v.type = Value::Type::String;
            v.str = parseString();
            return v;
        }

        if (c == 't' && tryKeyword("true"))
        {
            v.type = Value::Type::Bool;
            v.bval = true;
            return v;
        }
        if (c == 'f' && tryKeyword("false"))
        {
            v.type = Value::Type::Bool;
            v.bval = false;
            return v;
        }
        if (c == 'n' && tryKeyword("null"))
        {
            v.type = Value::Type::Null;
            return v;
        }

        if (c == '-' || (c >= '0' && c <= '9'))
        {
            v.type = Value::Type::Number;
            v.num = parseNumber();
            return v;
        }

        fail("unexpected token");
    }
};

} // namespace detail

// Parses a UTF-8 JSON document. Throws std::runtime_error on malformed input.
inline Value parse(const std::string& text)
{
    detail::Parser pa{ text.data(), text.data() + text.size() };
    Value v = pa.parseValue();
    pa.skipWs();
    if (pa.p != pa.end) pa.fail("trailing data");
    return v;
}

} // namespace Json
} // namespace Content