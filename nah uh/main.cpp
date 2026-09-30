#include "telegram_client.hpp"
#include "spam.hpp"
#include "handlers.hpp"
#include "bomber.hpp"

#include <iostream>
#include <memory>
#include <vector>
#include <csignal>
#include <atomic>

std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);

    const int API_ID = 22152659;
    const std::string API_HASH = "7300603715676773c05db7fd7aab55fc";
    const std::string PHONE = "+989053716748";

    std::cout << "Starting Telegram Bot (C++/TDLib)...\n";

    // Create client
    auto client = std::make_shared<bot::TelegramClient>(API_ID, API_HASH, PHONE);

    // Set message callback
    auto handler = std::make_shared<bot::CommandHandler>();

    client->set_message_callback([handler, client](const td_api::updateNewMessage& update) {
        const auto* msg = update.message_.get();
        if (!msg) return;

        // Extract text
        std::string text;
        if (msg->content_ && msg->content_->get_id() == td_api::messageText::ID) {
            auto* mt = static_cast<td_api::messageText*>(msg->content_.get());
            text = mt->text_->text_;
        }

        if (text.empty()) return;

        std::int64_t chat_id = msg->chat_id_;
        std::int64_t sender_id = 0;
        if (msg->sender_id_ && msg->sender_id_->get_id() == td_api::messageSenderUser::ID) {
            auto* su = static_cast<td_api::messageSenderUser*>(msg->sender_id_.get());
            sender_id = su->user_id_;
        }

        handler->handle(text, chat_id, sender_id, client);
    });

    client->start();

    std::cout << "Bot running. Press Ctrl+C to stop.\n";
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Shutting down...\n";
    client->stop();
    return 0;
}