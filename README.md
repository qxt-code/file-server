# C++ 文件服务器

## 主要功能

- 用户注册与登录 (基于 JWT 和 RSA 加密)
- 文件秒传、大文件分块上传、断点续传
- 目录结构管理 (创建目录, 切换目录)
- 文件列表查看

## 环境依赖

- C++ 编译器 (支持 C++17)
- CMake (3.10+)
- MySQL
- OpenSSL

## 编译与运行

本项目使用 CMake 进行构建。

1.  **克隆仓库**
    ```bash
    git clone https://github.com/qxt-code/file-server.git
    cd file-server
    ```

2.  **编译**
    ```bash
    make cm
    make
    ```
    编译成功后，`build` 目录下会生成两个可执行文件：`file_server_server` (服务端) 和 `file_server_client` (客户端)。

3.  **运行**
    *   **启动服务端**
        在 根 目录下执行：
        ```bash
        make s
        ```
        默认监听 `8000` 端口。

    *   **启动客户端**
        在 根 目录下执行：
        ```bash
        make c
        ```
        例如：

## 客户端主要命令

客户端支持以下命令来与服务器进行交互：
register、login：先提交命令，跟随提示输入用户名密码
其他：命令+参数同时提交

| 命令 | 参数 | 描述 |
| :--- | :--- | :--- |
| `register` | `<username> <password>` | 注册一个新用户 |
| `login` | `<username> <password>` | 登录到服务器 |
| `ls` |  | 列出当前远程目录下的文件和文件夹 |
| `cd` | `<dir_path>` | 切换远程目录 |
| `mkdir` | `<dir_name>` | 在当前远程目录下创建新目录 |
| `put` | `<local_file_path>` | 上传文件到当前远程目录 |
| `get` | `<remote_file_name>` | 从当前远程目录下载文件 |
| `quit` | | 退出客户端 |

---
