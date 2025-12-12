#include "fb_connect.h"
#include <format>
#include <sstream>
#include <utility>

void FirebirdConnection::fbInit() {
    using namespace Firebird;

    try {
        // 1. Получаем master интерфейс
        master = fb_get_master_interface();
        if (!master) {
            throw std::runtime_error("Cannot get master interface");
        }

        // 2. Создаем статус
        rawStatus = master->getStatus();
        if (!rawStatus) {
            throw std::runtime_error("Failed to get status");
        }

        // 3. Получаем утилиты
        utl = master->getUtilInterface();

        // 4. Создаем DPB builder, используя ThrowStatusWrapper
        ThrowStatusWrapper status(rawStatus);
        dpb = utl->getXpbBuilder(&status, IXpbBuilder::DPB, nullptr, 0);

        if (!dpb) {
            throw std::runtime_error("Failed to create DPB builder");
        }

        std::cout << "Firebird API initialized successfully" << std::endl;

    } catch (const FbException& e) {
        std::cerr << "Firebird initialization failed: " << e.getStatus() << std::endl;
        throw;
    } catch (const std::exception& e) {
        std::cerr << "Initialization error: " << e.what() << std::endl;
        throw;
    }
}

FirebirdConnection::FirebirdConnection(FBConnectionStruct  conf) : config(std::move(conf)) {
    fbInit();
    // provider и attachment инициализируются в connect()
}

FirebirdConnection::~FirebirdConnection() {
    disconnect();

    // Освобождаем ресурсы, созданные в fbInit
    if (dpb) {
        dpb->dispose();
        dpb = nullptr;
    }

    if (rawStatus) {
        rawStatus->dispose();
        rawStatus = nullptr;
    }

    // master не освобождаем - это глобальный синглтон
}

void FirebirdConnection::newConnection(FBConnectionStruct conf) {
    disconnect();
    config = conf;
    fbInit();
}

bool FirebirdConnection::connect() {
    using namespace Firebird;

    if (!rawStatus || !dpb || !master) {
        std::cerr << "Firebird not properly initialized" << std::endl;
        return false;
    }

    try {
        // Создаем обертку статуса для всех операций
        ThrowStatusWrapper status(rawStatus);

        // clearing dpb
        dpb->clear(&status);
        dpb->insertInt(&status, isc_dpb_page_size, 4 * 1024);
        dpb->insertString(&status, isc_dpb_user_name, config.db_user.c_str());
        dpb->insertString(&status, isc_dpb_password, config.db_password.c_str());
        dpb->insertInt(&status, isc_dpb_sql_dialect, 3);

        // connection string
        std::string connection_string = config.db_host + "/" + config.db_port + ":" + config.db_path;

        // getting provider interface
        provider = master->getDispatcher();
        if (!provider) {
            throw std::runtime_error("Failed to get provider");
        }

        // connecting
        attachment = provider->attachDatabase(&status,
            connection_string.c_str(),
            dpb->getBufferLength(&status),
            dpb->getBuffer(&status));

        if (attachment) {
            isConnected = true;
            std::cout << "Connected to database successfully" << std::endl;
            return true;
        } else {
            throw std::runtime_error("Failed to attach database");
        }

    } catch (const FbException& e) {
        std::cerr << "Firebird connection error: " << e.getStatus() << std::endl;
        isConnected = false;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        isConnected = false;
        return false;
    }
}

void FirebirdConnection::disconnect() {
    if (!isConnected) return;

    if (attachment) {
        try {
            Firebird::ThrowStatusWrapper status(rawStatus);
            attachment->detach(&status);
        } catch (...) {
            // Игнорируем ошибки при отключении
        }
        attachment->release();
        attachment = nullptr;
    }

    provider = nullptr;
    isConnected = false;
    std::cout << "Disconnected from database" << std::endl;
}

std::vector<crow::json::wvalue> FirebirdConnection::getSQL(const std::string& query) {
    using namespace Firebird;

    if (!isConnected || !attachment) {
        throw std::runtime_error("FirebirdConnection is not connected");
    }

    std::vector<crow::json::wvalue> result_json;
    ITransaction* transaction = nullptr;
    IStatement* stmt = nullptr;
    IMessageMetadata* meta = nullptr;
    IResultSet* rs = nullptr;
    unsigned char* buffer = nullptr;

    // ВАЖНО: Создаем новый объект статуса для этой операции
    IStatus* localStatus = master->getStatus();
    ThrowStatusWrapper statusWrapper(localStatus);

    try {
        // 1. Начинаем транзакцию
        transaction = attachment->startTransaction(&statusWrapper, 0, nullptr);

        // 2. Подготавливаем запрос
        stmt = attachment->prepare(&statusWrapper, transaction, 0,
                                   query.c_str(), SQL_DIALECT_V6, 0);

        // 3. Получаем метаданные результата
        meta = stmt->getOutputMetadata(&statusWrapper);

        // 4. Вычисляем размер буфера
        unsigned int msg_len = meta->getMessageLength(&statusWrapper);
        buffer = new unsigned char[msg_len];

        // 5. Открываем курсор
        rs = stmt->openCursor(&statusWrapper, transaction, nullptr, nullptr, meta, 0);

        // 6. Читаем строки
        while (rs->fetchNext(&statusWrapper, buffer) == IStatus::RESULT_OK) {
            crow::json::wvalue row_json;

            for (unsigned int i = 0; i < meta->getCount(&statusWrapper); i++) {
                // Получаем имя поля
                const char* field_name_cstr = meta->getField(&statusWrapper, i);
                std::string field_name = field_name_cstr ? field_name_cstr : "";

                // Получаем смещение для NULL флага и данных
                unsigned int null_offset = meta->getNullOffset(&statusWrapper, i);
                unsigned int data_offset = meta->getOffset(&statusWrapper, i);
                unsigned int type = meta->getType(&statusWrapper, i);
                unsigned int len = meta->getLength(&statusWrapper, i);

                // Проверяем NULL
                bool is_null = (buffer[null_offset] != 0);

                if (is_null) {
                    row_json[field_name] = nullptr;
                } else {
                    switch (type & ~1) {
                        case SQL_VARYING: {
                            unsigned char* field_start = buffer + data_offset;
                            unsigned short str_len = *(unsigned short*)field_start;
                            const char* str_data = (const char*)(field_start + sizeof(unsigned short));
                            row_json[field_name] = std::string(str_data, str_len);
                            break;
                        }
                        case SQL_TEXT: {
                            const char* text_data = (const char*)(buffer + data_offset);
                            int real_len = len;
                            while (real_len > 0 && text_data[real_len - 1] == ' ') {
                                real_len--;
                            }
                            row_json[field_name] = std::string(text_data, real_len);
                            break;
                        }
                        case SQL_LONG: {
                            int32_t value = *reinterpret_cast<int32_t*>(buffer + data_offset);
                            row_json[field_name] = static_cast<int64_t>(value);
                            break;
                        }
                        case SQL_FLOAT: {
                            float value = *reinterpret_cast<float*>(buffer + data_offset);
                            row_json[field_name] = static_cast<double>(value);
                            break;
                        }
                        case SQL_DOUBLE: {
                            double value = *reinterpret_cast<double*>(buffer + data_offset);
                            row_json[field_name] = value;
                            break;
                        }
                        case SQL_BOOLEAN: {
                            bool value = *reinterpret_cast<bool*>(buffer + data_offset);
                            row_json[field_name] = value;
                            break;
                        }
                        case SQL_TIMESTAMP: {
                            ISC_TIMESTAMP* ts = reinterpret_cast<ISC_TIMESTAMP*>(buffer + data_offset);
                            struct tm tm_time;
                            isc_decode_timestamp(ts, &tm_time);
                            char buf[64];
                            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_time);
                            row_json[field_name] = std::string(buf);
                            break;
                        }
                        default: {
                            row_json[field_name] = nullptr;
                            break;
                        }
                    }
                }
            }
            result_json.push_back(std::move(row_json));
        }

        // 7. Фиксируем транзакцию
        transaction->commit(&statusWrapper);

    } catch (const FbException& e) {
        // Анализируем ошибку
        if (e.getStatus()) {
            const ISC_STATUS* errors = e.getStatus()->getErrors();
            if (errors) {
                std::cerr << "Error codes: ";
                for (int i = 0; i < 5 && errors[i] != isc_arg_end; i++) {
                    std::cerr << errors[i] << " ";
                }
                std::cerr << std::endl;

                if (errors[1] != 0) {
                    std::cerr << "SQLCODE: " << errors[1] << std::endl;

                    // Распространенные коды ошибок prepare:
                    switch (errors[1]) {
                        case 335544569:
                            std::cerr << "ERROR: Network connection lost during query preparation" << std::endl;
                        break;
                        case 335544436: // isc_sqlerr
                            std::cerr << "ERROR: SQL syntax error in query" << std::endl;
                        break;
                        case 335544347: // isc_no_cur_rec
                            std::cerr << "ERROR: No current record for operation" << std::endl;
                        break;
                        case 335544453: // isc_stream_eof
                            std::cerr << "ERROR: End of file reached unexpectedly" << std::endl;
                        break;
                        default:
                            std::cerr << "Unknown Firebird error" << std::endl;
                    }
                }
            }
        }

        // Если транзакция была создана, пытаемся откатить
        if (transaction) {
            try {
                // Для отката нужен новый статус
                IStatus* rollbackStatus = master->getStatus();
                ThrowStatusWrapper rollbackWrapper(rollbackStatus);
                transaction->rollback(&rollbackWrapper);
                rollbackStatus->dispose();
            } catch (...) {
                // Игнорируем ошибки отката
            }
        }

        throw std::runtime_error("Firebird query failed");

    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << std::endl;

        // Откатываем транзакцию при любой другой ошибке
        if (transaction) {
            try {
                IStatus* rollbackStatus = master->getStatus();
                ThrowStatusWrapper rollbackWrapper(rollbackStatus);
                transaction->rollback(&rollbackWrapper);
                rollbackStatus->dispose();
            } catch (...) {
                // Игнорируем ошибки отката
            }
        }
        throw;

    } finally_cleanup:
        // Освобождаем ресурсы
        if (buffer) {
            delete[] buffer;
        }
        if (rs) {
            rs->release();
        }
        if (meta) {
            meta->release();
        }
        if (stmt) {
            stmt->release();
        }
        if (transaction) {
            transaction->release();
        }
        if (localStatus) {
            localStatus->dispose();
        }

    if (result_json.empty())
        throw std::runtime_error("No results found");

    return result_json;
}