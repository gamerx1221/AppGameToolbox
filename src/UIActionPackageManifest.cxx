#include <AppGameToolbox/UIActionPackageManifest.hpp>

#include <cctype>
#include <unordered_set>

namespace appgametoolbox {
namespace {

class JsonReader {
public:
    explicit JsonReader(std::string_view text) : m_text(text) {}

    bool objectBegin() { return token('{'); }
    bool arrayBegin() { return token('['); }
    bool objectEnd() { return token('}'); }
    bool arrayEnd() { return token(']'); }
    bool comma() { return token(','); }
    bool colon() { return token(':'); }
    bool at(char value) { skip(); return m_pos < m_text.size() && m_text[m_pos] == value; }
    bool finished() { skip(); return m_pos == m_text.size(); }

    bool string(std::string& value) {
        skip();
        if (m_pos == m_text.size() || m_text[m_pos++] != '"') return fail("expected string");
        value.clear();
        while (m_pos < m_text.size()) {
            const char ch = m_text[m_pos++];
            if (ch == '"') return true;
            if (static_cast<unsigned char>(ch) < 0x20) return fail("control character in string");
            if (ch != '\\') { value += ch; continue; }
            if (m_pos == m_text.size()) return fail("incomplete escape sequence");
            const char escaped = m_text[m_pos++];
            switch (escaped) {
            case '"': value += '"'; break; case '\\': value += '\\'; break; case '/': value += '/'; break;
            case 'b': value += '\b'; break; case 'f': value += '\f'; break; case 'n': value += '\n'; break;
            case 'r': value += '\r'; break; case 't': value += '\t'; break;
            // Keep Unicode escapes valid but unexpanded; manifest identifiers and paths are portable UTF-8 text.
            case 'u':
                if (m_pos + 4 > m_text.size()) return fail("incomplete unicode escape");
                for (int i = 0; i != 4; ++i)
                    if (!std::isxdigit(static_cast<unsigned char>(m_text[m_pos + i]))) return fail("invalid unicode escape");
                value.append("\\u").append(m_text.substr(m_pos, 4));
                m_pos += 4;
                break;
            default: return fail("invalid escape sequence");
            }
        }
        return fail("unterminated string");
    }

    bool boolean(bool& value) {
        skip();
        if (m_text.substr(m_pos, 4) == "true") { m_pos += 4; value = true; return true; }
        if (m_text.substr(m_pos, 5) == "false") { m_pos += 5; value = false; return true; }
        return fail("expected boolean");
    }

    bool skipValue() {
        skip();
        if (m_pos == m_text.size()) return fail("expected value");
        if (m_text[m_pos] == '"') { std::string value; return string(value); }
        if (m_text[m_pos] == '{') return skipObject();
        if (m_text[m_pos] == '[') return skipArray();
        if (m_text.substr(m_pos, 4) == "true") { m_pos += 4; return true; }
        if (m_text.substr(m_pos, 5) == "false") { m_pos += 5; return true; }
        if (m_text.substr(m_pos, 4) == "null") { m_pos += 4; return true; }
        return number();
    }

    const std::string& error() const { return m_error; }

private:
    bool token(char value) { skip(); if (m_pos < m_text.size() && m_text[m_pos] == value) { ++m_pos; return true; } return fail("unexpected JSON token"); }
    void skip() { while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos; }
    bool fail(const char* message) { if (m_error.empty()) m_error = message; return false; }
    bool skipObject() {
        if (!objectBegin()) return false;
        if (objectEnd()) return true;
        while (true) { std::string key; if (!string(key) || !colon() || !skipValue()) return false; if (objectEnd()) return true; if (!comma()) return false; }
    }
    bool skipArray() {
        if (!arrayBegin()) return false;
        if (arrayEnd()) return true;
        while (true) { if (!skipValue()) return false; if (arrayEnd()) return true; if (!comma()) return false; }
    }
    bool number() {
        const std::size_t start = m_pos;
        if (m_pos < m_text.size() && m_text[m_pos] == '-') ++m_pos;
        if (m_pos == m_text.size()) return fail("invalid number");
        if (m_text[m_pos] == '0') ++m_pos;
        else if (std::isdigit(static_cast<unsigned char>(m_text[m_pos])))
            while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
        else return fail("invalid value");
        if (m_pos < m_text.size() && m_text[m_pos] == '.') {
            ++m_pos;
            const std::size_t fraction = m_pos;
            while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
            if (m_pos == fraction) return fail("invalid number");
        }
        if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-')) ++m_pos;
            const std::size_t exponent = m_pos;
            while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
            if (m_pos == exponent) return fail("invalid number");
        }
        return m_pos != start;
    }
    std::string_view m_text;
    std::size_t m_pos = 0;
    std::string m_error;
};

bool requireString(JsonReader& reader, const std::string& key, std::string& value) {
    (void)key;
    return reader.string(value) && !value.empty();
}

bool parseRoute(JsonReader& reader, UIActionPackageRoute& route) {
    if (!reader.objectBegin()) return false;
    bool id = false, html = false, css = false;
    if (reader.objectEnd()) return false;
    while (true) {
        std::string key;
        if (!reader.string(key) || !reader.colon()) return false;
        if (key == "id") id = requireString(reader, key, route.id);
        else if (key == "html") html = requireString(reader, key, route.html);
        else if (key == "css") css = requireString(reader, key, route.css);
        else if (key == "preload") { if (!reader.boolean(route.preload)) return false; }
        else if (!reader.skipValue()) return false;
        if (reader.objectEnd()) break;
        if (!reader.comma()) return false;
    }
    return id && html && css;
}

bool parseAsset(JsonReader& reader, UIActionPackageAsset& asset) {
    if (!reader.objectBegin()) return false;
    bool id = false, path = false;
    if (reader.objectEnd()) return false;
    while (true) {
        std::string key;
        if (!reader.string(key) || !reader.colon()) return false;
        if (key == "id") id = requireString(reader, key, asset.id);
        else if (key == "path") path = requireString(reader, key, asset.path);
        else if (!reader.skipValue()) return false;
        if (reader.objectEnd()) break;
        if (!reader.comma()) return false;
    }
    return id && path;
}

template <typename T, typename Parse>
bool parseArray(JsonReader& reader, std::vector<T>& values, Parse parse) {
    if (!reader.arrayBegin()) return false;
    if (reader.arrayEnd()) return true;
    while (true) { T value; if (!parse(reader, value)) return false; values.push_back(std::move(value)); if (reader.arrayEnd()) return true; if (!reader.comma()) return false; }
}
}

bool parseUIActionPackageManifest(std::string_view text, UIActionPackageManifest& manifest, std::string& error) {
    JsonReader reader(text);
    UIActionPackageManifest parsed;
    bool packageId = false, entryRoute = false, localization = false, routes = false, assets = false;
    if (!reader.objectBegin()) { error = reader.error(); return false; }
    if (reader.objectEnd()) { error = "manifest is empty"; return false; }
    while (true) {
        std::string key;
        if (!reader.string(key) || !reader.colon()) { error = reader.error(); return false; }
        if (key == "packageId") packageId = requireString(reader, key, parsed.packageId);
        else if (key == "entryRoute") entryRoute = requireString(reader, key, parsed.entryRoute);
        else if (key == "localization") localization = requireString(reader, key, parsed.localization);
        else if (key == "routes") routes = parseArray(reader, parsed.routes, parseRoute);
        else if (key == "assets") assets = parseArray(reader, parsed.assets, parseAsset);
        else if (!reader.skipValue()) { error = reader.error(); return false; }
        if (reader.objectEnd()) break;
        if (!reader.comma()) { error = reader.error(); return false; }
    }
    if (!reader.finished()) { error = "trailing JSON content"; return false; }
    if (!packageId || !entryRoute || !localization || !routes || !assets || parsed.routes.empty()) { error = "manifest is missing a required member"; return false; }
    std::unordered_set<std::string> routeIds, assetIds;
    bool hasEntryRoute = false;
    for (const auto& route : parsed.routes) { if (!routeIds.insert(route.id).second) { error = "duplicate route id"; return false; } hasEntryRoute = hasEntryRoute || route.id == parsed.entryRoute; }
    for (const auto& asset : parsed.assets) if (!assetIds.insert(asset.id).second) { error = "duplicate asset id"; return false; }
    if (!hasEntryRoute) { error = "entry route is not declared"; return false; }
    manifest = std::move(parsed);
    error.clear();
    return true;
}

} // namespace appgametoolbox
