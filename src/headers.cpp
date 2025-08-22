#include "headers.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <ranges>
#include <string_view>
#include <print>
#include <iostream>

using namespace std::string_view_literals;

std::string repr(std::string_view view) {
    std::string result = "";
    for (char c : view) {
        if (c == '\r') {
            result += "\\r";
        } else if (c == '\n') {
            result += "\\n";
        } else {
            result += c;
        }
    }
    return result;
}

void iterHeaders(std::string_view req, Callback &&callback) {
    size_t request_eor = req.find(delimiter);
    size_t request_first_line_end = req.find(single_delimiter);

    std::string_view request_headers_lines =
        req.substr(request_first_line_end + 2, request_eor - request_first_line_end - 2);

    if (request_headers_lines == single_delimiter) {
        return;
    }

    std::ranges::for_each(
        request_headers_lines | std::ranges::views::split(std::string_view(single_delimiter)) |
            std::views::transform([](auto &&r) {
                auto trim = [](std::string_view str) {
                    while (!str.empty() && std::isspace(str.front())) {
                        str.remove_prefix(1);
                    }
                    while (!str.empty() && std::isspace(str.back())) {
                        str.remove_suffix(1);
                    }
                    return str;
                };

                std::string_view line(r);
                size_t pos = line.find(':');
                if (pos == std::string_view::npos) {
                    throw std::runtime_error("Headers must have a colon");
                }
                return std::pair{trim(line.substr(0, pos)), trim(line.substr(pos + 1))};
            }),
        [callback = std::move(callback)](const auto &pair) { callback(std::get<0>(pair), std::get<1>(pair)); });
}

std::optional<std::pair<std::string_view, std::string_view>> findHostPort(std::string_view req) {
    std::unordered_map<std::string_view, std::string_view> headers;
    iterHeaders(req, [&headers](std::string_view key, std::string_view value) {
        headers.emplace(key, value);
    });
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

std::optional<size_t> findContentLength(std::string_view req) {
    std::unordered_map<std::string_view, std::string_view> headers;
    iterHeaders(req, [&headers](std::string_view key, std::string_view value) {
        headers.emplace(key, value);
    });
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
