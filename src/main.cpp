#include <TelegramBackup.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0] << " <path_to_file> <chat_id>" << std::endl;
        return 1;
    }
    std::filesystem::path file_path = argv[1];
    int64_t chat_id{};
    try
    {
        chat_id = std::stol(argv[2]);
    }
    catch (const std::invalid_argument &ia)
    {
        std::cerr << "Invalid argument: " << ia.what() << std::endl;
        return 1;
    }
    catch (const std::out_of_range &oor)
    {
        std::cerr << "Out of range: " << oor.what() << std::endl;
        return 1;
    }
    if (!std::filesystem::exists(file_path))
    {
        std::cout << "File not found: " << file_path << std::endl;
        return 1;
    }

    TelegramBackup telegram_backup;
    telegram_backup.start();
    if (!telegram_backup.chat_id_exists(chat_id))
    {
        std::cout << "Chat not found: " << chat_id << std::endl;
        return 1;
    }
    telegram_backup.queue_file_upload(file_path, chat_id);
    std::cout << "Sending files..." << std::endl;
    telegram_backup.send_all_files();
    std::cout << "All files sent." << std::endl;
    return 0;
}