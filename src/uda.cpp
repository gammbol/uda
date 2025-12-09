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

    std::cout << "Connecting to Firebird database..." << std::endl;
    if (!fbc.connect()) {
        std::cerr << "FATAL: Failed to connect to database on startup!" << std::endl;
        return 1; // Завершаем приложение, если не удалось подключиться
    }
    std::cout << "Successfully connected to database!" << std::endl;

    CROW_ROUTE(app, "/api/boards")([&fbc]() {
        try {

            // Получаем данные из БД
            std::vector<crow::json::wvalue> boards = fbc.getSQL("SELECT * FROM DEVICES");

            // ПРЕОБРАЗУЕМ вектор в JSON-массив
            crow::json::wvalue result_json;
            for (size_t i = 0; i < boards.size(); i++) {
                result_json[i] = std::move(boards[i]);
            }
            std::cout << result_json.dump() << std::endl;

            // Возвращаем JSON напрямую - Crow знает как его обработать
            return crow::response(200, result_json);

        } catch (const std::exception &e) {
            std::cerr << "Error in /api/boards: " << e.what() << std::endl;

            return crow::response(500, "pizdec :(");
        }
    });

    app.port(8080).multithreaded().run();

    return 0;
}