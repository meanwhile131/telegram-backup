#include <TelegramBackup.h>
#include <CLI/CLI.hpp>

int main(int argc, char *argv[]) {
    CLI::App app{"A utility to back up files to Telegram."};
    argv = app.ensure_utf8(argv);

    int64_t chat_id;
    std::filesystem::path file_path;
    app.add_option("chat_id", chat_id, "Chat ID")->required();
    app.add_option("file", file_path, "File to upload")->required();
    CLI11_PARSE(app, argc, argv);
    if (!std::filesystem::exists(file_path)) {
        std::cout << "File not found: " << file_path << std::endl;
        return 1;
    }

    TelegramBackup telegram_backup;
    telegram_backup.start();
    if (!telegram_backup.chat_id_exists(chat_id)) {
        std::cout << "Chat not found: " << chat_id << std::endl;
        return 1;
    }
    telegram_backup.queue_file_upload(file_path, chat_id);
    std::cout << "Sending files..." << std::endl;
    telegram_backup.send_all_files();
    std::cout << "Done." << std::endl;
    return 0;
}
