#include "telegram_client.hpp"
#include <iostream>
#include <chrono>

namespace bot {

TelegramClient::TelegramClient(int api_id, const std::string& api_hash, const std::string& phone)
    : client_(td::Client::create()),
      api_id_(api_id),
      api_hash_(api_hash),
      phone_(phone) {}

TelegramClient::~TelegramClient() {
    stop();
}

void TelegramClient::start() {
    running_ = true;
    loop_thread_ = std::thread([this]() {
        while (running_) {
            auto response = client_->receive(1.0);
            if (!response.object) continue;
            process_response(std::move(response));
        }
    });
}

void TelegramClient::stop() {
    running_ = false;
    if (loop_thread_.joinable()) loop_thread_.join();
}

void TelegramClient::send_query(
    td_api::object_ptr<td_api::Function> f,
    std::function<void(td_api::object_ptr<td_api::Object>)> handler) {
    auto id = ++query_id_;
    if (handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_[id] = std::move(handler);
    }
    client_->send({id, std::move(f)});
}

void TelegramClient::process_response(td::Client::Response response) {
    if (!td::Client::is_response(response)) {
        auto update = td::move_tl_object_as<td_api::Update>(response.object);
        handle_update(std::move(update));
        return;
    }

    std::function<void(td_api::object_ptr<td_api::Object>)> handler;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        auto it = handlers_.find(response.id);
        if (it != handlers_.end()) {
            handler = std::move(it->second);
            handlers_.erase(it);
        }
    }
    if (handler) handler(std::move(response.object));
}

void TelegramClient::handle_update(td_api::object_ptr<td_api::Update> update) {
    if (update->get_id() == td_api::updateAuthorizationState::ID) {
        auto* auth = static_cast<td_api::updateAuthorizationState*>(update.get());
        handle_authorization_state(std::move(auth->authorization_state_));
    } else if (update->get_id() == td_api::updateNewMessage::ID) {
        auto* msg = static_cast<td_api::updateNewMessage*>(update.get());
        if (message_callback_) {
            message_callback_(*msg);
        }
    }
}

void TelegramClient::handle_authorization_state(td_api::object_ptr<td_api::AuthorizationState> state) {
    switch (state->get_id()) {
        case td_api::authorizationStateWaitTdlibParameters::ID:
            send_query(td_api::make_object<td_api::setTdlibParameters>(
                false,
                "td_db",
                "td_files",
                true, true, true, false,
                api_id_,
                api_hash_,
                "en",
                "Linux",
                "1.0",
                "1.0"
            ));
            break;

        case td_api::authorizationStateWaitPhoneNumber::ID:
            send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(
                phone_, nullptr
            ));
            break;

        case td_api::authorizationStateWaitCode::ID:
            std::cout << "[AUTH] Enter code: ";
            std::string code;
            std::cin >> code;
            send_query(td_api::make_object<td_api::checkAuthenticationCode>(code));
            break;

        case td_api::authorizationStateWaitPassword::ID:
            std::cout << "[AUTH] Enter 2FA password: ";
            std::string pwd;
            std::cin >> pwd;
            send_query(td_api::make_object<td_api::checkAuthenticationPassword>(pwd));
            break;

        case td_api::authorizationStateReady::ID: {
            std::cout << "[AUTH] Ready!\n";
            // Get self
            send_query(td_api::make_object<td_api::getMe>(), [this](auto obj) {
                if (obj && obj->get_id() == td_api::user::ID) {
                    auto* user = static_cast<td_api::user*>(obj.get());
                    self_id_ = user->id_;
                    std::cout << "[AUTH] Logged in as ID: " << self_id_ << "\n";
                }
            });
            break;
        }

        case td_api::authorizationStateClosed::ID:
            std::cout << "[AUTH] Closed\n";
            break;

        default:
            break;
    }
}

void TelegramClient::send_message(std::int64_t chat_id, const std::string& text) {
    send_query(td_api::make_object<td_api::sendMessage>(
        chat_id,
        nullptr,  // message_thread_id
        nullptr,  // reply_to
        nullptr,  // options
        nullptr,  // reply_markup
        td_api::make_object<td_api::inputMessageText>(
            td_api::make_object<td_api::formattedText>(text, std::vector<td_api::object_ptr<td_api::textEntity>>()),
            true, false
        )
    ));
}

void TelegramClient::join_chat(std::int64_t chat_id) {
    send_query(td_api::make_object<td_api::joinChat>(chat_id));
}

} // namespace bot