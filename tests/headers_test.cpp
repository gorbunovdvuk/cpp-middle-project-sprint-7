#include <gtest/gtest.h>
#include "headers.h"

using namespace std::string_view_literals;

TEST(iterHeaders, Empty) {
    bool empty = true;
    iterHeaders("\r\n\r\n"sv, [&empty](auto...) { empty = false; });
    EXPECT_TRUE(empty);
}

TEST(iterHeaders, SkipRequestLine) {
    bool empty = true;
    const std::string query = "GET /path?q=1 HTTP/1.1\r\n\r\n";
    iterHeaders(query, [&empty](std::string_view key, std::string_view value) {
        empty = false;
    });
    EXPECT_TRUE(empty);
}

TEST(iterHeaders, SingleHeader) {
    std::vector<std::pair<std::string_view, std::string_view>> headers;
    const std::string query = "GET /path?q=1 HTTP/1.1\r\nHost: 127.0.0.1:8000\r\n\r\n";
    iterHeaders(query, [&headers](std::string_view key, std::string_view value) {
        headers.emplace_back(key, value);
    });
    EXPECT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "127.0.0.1:8000");
}

TEST(iterHeaders, MultipleHeaders) {
    std::vector<std::pair<std::string_view, std::string_view>> headers;
    const std::string query = "GET /path?q=1 HTTP/1.1\r\nHost: 127.0.0.1:8000\r\nUser-Agent: test\r\n\r\n";
    iterHeaders(query, [&headers](std::string_view key, std::string_view value) {
        headers.emplace_back(key, value);
    });
    EXPECT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "127.0.0.1:8000");
    EXPECT_EQ(headers[1].first, "User-Agent");
    EXPECT_EQ(headers[1].second, "test");
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::vector<std::pair<std::string_view, std::string_view>> headers;
    const std::string query = "GET /path?q=1 HTTP/1.1\r\nHost: 127.0.0.1:8000\r\nHost: test\r\n\r\n";
    iterHeaders(query, [&headers](std::string_view key, std::string_view value) {
        headers.emplace_back(key, value);
    });
    EXPECT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "127.0.0.1:8000");
    EXPECT_EQ(headers[1].first, "Host");
    EXPECT_EQ(headers[1].second, "test");
}

TEST(findHostPort, Simple) {
    const std::string query = "GET /path?q=1 HTTP/1.1\r\nHost: 127.0.0.1:8000\r\n\r\n";
    EXPECT_EQ(findHostPort(query).value(), std::pair("127.0.0.1"sv, "8000"sv));
}

TEST(findHostPort, NoHost) {
    const std::string query = "GET /path?q=1 HTTP/1.1\r\nUser-Agent: test\r\n\r\n";
    EXPECT_EQ(findHostPort(query), std::nullopt);
}

TEST(findContentLength, Simple) {
    const std::string query = "GET /path?q=1 HTTP/1.1\r\nContent-Length: 127\r\n\r\n";
    EXPECT_EQ(findContentLength(query).value(), 127);
}

TEST(findContentLength, NoContentLength) {
    const std::string query = "GET /path?q=1 HTTP/1.1\r\n\r\n";
    EXPECT_EQ(findContentLength(query), std::nullopt);
}
