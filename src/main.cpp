#include <iostream>
#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <overload.h>

class Client
{
public:
    Client()
    {
        client_id = client_manager.create_client_id();
        send_request(td::td_api::make_object<td::td_api::getOption>("version"));
    }
    void loop()
    {
        while (true)
        {
            td::ClientManager::Response response = client_manager.receive(10);
            if (!response.object)
                continue; // timeout
            td::td_api::downcast_call(
                *response.object,
                overloaded(
                    [&](td::td_api::updateAuthorizationState &authUpdate)
                    {
                        handle_auth_update(authUpdate);
                    },
                    [&](auto &update) {}));
        }
    }

private:
    td::ClientManager client_manager;
    td::ClientManager::ClientId client_id;
    td::ClientManager::RequestId request_id{1};
    void send_request(td::td_api::object_ptr<td::td_api::Function> request)
    {
        client_manager.send(client_id, request_id, std::move(request));
    }
    void handle_auth_update(td::td_api::updateAuthorizationState &authUpdate)
    {
        td::td_api::downcast_call(
            *authUpdate.authorization_state_,
            overloaded(
                [this](td::td_api::authorizationStateReady &)
                {
                    std::cout << "Authorization is completed!" << std::endl;
                },
                [this](td::td_api::authorizationStateLoggingOut &)
                {
                    std::cout << "Logging out" << std::endl;
                },
                [this](td::td_api::authorizationStateClosing &)
                {
                    std::cout << "Closing" << std::endl;
                },
                [this](td::td_api::authorizationStateClosed &)
                {
                    std::cout << "Terminated" << std::endl;
                },
                [this](td::td_api::authorizationStateWaitPhoneNumber &)
                {
                    std::cout << "Enter phone number: " << std::flush;
                    std::string phone_number;
                    std::cin >> phone_number;
                    send_request(td::td_api::make_object<td::td_api::setAuthenticationPhoneNumber>(phone_number, nullptr));
                },
                [this](td::td_api::authorizationStateWaitPremiumPurchase &)
                {
                    std::cout << "Telegram Premium subscription is required." << std::endl;
                },
                [this](td::td_api::authorizationStateWaitEmailAddress &)
                {
                    std::cout << "Enter email address: " << std::flush;
                    std::string email_address;
                    std::cin >> email_address;
                    send_request(td::td_api::make_object<td::td_api::setAuthenticationEmailAddress>(email_address));
                },
                [this](td::td_api::authorizationStateWaitEmailCode &)
                {
                    std::cout << "Enter email authentication code: " << std::flush;
                    std::string code;
                    std::cin >> code;
                    send_request(td::td_api::make_object<td::td_api::checkAuthenticationEmailCode>(td::td_api::make_object<td::td_api::emailAddressAuthenticationCode>(code)));
                },
                [this](td::td_api::authorizationStateWaitCode &)
                {
                    std::cout << "Enter authentication code: " << std::flush;
                    std::string code;
                    std::cin >> code;
                    send_request(td::td_api::make_object<td::td_api::checkAuthenticationCode>(code));
                },
                [this](td::td_api::authorizationStateWaitRegistration &)
                {
                    std::string first_name;
                    std::string last_name;
                    std::cout << "Enter your first name: " << std::flush;
                    std::cin >> first_name;
                    std::cout << "Enter your last name: " << std::flush;
                    std::cin >> last_name;
                    send_request(td::td_api::make_object<td::td_api::registerUser>(first_name, last_name, false));
                },
                [this](td::td_api::authorizationStateWaitPassword &)
                {
                    std::cout << "Enter authentication password: " << std::flush;
                    std::string password;
                    std::getline(std::cin, password);
                    send_request(td::td_api::make_object<td::td_api::checkAuthenticationPassword>(password));
                },
                [this](td::td_api::authorizationStateWaitOtherDeviceConfirmation &state)
                {
                    std::cout << "Confirm this login link on another device: " << state.link_ << std::endl;
                },
                [this](td::td_api::authorizationStateWaitTdlibParameters &)
                {
                    auto request = td::td_api::make_object<td::td_api::setTdlibParameters>();
                    request->database_directory_ = "td_db";
                    request->api_id_ = 94575;
                    request->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
                    request->system_language_code_ = "en";
                    request->device_model_ = "Desktop";
                    request->application_version_ = "1.0";
                    send_request(std::move(request));
                }));
    }
};

int main()
{
    td::ClientManager::execute(td::td_api::make_object<td::td_api::setLogVerbosityLevel>(1));
    Client client;
    client.loop();
}