#ifndef FB_CONNECT_H
#define FB_CONNECT_H

#include <firebird/Interface.h>
#include <string>
#include <vector>
#include <format>
#include <iostream>

#include <crow/json.h>

using namespace Firebird;

typedef struct FBConnectionStruct {
  std::string db_path;
  std::string db_user;
  std::string db_password;
  std::string db_host;
  std::string db_port;
} FBConnectionStruct;

class FirebirdConnection {
private:
  bool isConnected = false;

  // api
  IMaster *master;IStatus *st;
  IUtil *utl;
  IXpbBuilder *dpb;

  // connection
  IProvider *provider;
  IAttachment *attachment;

  FBConnectionStruct config;

  void fbInit();

public:
  FirebirdConnection(FBConnectionStruct conf);
  ~FirebirdConnection();

  void newConnection(FBConnectionStruct conf);

  bool connect();
  void disconnect();

  std::vector<crow::json::wvalue> getSQL(std::string query);
};


#endif //FB_CONNECT_H
