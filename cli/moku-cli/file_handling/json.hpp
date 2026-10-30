#pragma once

#include <map>
#include <string>
#include <vector>

namespace cli
{
struct JsonValue
{
    enum class Kind
    {
        Null,
        Number,
        String,
        Object,
        Array,
        Boolean
    };
    Kind kind = Kind::Null;
    std::string text;
    std::map<std::string, JsonValue> members;
    std::vector<JsonValue> elements;
};

// Throws invalid_argument for malformed JSON or excessive nesting.
JsonValue parseJson(const std::string& text);
std::string jsonQuote(const std::string& text);
} // namespace cli
