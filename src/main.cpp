#include <TelegramBackup.h>
#include <CLI/CLI.hpp>

int main(int argc, char *argv[]) {
    CLI::App app{"A utility to back up files to Telegram."};
    argv = app.ensure_utf8(argv);

    int64_t chat_id;
    std::filesystem::path file_path;
    std::filesystem::path data_dir = "tdlib";
    const CLI::App *auth_subcommand = app.add_subcommand("auth", "Log in to Telegram");
    app.add_option("--datadir", data_dir, "TDLib database directory")->required(false);
    app.add_option("chat_id", chat_id, "Chat ID")->required(false);
    app.add_option("file", file_path, "File to upload")->required(false);
    CLI11_PARSE(app, argc, argv);
    bool has_auth = auth_subcommand->parsed();
    bool has_args = (chat_id != 0) && !file_path.empty();

    if (has_auth && has_args) {
        std::cerr << "Cannot provide chat_id and file with auth command" << std::endl;
        return 1;
    }

    if (!has_auth && !has_args) {
        std::cerr << "Must provide both chat_id and file when not using auth" << std::endl;
        return 1;
    }
    if (!has_auth && !std::filesystem::exists(file_path)) {
        std::cout << "File not found: " << file_path << std::endl;
        return 1;
    }

    TelegramBackup telegram_backup{data_dir, has_auth};
    if (!telegram_backup.start()) {
        return 1;
    }
    if (has_auth) {
        return 0;
    }
    if (!telegram_backup.chat_id_exists(chat_id)) {
        std::cerr << "Chat not found: " << chat_id << std::endl;
        return 1;
    }
    telegram_backup.queue_file_upload(file_path, chat_id);
    std::cout << "Sending files..." << std::endl;
    telegram_backup.send_all_files();
    std::cout << "Done." << std::endl;
    return 0;
}
