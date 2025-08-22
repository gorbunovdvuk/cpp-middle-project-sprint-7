#include "headers.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <ranges>
#include <string_view>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(const Headers& headers, Callback&& callback) {
    std::ranges::for_each(headers, [callback = std::move(callback)](const auto& pair) {
        return callback(std::get<0>(pair), std::get<1>(pair));
    });
}

std::optional<std::pair<std::string_view, std::string_view>> findHostPort(const Headers& headers) {
    if (!headers.contains("Host")) {
        return std::nullopt;
    }
    std::string_view host_port_line = headers.at("Host");
    size_t colon_pos = host_port_line.find(':');
    if (colon_pos == std::string_view::npos) {
        throw std::runtime_error("invalid host port line");
    }
    std::string_view host = host_port_line.substr(0, colon_pos);
    std::string_view port = host_port_line.substr(colon_pos + 1);
    return {{host, port}};
}

std::optional<size_t> findContentLength(const Headers& headers) {
    if (headers.contains("Content-Length")) {
        std::string_view content_length_line = headers.at("Content-Length");
        size_t result = 0;
        if (auto [end_p, errc] = std::from_chars(content_length_line.data(), content_length_line.data() + content_length_line.size(), result); errc != std::errc{}) {
            return std::nullopt;
        }
        return result;
    }
    return std::nullopt;
}
