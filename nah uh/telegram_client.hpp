#pragma once

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>

namespace bot {

class TelegramClient {
public:
    using MessageCallback = std::function<void(const td_api::updateNewMessage&)>;

    TelegramClient(int api_id, const std::string& api_hash, const std::string& phone);
    ~TelegramClient();

    void start();
    void stop();

    void set_message_callback(MessageCallback cb) { message_callback_ = std::move(cb); }

    // High-level helpers
    void send_message(std::int64_t chat_id, const std::string& text);
    void join_chat(std::int64_t chat_id);
    std::int64_t get_self_id() const { return self_id_; }

private:
    void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(td_api::object_ptr<td_api::Object>)> handler = nullptr);
    void process_response(td::Client::Response response);
    void handle_update(td_api::object_ptr<td_api::Update> update);
    void handle_authorization_state(td_api::object_ptr<td_api::AuthorizationState> state);

    std::shared_ptr<td::Client> client_;
    std::atomic<std::uint64_t> query_id_{0};
    std::atomic<bool> running_{false};
    std::thread loop_thread_;

    int api_id_;
    std::string api_hash_;
    std::string phone_;
    std::int64_t self_id_ = 0;

    MessageCallback message_callback_;

    std::mutex handlers_mutex_;
    std::unordered_map<std::uint64_t, std::function<void(td_api::object_ptr<td_api::Object>)>> handlers_;
};

} // namespace bot