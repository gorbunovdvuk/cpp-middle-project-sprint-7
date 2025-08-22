#pragma once

#include <functional>
#include <optional>
#include <string_view>

using Callback = std::function<void(std::string_view, std::string_view)>;

constexpr std::string_view single_delimiter = "\r\n";
constexpr std::string_view delimiter = "\r\n\r\n";

void iterHeaders(std::string_view req, Callback&& callback);

std::optional<std::pair<std::string_view, std::string_view>> findHostPort(std::string_view req);;

std::optional<size_t> findContentLength(std::string_view req);
