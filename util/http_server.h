#ifndef BLOKUS_UTIL_HTTP_SERVER_H
#define BLOKUS_UTIL_HTTP_SERVER_H

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include <jsoncpp/json/value.h>
#include <microhttpd.h>

namespace blokus {

class PostProcessor;
  
class HttpServer {
 public:
  HttpServer();
  ~HttpServer();
  
  class Request {
   public:
    Request(absl::string_view method, absl::string_view version) :
        method_(method), version_(version) {}
    
    absl::string_view method() const { return method_; }
    absl::string_view version() const { return version_; }

    // TODO(piotrf): something better than string.
    const std::string& response() const { return response_; }
    void set_response(std::string response) { response_ = std::move(response); }
    
    int status() const { return status_; }
    void set_status(int status) { status_ = status; }

    // Post data access.
    // Super-janky for now, can just access json data. In the future, this
    // should somehow be templated / streamed (via some helper?).
    bool has_json() { return json_ != nullptr; }
    const Json::Value& json() { return *json_; }
    void set_json(std::unique_ptr<Json::Value> json) {
      json_ = std::move(json);
    }
    
   private:
    // None of these are owned by HttpRequest.
    absl::string_view method_;
    absl::string_view version_;
    int status_;
    std::string response_;
    std::unique_ptr<Json::Value> json_;
  };
  
  typedef std::function<void(Request*)> HandlerCB;

  // TODO(piotrf): serve static content from an entire directory.
  void RegisterStaticHandler(absl::string_view uri, absl::string_view path);
  void RegisterHandler(absl::string_view uri, HandlerCB handler);

  // Start the background server thread, returns false on error.
  bool Start();

 private:
  int Return404(MHD_Connection* connection);
  
  static int HandleRequest(
    void* cls, MHD_Connection* connection, const char* url,
    const char* method, const char* version, const char* upload_data,
    size_t* upload_data_size, void** con_cls);
  
  void Shutdown();
  
  int port_;
  MHD_Daemon* d_ = nullptr;

  struct HandlerInfo {
    std::string static_path;
    HandlerCB   handler;
  };
  std::map<std::string, HandlerInfo> handlers_;

  std::unique_ptr<PostProcessor> post_processor_;
};

}  // namespace blokus

#endif
