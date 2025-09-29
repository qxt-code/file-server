#include <iostream>
#include <csignal>

#include "server.h"
#include "db/mysql_pool.h"
#include "db/user_file_repository.h"
#include "storage/global_open_table.h"
#include "auth/rsa_key_manager.h"



int main() {
    db::MySQLConfig mysqlConfig = {
        "127.0.0.1",
        "root",
        "123456",
        "netdisk",
        16
    };
    db::MySQLPool::init(mysqlConfig);
    storage::GlobalOpenTable::init("./repository");
    RSAKeyManager::getInstance().generateKeyPair();
    Server server(8000);
    server.start();
    getchar();


    return EXIT_SUCCESS;
}