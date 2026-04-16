#pragma once

#include <functional>
#include <map>
#include <vector>

#include "Arduino.h"

enum HTTPMethod : uint8_t {
  HTTP_GET = 0,
  HTTP_POST = 1,
};

enum HTTPAuthMethod : uint8_t {
  BASIC_AUTH = 0,
};

struct FakeHttpResponse {
  int statusCode = 0;
  String contentType;
  String body;
  std::map<std::string, std::string> headers;
};

class WebServer {
 public:
  using Handler = std::function<void()>;

  explicit WebServer(int port = 80);

  void on(const String& path, HTTPMethod method, Handler handler);
  void onNotFound(Handler handler);

  void begin();
  void stop();
  void handleClient();

  bool authenticate(const char* user, const char* password);
  void requestAuthentication(HTTPAuthMethod, const char* realm, const char* message);

  void send(int code, const char* contentType, const String& body);
  void sendHeader(const char* name, const char* value, bool first = false);
  String arg(const char* name) const;

  FakeHttpResponse simulateRequest(HTTPMethod method, const String& path,
                                   const std::map<std::string, String>& args = {},
                                   const String& authUser = String(),
                                   const String& authPassword = String());

  static WebServer* lastInstance();

 private:
  struct RouteKey {
    std::string path;
    HTTPMethod method = HTTP_GET;

    bool operator<(const RouteKey& other) const {
      return path < other.path || (path == other.path && method < other.method);
    }
  };

  int port_ = 80;
  bool started_ = false;
  Handler notFoundHandler_;
  std::map<RouteKey, Handler> routes_;

  HTTPMethod currentMethod_ = HTTP_GET;
  String currentPath_;
  std::map<std::string, String> currentArgs_;
  String currentAuthUser_;
  String currentAuthPassword_;
  FakeHttpResponse currentResponse_;

  static WebServer* lastInstance_;
};
