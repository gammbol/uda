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

    // FirebirdConnection fbc(fb_conf);
    //
    // std::cout << "Connecting to Firebird database..." << std::endl;
    // if (!fbc.connect()) {
    //     std::cerr << "FATAL: Failed to connect to database on startup!" << std::endl;
    //     return 1; // Завершаем приложение, если не удалось подключиться
    // }
    // std::cout << "Successfully connected to database!" << std::endl;

    CROW_ROUTE(app, "/api/boards")([&]() {
        try {
            FirebirdConnection fbc(fb_conf);
            if (!fbc.connect()) {
                throw std::runtime_error("Failed to connect to Firebird database");
            }

            // Получаем данные из БД
            std::vector<crow::json::wvalue> boards = fbc.getSQL("SELECT * FROM DEVICES");
            fbc.disconnect();

            if (boards.empty())
                return crow::response(404, "Not found");

            // ПРЕОБРАЗУЕМ вектор в JSON-массив
            crow::json::wvalue result_json;
            for (size_t i = 0; i < boards.size(); i++) {
                result_json[i] = std::move(boards[i]);
            }

            // Возвращаем JSON напрямую - Crow знает как его обработать
            return crow::response(200, result_json);

        } catch (const std::exception &e) {
            std::cerr << "Error in /api/boards: " << e.what() << std::endl;

            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/api/boards/<int>")([&](int id) {
        try {
            FirebirdConnection fbc(fb_conf);
            if (!fbc.connect()) {
                throw std::runtime_error("Failed to connect to Firebird database");
            }

            std::string query = "SELECT * FROM DEVICES WHERE DEVICE_ID=" + std::to_string(id);
            std::cout << query << std::endl;

            std::vector<crow::json::wvalue> board = fbc.getSQL(query);
            // crow::json::wvalue result_json;
            // result_json[0] = std::move(board[0]);
            fbc.disconnect();

            if (board.empty())
                return crow::response(404, "Not found");

            return crow::response(200, board[0]);
        } catch (const std::exception &e) {
            std::cerr << "Error in /api/boards: " << e.what() << std::endl;
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/api/sessions/<int>")([&](int id) {
        try {
            FirebirdConnection fbc(fb_conf);
            if (!fbc.connect()) {
                throw std::runtime_error("Failed to connect to Firebird database");
            }

            std::string query = "SELECT * FROM RD2_SESSIONS WHERE DEVICE_ID=" + std::to_string(id) + " ORDER BY SESSION_ID DESC";
            std::cout << query << std::endl;

            std::vector<crow::json::wvalue> sessions = fbc.getSQL(query);
            fbc.disconnect();

            if (sessions.empty())
                return crow::response(404, "Not found");

            crow::json::wvalue result_json;
            for (size_t i = 0; i < sessions.size(); i++) {
                result_json[i] = std::move(sessions[i]);
            }

            return crow::response(200, result_json);
        } catch (const std::exception &e) {
            std::cerr << "Error in /api/sessions: " << e.what() << std::endl;

            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/api/session/<int>")([&](int id) {
        try {
            FirebirdConnection fbc(fb_conf);
            if (!fbc.connect()) {
                throw std::runtime_error("Failed to connect to Firebird database");
            }

            std::string query = "SELECT * FROM RD2_SESSIONS WHERE SESSION_ID=" + std::to_string(id);
            std::cout << query << std::endl;

            std::vector<crow::json::wvalue> session = fbc.getSQL(query);
            fbc.disconnect();

            if (session.empty())
                return crow::response(404, "Not found");

            // crow::json::wvalue result_json;
            // for (size_t i = 0; i < session.size(); i++) {
            //     result_json[i] = std::move(session[i]);
            // }

            return crow::response(200, session[0]);
        } catch (const std::exception &e) {
            std::cerr << "Error in /api/session: " << e.what() << std::endl;
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/api/points/<int>")([&](int id) {
        try {
            FirebirdConnection fbc(fb_conf);
            if (!fbc.connect()) {
                throw std::runtime_error("Failed to connect to Firebird database");
            }

            std::string query = "SELECT * FROM RD2_POINTS WHERE SESSION_ID=" + std::to_string(id);
            std::cout << query << std::endl;

            std::vector<crow::json::wvalue> points = fbc.getSQL(query);
            fbc.disconnect();

            if (points.empty())
                return crow::response(404, "Not found");

            crow::json::wvalue result_json;
            for (size_t i = 0; i < points.size(); i++) {
                result_json[i] = std::move(points[i]);
            }

            return crow::response(200, result_json);
        } catch (const std::exception &e) {
            std::cerr << "Error in /api/points: " << e.what() << std::endl;
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/api/param/<int>")([&](int id) {
        try {
            FirebirdConnection fbc(fb_conf);
            if (!fbc.connect()) {
                throw std::runtime_error("Failed to connect to Firebird database");
            }

            std::string query = "SELECT * FROM PASSP_SCAN WHERE DEVICE_ID=" + std::to_string(id);
            std::cout << query << std::endl;

            std::vector<crow::json::wvalue> params = fbc.getSQL(query);
            fbc.disconnect();

            if (params.empty())
                return crow::response(404, "Not found");

            return crow::response(200, params[0]);
        } catch (const std::exception &e) {
            std::cerr << "Error in /api/params: " << e.what() << std::endl;
            return crow::response(500, e.what());
        }
    });

    app.port(CROW_PORT).multithreaded().run();

    return 0;
}