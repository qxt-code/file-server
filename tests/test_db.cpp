#include <gtest/gtest.h>
#include "db/mysql_pool.h"
#include "db/user_repository.h"
#include "db/file_repository.h"

class DatabaseTest : public ::testing::Test {
protected:
    MySQLPool* pool;
    UserRepository* userRepo;
    FileRepository* fileRepo;

    void SetUp() override {
        pool = new MySQLPool();
        pool->initialize("localhost", "user", "password", "database");
        userRepo = new UserRepository(pool);
        fileRepo = new FileRepository(pool);
    }

    void TearDown() override {
        delete userRepo;
        delete fileRepo;
        delete pool;
    }
};

TEST_F(DatabaseTest, UserCreation) {
    User user;
    user.username = "test_user";
    user.password_hash = "hashed_password";

    ASSERT_TRUE(userRepo->createUser(user));
}

TEST_F(DatabaseTest, FileCreation) {
    FileMetadata file;
    file.filename = "test_file.txt";
    file.user_id = 1;

    ASSERT_TRUE(fileRepo->createFile(file));
}

TEST_F(DatabaseTest, UserRetrieval) {
    User user = userRepo->getUserByUsername("test_user");
    ASSERT_EQ(user.username, "test_user");
}

TEST_F(DatabaseTest, FileRetrieval) {
    FileMetadata file = fileRepo->getFileById(1);
    ASSERT_EQ(file.filename, "test_file.txt");
}