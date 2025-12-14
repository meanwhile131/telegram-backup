
#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>
#include <iostream>
#include <filesystem>
#include <map>
#include <functional>
#include <set>

// overloaded
namespace detail {
    template<class... Fs>
    struct overload;

    template<class F>
    struct overload<F> : public F {
        explicit overload(F f) : F(f) {
        }
    };

    template<class F, class... Fs>
    struct overload<F, Fs...>
            : public overload<F>, public overload<Fs...> {
        overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
        }

        using overload<F>::operator();
        using overload<Fs...>::operator();
    };
} // namespace detail

template<class... F>
auto overloaded(F... f) {
    return detail::overload<F...>(f...);
}

namespace td_api = td::td_api;

class TelegramBackup {
public:
    TelegramBackup(bool auth_only = false);

    bool start();

    void queue_file_upload(const std::filesystem::path &path, int64_t chat_id);

    void send_all_files();

    bool chat_id_exists(int64_t chat_id);

private:
    using Object = td_api::object_ptr<td_api::Object>;
    std::unique_ptr<td::ClientManager> client_manager_;
    std::int32_t client_id_{0};

    td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
    bool are_authorized_{false};
    bool exiting{false};
    bool chats_loaded{false};
    std::set<int64_t> messages_sending;
    int64_t messages_queuing{0};
    bool auth_only;
    bool auth_needed{false};
    std::uint64_t current_query_id_{0};
    std::uint64_t authentication_query_id_{0};

    std::map<std::uint64_t, std::function<void(Object)> > handlers_;

    void load_chats();

    void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler);

    void process_response(td::ClientManager::Response response);

    void process_update(td_api::object_ptr<td_api::Object> update);

    auto create_authentication_query_handler();

    void on_authorization_state_update();

    void check_authentication_error(Object object);

    std::uint64_t next_query_id();
};
