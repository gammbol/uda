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
        try {
            if (!fbc.connect()) {
                // ОШИБКА: не используйте new при throw!
                throw std::runtime_error("Failed to connect to firebird");
            }

            // Получаем данные из БД
            std::vector<crow::json::wvalue> boards = fbc.getSQL("SELECT * FROM boards");

            // ПРЕОБРАЗУЕМ вектор в JSON-массив
            crow::json::wvalue result_json;
            for (size_t i = 0; i < boards.size(); i++) {
                result_json[i] = std::move(boards[i]);
            }

            // Возвращаем JSON напрямую - Crow знает как его обработать
            return result_json;

        } catch (const std::exception &e) {
            std::cerr << "Error in /api/boards: " << e.what() << std::endl;

            return crow::response({"fdlskjfl"});
        }
    });

    app.port(8080).multithreaded().run();

    return 0;
}