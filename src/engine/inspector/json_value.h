#pragma once
// A small JSON reader for the inspector's glTF import. Plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/types.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gbtg
{
class JsonValue
{
public:
    enum class Kind
    {
        Null, Bool, Number, String, Array, Object
    };
    static JsonValue parse(std::string_view text);

    Kind kind() const
    {
        return kind_;
    }
    bool is_null() const
    {
        return kind_ == Kind::Null;
    }
    bool has(const std::string& key) const
    {
        return kind_ == Kind::Object && object_.contains(key);
    }
    // A missing key or index reads as null.
    const JsonValue& operator[](const std::string& key) const;
    const JsonValue& operator[](size_t index) const;
    size_t size() const
    {
        return kind_ == Kind::Array ? array_.size() : kind_ == Kind::Object ? object_.size() : 0;
    }
    double number(double fallback = 0.0) const
    {
        return kind_ == Kind::Number ? number_ : fallback;
    }
    long long integer(long long fallback = 0) const
    {
        return kind_ == Kind::Number ? (long long)(number_) : fallback;
    }
    bool boolean(bool fallback = false) const
    {
        return kind_ == Kind::Bool ? boolean_ : fallback;
    }
    const std::string& string() const
    {
        return string_;
    }
    const std::vector<JsonValue>& items() const
    {
        return array_;
    }
    const std::map<std::string, JsonValue>& members() const
    {
        return object_;
    }

private:
    Kind kind_ = Kind::Null;
    bool boolean_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::map<std::string, JsonValue> object_;
    friend class JsonParser;
};
}
#endif
