#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace oceandl {

using HeaderMap = std::map<std::string, std::string>;

struct HttpRequest {
    std::string url;
    std::vector<std::string> headers;
    double timeout_seconds = 60.0;
    std::uint64_t buffer_size_bytes = 0;
};

struct HttpResponse {
    int status_code = 0;
    HeaderMap headers;
    std::size_t bytes_transferred = 0;
};

class ResponseHandler {
  public:
    virtual ~ResponseHandler() = default;
    virtual void on_response_start(const HttpResponse& response) = 0;
    virtual bool on_chunk(std::string_view chunk) = 0;
};

class NetworkError : public std::runtime_error {
  public:
    NetworkError(std::string message, int code);

    int code() const;

  private:
    int code_;
};

class HttpStatusError : public std::runtime_error {
  public:
    HttpStatusError(int status_code, std::string url);

    int status_code() const;
    const std::string& url() const;

  private:
    int status_code_;
    std::string url_;
};

class IHttpClient {
  public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse head(const HttpRequest& request) = 0;
    virtual HttpResponse get(const HttpRequest& request, ResponseHandler& handler) = 0;
};

class CurlHttpClient final : public IHttpClient {
  public:
    explicit CurlHttpClient(std::string user_agent);

    HttpResponse head(const HttpRequest& request) override;
    HttpResponse get(const HttpRequest& request, ResponseHandler& handler) override;

  private:
    std::string user_agent_;
};

}  // namespace oceandl
