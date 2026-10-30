#include "file_handling/json.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cli
{
namespace
{
/**
 * Checks if the character at the given index in the text is a valid UTF-8 continuation byte.
 * @param text The text to check.
 * @param index The index of the character to check.
 * @param low The lower bound for the continuation byte (default is 0x80).
 * @param high The upper bound for the continuation byte (default is 0xbf).
 * @return True if the character is a valid continuation byte, false otherwise.
 */
bool continuation(const std::string& text, std::size_t index, unsigned char low = 0x80,
                  unsigned char high = 0xbf)
{
    if (index >= text.size())
    {
        return false;
    }
    const auto ch = static_cast<unsigned char>(text[index]);
    return ch >= low && ch <= high;
}

/**
 * Calculates the length of a UTF-8 encoded character starting at the given index in the text.
 * @param text The text to check.
 * @param index The index of the character to check.
 * @return The length of the UTF-8 character, or 0 if the character is invalid.
 */
std::size_t utf8Length(const std::string& text, std::size_t index)
{
    const auto lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80)
    {
        return 1;
    }
    if (lead >= 0xc2 && lead <= 0xdf)
    {
        return continuation(text, index + 1) ? 2 : 0;
    }
    if (lead >= 0xe0 && lead <= 0xef)
    {
        const unsigned char low = lead == 0xe0 ? 0xa0 : 0x80;
        const unsigned char high = lead == 0xed ? 0x9f : 0xbf;
        return continuation(text, index + 1, low, high) && continuation(text, index + 2) ? 3 : 0;
    }
    if (lead >= 0xf0 && lead <= 0xf4)
    {
        const unsigned char low = lead == 0xf0 ? 0x90 : 0x80;
        const unsigned char high = lead == 0xf4 ? 0x8f : 0xbf;
        return continuation(text, index + 1, low, high) && continuation(text, index + 2) &&
                       continuation(text, index + 3)
                   ? 4
                   : 0;
    }
    return 0;
}

class Parser
{
public:
    explicit Parser(const std::string& text) : text_(text) {}

    JsonValue parse()
    {
        auto result = value(0);
        space();
        if (pos_ != text_.size())
        {
            fail();
        }
        return result;
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail() const
    {
        throw std::invalid_argument("Invalid JSON.");
    }
    void space()
    {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\r' ||
                                       text_[pos_] == '\n' || text_[pos_] == '\t'))
        {
            ++pos_;
        }
    }
    bool take(char ch)
    {
        space();
        if (pos_ < text_.size() && text_[pos_] == ch)
        {
            ++pos_;
            return true;
        }
        return false;
    }
    void expect(char ch)
    {
        if (!take(ch))
        {
            fail();
        }
    }
    unsigned hex()
    {
        unsigned result = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (pos_ == text_.size())
            {
                fail();
            }
            const char ch = text_[pos_++];
            const int digit = ch >= '0' && ch <= '9'   ? ch - '0'
                              : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
                              : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10
                                                       : -1;
            if (digit < 0)
            {
                fail();
            }
            result = result * 16 + static_cast<unsigned>(digit);
        }
        return result;
    }
    std::string string()
    {
        expect('"');
        std::string result;
        while (pos_ < text_.size())
        {
            const auto ch = static_cast<unsigned char>(text_[pos_++]);
            if (ch == '"')
            {
                return result;
            }
            if (ch < 0x20)
            {
                fail();
            }
            if (ch != '\\')
            {
                result += static_cast<char>(ch);
                continue;
            }
            if (pos_ == text_.size())
            {
                fail();
            }
            switch (text_[pos_++])
            {
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
            case 'u':
            {
                unsigned code = hex();
                if (code >= 0xd800 && code <= 0xdbff)
                {
                    if (pos_ + 2 > text_.size() || text_.substr(pos_, 2) != "\\u")
                    {
                        fail();
                    }
                    pos_ += 2;
                    const unsigned low = hex();
                    if (low < 0xdc00 || low > 0xdfff)
                    {
                        fail();
                    }
                    code = 0x10000 + (code - 0xd800) * 1024 + low - 0xdc00;
                }
                else if (code >= 0xdc00 && code <= 0xdfff)
                {
                    fail();
                }
                if (code < 0x80)
                {
                    result += static_cast<char>(code);
                }
                else
                {
                    if (code >= 0x10000)
                    {
                        result += static_cast<char>(0xf0 | (code >> 18));
                    }
                    if (code >= 0x800)
                    {
                        result += static_cast<char>((code >= 0x10000 ? 0x80 : 0xe0) |
                                                    ((code >> 12) & 0x3f));
                    }
                    result +=
                        static_cast<char>((code >= 0x800 ? 0x80 : 0xc0) | ((code >> 6) & 0x3f));
                    result += static_cast<char>(0x80 | (code & 0x3f));
                }
                break;
            }
            default:
                fail();
            }
        }
        fail();
    }
    JsonValue value(int depth)
    {
        if (depth > 32)
        {
            fail();
        }
        space();
        if (pos_ == text_.size())
        {
            fail();
        }
        JsonValue result;
        if (take('{'))
        {
            result.kind = JsonValue::Kind::Object;
            if (!take('}'))
            {
                do
                {
                    auto key = string();
                    expect(':');
                    if (!result.members.emplace(key, value(depth + 1)).second)
                    {
                        fail();
                    }
                } while (take(','));
                expect('}');
            }
        }
        else if (take('['))
        {
            result.kind = JsonValue::Kind::Array;
            if (!take(']'))
            {
                do
                {
                    result.elements.push_back(value(depth + 1));
                } while (take(','));
                expect(']');
            }
        }
        else if (text_[pos_] == '"')
        {
            result.kind = JsonValue::Kind::String;
            result.text = string();
        }
        else if (text_[pos_] == '-' || std::isdigit(static_cast<unsigned char>(text_[pos_])))
        {
            result.kind = JsonValue::Kind::Number;
            const auto start = pos_;
            if (text_[pos_] == '-')
            {
                ++pos_;
            }
            if (pos_ == text_.size())
            {
                fail();
            }
            if (text_[pos_] == '0')
            {
                ++pos_;
            }
            else
            {
                if (text_[pos_] < '1' || text_[pos_] > '9')
                {
                    fail();
                }
                digits();
            }
            if (pos_ < text_.size() && text_[pos_] == '.')
            {
                ++pos_;
                digits();
            }
            if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E'))
            {
                ++pos_;
                if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
                {
                    ++pos_;
                }
                digits();
            }
            result.text = text_.substr(start, pos_ - start);
        }
        else
        {
            bool found = false;
            for (const auto* literal : {"null", "true", "false"})
            {
                const std::string word(literal);
                if (text_.compare(pos_, word.size(), word) == 0)
                {
                    result.kind = word == "null" ? JsonValue::Kind::Null : JsonValue::Kind::Boolean;
                    result.text = word;
                    pos_ += word.size();
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                fail();
            }
        }
        return result;
    }
    void digits()
    {
        const auto start = pos_;
        while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
        {
            ++pos_;
        }
        if (pos_ == start)
        {
            fail();
        }
    }
};
} // namespace

/**
 * Parses a JSON string and returns a JsonValue representing the parsed data.
 * @param text The JSON string to parse.
 * @return A JsonValue representing the parsed JSON data.
 */
JsonValue parseJson(const std::string& text)
{
    return Parser(text).parse();
}

/**
 * Quotes a string for JSON output, escaping special characters as needed.
 * @param text The string to quote.
 * @return The quoted string suitable for JSON output.
 */
std::string jsonQuote(const std::string& text)
{
    std::string result = "\"";
    for (std::size_t i = 0; i < text.size();)
    {
        const auto length = utf8Length(text, i);
        if (length == 0)
        {
            result += "\\ufffd";
            ++i;
            continue;
        }
        if (length > 1)
        {
            result.append(text, i, length);
            i += length;
            continue;
        }
        const auto ch = static_cast<unsigned char>(text[i++]);
        switch (ch)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (ch < 0x20 || ch == 0x7f)
            {
                std::ostringstream escaped;
                escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch);
                result += escaped.str();
            }
            else
            {
                result += static_cast<char>(ch);
            }
        }
    }
    return result + "\"";
}
} // namespace cli
