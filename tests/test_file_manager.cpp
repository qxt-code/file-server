#include <gtest/gtest.h>
#include "file_manager.h"
#include "global_open_table.h"
#include "user_open_table.h"

class FileManagerTest : public ::testing::Test {
protected:
    FileManager fileManager;
    GlobalOpenTable globalOpenTable;
    UserOpenTable userOpenTable;

    void SetUp() override {
        // Setup code for initializing file manager and tables
    }

    void TearDown() override {
        // Cleanup code after each test
    }
};

TEST_F(FileManagerTest, TestFileOpen) {
    // Test opening a file
    const std::string filename = "test_file.txt";
    EXPECT_TRUE(fileManager.openFile(filename));
    EXPECT_TRUE(globalOpenTable.isFileOpen(filename));
}

TEST_F(FileManagerTest, TestFileClose) {
    // Test closing a file
    const std::string filename = "test_file.txt";
    fileManager.openFile(filename);
    EXPECT_TRUE(fileManager.closeFile(filename));
    EXPECT_FALSE(globalOpenTable.isFileOpen(filename));
}

TEST_F(FileManagerTest, TestUserFileOpen) {
    // Test user-specific file opening
    const std::string username = "test_user";
    const std::string filename = "user_file.txt";
    EXPECT_TRUE(userOpenTable.openFileForUser(username, filename));
    EXPECT_TRUE(userOpenTable.isFileOpenForUser(username, filename));
}

TEST_F(FileManagerTest, TestUserFileClose) {
    // Test user-specific file closing
    const std::string username = "test_user";
    const std::string filename = "user_file.txt";
    userOpenTable.openFileForUser(username, filename);
    EXPECT_TRUE(userOpenTable.closeFileForUser(username, filename));
    EXPECT_FALSE(userOpenTable.isFileOpenForUser(username, filename));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}