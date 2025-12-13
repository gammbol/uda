#ifndef FB_CONNECT_H
#define FB_CONNECT_H

#include <firebird/Interface.h>
#include <string>
#include <vector>
#include <iostream>
#include <crow/json.h>
#include <mutex>

struct FBConnectionStruct {
  std::string db_path;
  std::string db_user;
  std::string db_password;
  std::string db_host;
  std::string db_port;
};

class FirebirdConnection {
private:
  static std::mutex fb_mutex_;

  bool isConnected = false;

  // api
  Firebird::IMaster* master = nullptr;
  Firebird::IStatus* rawStatus = nullptr;  // Переименован, чтобы было понятнее
  Firebird::IUtil* utl = nullptr;
  Firebird::IXpbBuilder* dpb = nullptr;

  // connection
  Firebird::IProvider* provider = nullptr;
  Firebird::IAttachment* attachment = nullptr;

  FBConnectionStruct config;

  void fbInit();

public:
  explicit FirebirdConnection(FBConnectionStruct  conf);
  ~FirebirdConnection();

  void newConnection(FBConnectionStruct conf);

  bool connect();
  void disconnect();

  std::vector<crow::json::wvalue> getSQL(const std::string& query);
};

#endif // FB_CONNECT_H