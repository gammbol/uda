#include <fb_connect.h>
#include <firebird/Interface.h>

void FirebirdConnection::fbInit() {
  master = fb_get_master_interface();
  st = master->getStatus();
  utl = master->getUtilInterface();
  dpb = utl->getXpbBuilder(st, IXpbBuilder::DPB, nullptr, 0);
}

FirebirdConnection::FirebirdConnection(FBConnectionStruct conf) : config(conf) {
  fbInit();

  provider = nullptr;
  attachment = nullptr;
}

FirebirdConnection::~FirebirdConnection() {
  disconnect();
}

void FirebirdConnection::newConnection(FBConnectionStruct conf) {
  fbInit();
  config = conf;
}

bool FirebirdConnection::connect() {
  // creating dpb
  dpb->clear(st);
  dpb->insertInt(st, isc_dpb_page_size, 4 * 1024);
  dpb->insertString(st, isc_dpb_user_name, config.db_user.c_str());
  dpb->insertString(st, isc_dpb_password, config.db_password.c_str());

  // connection string
  std::string connection_string = std::format("{}/{}:{}", config.db_host, config.db_port, config.db_path);

  // getting provider interface
  provider = master->getDispatcher();

  // connecting
  attachment = nullptr;
  try {
    attachment = provider->attachDatabase(st,
      connection_string.c_str(),
      dpb->getBufferLength(st),
      dpb->getBuffer(st));

    if (attachment) {
      isConnected = true;
      return true;
    }

    throw std::runtime_error("Failed to attach database");
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
    isConnected = false;
    return false;
  }
}

void FirebirdConnection::disconnect() {
  if (!isConnected) return;

  if (attachment) {
    attachment->release();
    attachment = nullptr;
  }

  if (dpb) {
    dpb->dispose();
    dpb = nullptr;
  }

  if (st) {
    st->dispose();
    st = nullptr;
  }
}

std::vector<crow::json::wvalue> FirebirdConnection::getSQL(std::string query) {
  if (!isConnected) throw std::runtime_error("FirebirdConnection is not connected");

  std::vector<crow::json::wvalue> result_json;
  ITransaction *transaction = nullptr;
  IStatement *stmt = nullptr;
  IMessageMetadata *meta = nullptr;

  try {
    transaction = attachment->startTransaction(st, 0, nullptr); // creating a transaction

    // setting up statement
    stmt = attachment->prepare(st, transaction, 0, query.c_str(), SQL_DIALECT_V6, 0);

    // getting meta data
    meta = stmt->getOutputMetadata(st);

    // calculating row buffer size
    unsigned char* buffer = new unsigned char[meta->getMessageLength(st)];

    IResultSet *rs = stmt->openCursor(st, transaction, nullptr, nullptr, nullptr, 0);
    while (rs->fetchNext(st, buffer) == IStatus::RESULT_OK) {
      crow::json::wvalue row_json;
      unsigned int offset = 0;

      for (unsigned int i = 0; i < meta->getCount(st); i++) {
        // field meta data
        unsigned int type = meta->getType(st, i);
        unsigned int len = meta->getLength(st, i);
        bool is_null = static_cast<FB_BOOLEAN>(buffer[offset]);
        offset++;

        // getting field name
        std::string field_name = meta->getField(st, i);

        // cast to json
        if (is_null) {
          row_json[field_name] = nullptr;
          offset += len;
        } else {
          switch (type & ~1) { // ignoring nullable flag
            case SQL_VARYING: {
              unsigned char *field_start = buffer + meta->getOffset(st, i);
              unsigned short str_len = *(unsigned short*)field_start;
              const char *str_data = (const char*)(field_start + sizeof(unsigned short));

              row_json[field_name] = std::string(str_data, str_len);
              break;
            }
            case SQL_LONG: {
              int32_t value = *reinterpret_cast<int32_t*>(buffer + meta->getOffset(st, i));
              row_json[field_name] = static_cast<int64_t>(value);
              break;
            }
            case SQL_FLOAT: {
              float value = *reinterpret_cast<float*>(buffer + meta->getOffset(st, i));
              row_json[field_name] = static_cast<double>(value);
            }
            case SQL_DOUBLE: {
              double value = *reinterpret_cast<double*>(buffer + meta->getOffset(st, i));
              row_json[field_name] = value;
            }
            case SQL_BOOLEAN: {
              bool value = *reinterpret_cast<bool*>(buffer + meta->getOffset(st, i));
              row_json[field_name] = value;
              break;
            }
            default: {
              row_json[field_name] = nullptr;
              break;
            }
          }
        }
      }

      result_json.push_back(row_json);
    }
    transaction->commit(st);
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;

    if (transaction) transaction->rollback(st);
  }

  if (meta) meta->release();
  if (stmt) stmt->release();
  if (transaction) transaction->release();

  return result_json;

}


