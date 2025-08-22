#pragma once

#include <string>
#include <functional>
#include <optional>
#include <ranges>

using Callback = std::function<void(std::string_view, std::string_view)>;

class Headers {
public:
    explicit Headers(std::string_view headers): headers_(headers), map_{
        std::from_range,
        headers_ | std::ranges::views::split(std::string_view("\r\n")) | std::views::transform([](auto&& r) {
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
            return std::pair{
                trim(line.substr(0, pos)),
                trim(line.substr(pos + 1))
            };
        })
    } {
    }

    decltype(auto) at(this auto&& self, const std::string_view& key) {
        return std::forward_like<decltype(self)>(self.map_.at(key));
    }

    [[nodiscard]] bool contains(const std::string_view& key) const {
        return map_.contains(key);
    }

    auto begin(this auto&& self) {
        return std::ranges::begin(self.map_);
    }

    auto end(this auto&& self) {
        return std::ranges::end(self.map_);
    }

private:
    std::string headers_;
    std::unordered_map<std::string_view, std::string_view> map_;
};

void iterHeaders(std::string_view req, Callback&& callback);

std::optional<std::pair<std::string_view, std::string_view>> findHostPort(const Headers& headers);

std::optional<size_t> findContentLength(const Headers& headers);
