#include "http_server.h"

#include <fcntl.h>
#include <glog/logging.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <sys/stat.h>

namespace blokus {

HttpServer::HttpServer() {
  port_ = 7777;
}

HttpServer::~HttpServer() {
  Shutdown();
}

void HttpServer::RegisterStaticHandler(StringPiece uri, StringPiece path) {
  CHECK(handlers_.count(uri.data()) == 0);
  handlers_[uri.ToString()].static_path = path.ToString();
}

void HttpServer::RegisterHandler(StringPiece uri, HandlerCB handler) {
  // TODO(piotrf): do some verification on the uri.
  CHECK(handlers_.count(uri.data()) == 0);
  handlers_[uri.ToString()].handler = handler;
}

bool HttpServer::Start() {
  d_ = MHD_start_daemon(
      MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
      port_,
      nullptr, /* client connect callback */
      nullptr, /* clienct connect callback arguments */
      &HttpServer::HandleRequest, this, MHD_OPTION_END);
  if (!d_) return false;

  return true;
}

void HttpServer::Shutdown() {
  if (d_) {
    MHD_stop_daemon(d_);
  }
}

int HttpServer::Return404(MHD_Connection* connection) {
  const char* data = "<html>Nothing found!<html>";
  MHD_Response* response = MHD_create_response_from_buffer(
      strlen(data), const_cast<char*>(data), MHD_RESPMEM_PERSISTENT);
  int ret = MHD_queue_response(connection, 404, response);
  MHD_destroy_response(response);
  return ret;
}

namespace {

}  // namespace

class PostProcessor {
 public:
  static std::unique_ptr<PostProcessor> Create(MHD_Connection* connection) {
    const char* encoding = MHD_lookup_connection_value(
        connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
    if (!strcmp(encoding, "application/json")) {
      return std::unique_ptr<PostProcessor>(new PostProcessor);
    }
    LOG(ERROR) << "Can't handle encoding: " << encoding;
    return nullptr;
  }

  void AddData(StringPiece upload_data) {
    data_ += upload_data.ToString();
  }

  bool Finalize() {
    json_.reset(new Json::Value);
    Json::Reader reader;
    if (!reader.parse(data_, *json_)) {
      LOG(ERROR) << "Failed to parse json data."
                 << reader.getFormattedErrorMessages();
      return false;
    }
    return true;
  }

  std::unique_ptr<Json::Value> Consume() {
    return std::move(json_);
  }

 private:
  PostProcessor() {}

  std::string data_;
  std::unique_ptr<Json::Value> json_;
};

// TODO(piotrf): make this a shim, like IteratePost above.
/* static */
int HttpServer::HandleRequest(
    void* cls, MHD_Connection* connection, const char* url,
    const char* method, const char* version, const char* upload_data,
    size_t* upload_data_size, void** con_cls __attribute__ ((unused))) {
  HttpServer* server = static_cast<HttpServer*>(cls);

  // TODO(piotrf): url params parsing.
  if (server->handlers_.count(url) == 0) {
    LOG(INFO) << "404 for url: " << url;
    return server->Return404(connection);
  }

  const HandlerInfo& info = server->handlers_.at(url);
  MHD_Response* response = nullptr;
  int status = 200;

  if (!strcmp(method, "POST")) {
    if (server->post_processor_ == nullptr) {
      server->post_processor_ = PostProcessor::Create(connection);
      return server->post_processor_ == nullptr ? MHD_NO : MHD_YES;
    } else {
      if (*upload_data_size != 0) {
        server->post_processor_->AddData(
            StringPiece(upload_data, *upload_data_size));
        // TODO(piotrf): above should return how much we eat up.
        *upload_data_size = 0;
        return MHD_YES;
      } else {
        CHECK(server->post_processor_->Finalize());
        
        Request request(method, version);
        request.set_json(server->post_processor_->Consume());
        server->handlers_[url].handler(&request);
        
        response = MHD_create_response_from_buffer(
            request.response().size(),
            const_cast<char*>(request.response().c_str()),
            MHD_RESPMEM_MUST_COPY);
        status = request.status();

        server->post_processor_.reset();
      }
    }
  } else if (!strcmp(method, "GET")) {
    if (info.static_path.empty()) {
      // Regular request handler.
      // TODO(piotrf): no upload data here.
      Request request(method, version);
      server->handlers_[url].handler(&request);
  
      response = MHD_create_response_from_buffer(
          request.response().size(),
          const_cast<char*>(request.response().c_str()), MHD_RESPMEM_MUST_COPY);
      status = request.status();
    } else {
      // Simply serve a static file, use create_response_from_fd so that we can
      // take advantage of sendfile.
      // TODO(piotrf): cache the fd, or even file contents.
      int fd = open(info.static_path.c_str(), O_RDONLY);
      if (fd == -1) {
        // TODO(piotrf): not really 404, more like 500. Need a generic error
        // handler.
        return server->Return404(connection);
      }
      struct stat s;
      CHECK(fstat(fd, &s) != -1);
      response = MHD_create_response_from_fd(s.st_size, fd);
      status = 200;
    }
  }
  int ret = MHD_queue_response(connection, status, response);
  MHD_destroy_response(response);
  return ret;
}


}  // namespace blokus
