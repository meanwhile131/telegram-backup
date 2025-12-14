#include <TelegramBackup.h>

TelegramBackup::TelegramBackup(const bool auth_only) : auth_only(auth_only) {
    td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
    client_manager_ = std::make_unique<td::ClientManager>();
    client_id_ = client_manager_->create_client_id();
    send_query(td_api::make_object<td_api::getOption>("version"), {});
}

bool TelegramBackup::start() {
    std::cout << "Authorizing..." << std::endl;
    while (true) {
        if (need_restart_) {
            restart();
        } else if (!are_authorized_ || !chats_loaded) {
            if (are_authorized_ && auth_only) {
                return true;
            }
            process_response(client_manager_->receive(10));
            if (auth_needed) { return false; }
        } else {
            return true;
        }
    }
}

void TelegramBackup::queue_file_upload(const std::filesystem::path &path, int64_t chat_id) {
    auto file_path = path.string();
    auto message_content = td_api::make_object<td_api::inputMessageDocument>();
    message_content->document_ = td_api::make_object<td_api::inputFileLocal>(file_path);
    messages_queuing += 1;
    send_query(td_api::make_object<td_api::sendMessage>(chat_id, nullptr, nullptr, nullptr, nullptr,
                                                        std::move(message_content)),
               [this, file_path](Object object) {
                   td_api::downcast_call(
                       *object, overloaded(
                           [&](td_api::message &message) {
                               std::cout << "Queued " << file_path << " (message #" << message.id_ << ")" << std::endl;
                               messages_sending.insert(message.id_);
                           },
                           [&](auto &error) {
                               std::cout << "Failed to queue " << file_path << ": " << to_string(error) << std::endl;
                           }));
                   messages_queuing -= 1;
               });
}

void TelegramBackup::send_all_files() {
    while (messages_queuing != 0 || !messages_sending.empty()) {
        td::ClientManager::Response response = client_manager_->receive(10);
        if (response.object) {
            process_response(std::move(response));
        }
    }
}

bool TelegramBackup::chat_id_exists(int64_t chat_id) {
    bool chat_exists{};
    bool chat_checked{false};
    send_query(td_api::make_object<td_api::getChat>(chat_id),
               [&chat_exists, &chat_checked](Object object) {
                   chat_exists = object->get_id() == td_api::chat::ID;
                   chat_checked = true;
               });

    while (!chat_checked) {
        td::ClientManager::Response response = client_manager_->receive(10);
        if (response.object) {
            process_response(std::move(response));
        }
    }
    return chat_exists;
}

void TelegramBackup::load_chats() {
    send_query(td_api::make_object<td_api::loadChats>(nullptr, 1024), [&](Object object) {
        if (object->get_id() == td_api::error::ID) {
            chats_loaded = true;
            std::cout << "Done loading chats." << std::endl;
            return;
        }
        load_chats();
    });
}

void TelegramBackup::restart() {
    client_manager_.reset();
    *this = TelegramBackup();
}

void TelegramBackup::send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
    auto query_id = next_query_id();
    if (handler) {
        handlers_.emplace(query_id, std::move(handler));
    }
    client_manager_->send(client_id_, query_id, std::move(f));
}

void TelegramBackup::process_response(td::ClientManager::Response response) {
    if (!response.object) {
        return;
    }
    if (response.request_id == 0) {
        return process_update(std::move(response.object));
    }
    auto it = handlers_.find(response.request_id);
    if (it != handlers_.end()) {
        it->second(std::move(response.object));
        handlers_.erase(it);
    }
}

void TelegramBackup::process_update(td_api::object_ptr<td_api::Object> update) {
    td_api::downcast_call(
        *update, overloaded(
            [this](td_api::updateAuthorizationState &update_authorization_state) {
                authorization_state_ = std::move(update_authorization_state.authorization_state_);
                on_authorization_state_update();
            },
            [this](td_api::updateMessageSendSucceeded &update) {
                int64_t old_id = update.old_message_id_;
                if (messages_sending.contains(old_id)) {
                    messages_sending.erase(old_id);
                    std::cout << "Message #" << old_id << " sent." << std::endl;
                }
            },
            [](auto &update) {
            }));
}

auto TelegramBackup::create_authentication_query_handler() {
    return [this, id = authentication_query_id_](Object object) {
        if (id == authentication_query_id_) {
            check_authentication_error(std::move(object));
        }
    };
}

void TelegramBackup::on_authorization_state_update() {
    authentication_query_id_++;
    td_api::downcast_call(*authorization_state_,
                          overloaded(
                              [this](td_api::authorizationStateReady &) {
                                  are_authorized_ = true;
                                  std::cout << "Authorization is completed." << std::endl;
                                  load_chats();
                              },
                              [this](td_api::authorizationStateLoggingOut &) {
                                  are_authorized_ = false;
                                  std::cout << "Logging out" << std::endl;
                              },
                              [](td_api::authorizationStateClosing &) { std::cout << "Closing" << std::endl; },
                              [this](td_api::authorizationStateClosed &) {
                                  are_authorized_ = false;
                                  need_restart_ = true;
                                  std::cout << "Terminated" << std::endl;
                              },
                              [this](td_api::authorizationStateWaitPhoneNumber &) {
                                  if (this->auth_only) {
                                      std::cout << "Enter phone number: " << std::flush;
                                      std::string phone_number;
                                      std::cin >> phone_number;
                                      send_query(
                                          td_api::make_object<td_api::setAuthenticationPhoneNumber>(
                                              phone_number, nullptr),
                                          create_authentication_query_handler());
                                  } else {
                                      auth_needed = true;
                                  }
                              },
                              [](td_api::authorizationStateWaitPremiumPurchase &) {
                                  std::cout << "Telegram Premium subscription is required" << std::endl;
                              },
                              [this](td_api::authorizationStateWaitEmailAddress &) {
                                  if (this->auth_only) {
                                      std::cout << "Enter email address: " << std::flush;
                                      std::string email_address;
                                      std::cin >> email_address;
                                      send_query(
                                          td_api::make_object<td_api::setAuthenticationEmailAddress>(email_address),
                                          create_authentication_query_handler());
                                  } else {
                                      auth_needed = true;
                                  }
                              },
                              [this](td_api::authorizationStateWaitEmailCode &) {
                                  if (this->auth_only) {
                                      std::cout << "Enter email authentication code: " << std::flush;
                                      std::string code;
                                      std::cin >> code;
                                      send_query(td_api::make_object<td_api::checkAuthenticationEmailCode>(
                                                     td_api::make_object<td_api::emailAddressAuthenticationCode>(code)),
                                                 create_authentication_query_handler());
                                  } else {
                                      auth_needed = true;
                                  }
                              },
                              [this](td_api::authorizationStateWaitCode &) {
                                  if (this->auth_only) {
                                      std::cout << "Enter authentication code: " << std::flush;
                                      std::string code;
                                      std::cin >> code;
                                      send_query(td_api::make_object<td_api::checkAuthenticationCode>(code),
                                                 create_authentication_query_handler());
                                  } else {
                                      auth_needed = true;
                                  }
                              },
                              [this](td_api::authorizationStateWaitRegistration &) {
                                  if (this->auth_only) {
                                      std::string first_name;
                                      std::string last_name;
                                      std::cout << "Enter your first name: " << std::flush;
                                      std::cin >> first_name;
                                      std::cout << "Enter your last name: " << std::flush;
                                      std::cin >> last_name;
                                      send_query(
                                          td_api::make_object<td_api::registerUser>(first_name, last_name, false),
                                          create_authentication_query_handler());
                                  } else {
                                      auth_needed = true;
                                  }
                              },
                              [this](td_api::authorizationStateWaitPassword &) {
                                  if (this->auth_only) {
                                      std::cout << "Enter authentication password: " << std::flush;
                                      std::string password;
                                      std::getline(std::cin, password);
                                      send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                                                 create_authentication_query_handler());
                                  } else {
                                      auth_needed = true;
                                  }
                              },
                              [this](const td_api::authorizationStateWaitOtherDeviceConfirmation &state) {
                                  if (this->auth_only) {
                                      std::cout << "Confirm this login link on another device: " << state.link_ <<
                                              std::endl;
                                  } else {
                                      auth_needed = true;
                                  }
                              },
                              [this](td_api::authorizationStateWaitTdlibParameters &) {
                                  auto request = td_api::make_object<td_api::setTdlibParameters>();
                                  request->database_directory_ = "tdlib";
                                  request->use_chat_info_database_ = true;
                                  request->use_secret_chats_ = true;
                                  request->api_id_ = 94575;
                                  request->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
                                  request->system_language_code_ = "en";
                                  request->device_model_ = "Desktop";
                                  request->application_version_ = "1.0";
                                  send_query(std::move(request), create_authentication_query_handler());
                              }));
}

void TelegramBackup::check_authentication_error(Object object) {
    if (object->get_id() == td_api::error::ID) {
        auto error = td::move_tl_object_as<td_api::error>(object);
        std::cout << "Error: " << to_string(error) << std::flush;
        on_authorization_state_update();
    }
}

std::uint64_t TelegramBackup::next_query_id() {
    return ++current_query_id_;
}
