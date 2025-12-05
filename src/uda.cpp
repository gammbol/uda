#include <uda.h>

int main()
{
  using namespace Firebird;

  crow::SimpleApp app;

  // firebird connect
  FBConnectionStruct fb_conf = {
    "C:\\base\\TSDSQL.FDB",
    "UDR",
    "tsdsql",
    "10.105.160.98",
    "3050"
  };
  FirebirdConnection fbc(fb_conf);


  CROW_ROUTE(app, "/api/boards")([&fbc]() {
    std::vector<crow::json::wvalue> boards{};
    try {
      if (!fbc.connect())
        throw new std::runtime_error("Failed to connect to firebird");

      boards = fbc.getSQL("SELECT * FROM boards");
    } catch (const std::exception &e) {
      std::cout << e.what() << std::endl;
    }

    return boards;
  });

  app.port(8080).multithreaded().run();


  return 0;
}