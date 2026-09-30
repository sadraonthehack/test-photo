#include "spam.hpp"
#include "telegram_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>

namespace bot {

SpamManager::SpamManager() = default;

SpamManager::~SpamManager() {
    stop_spam();
}

void SpamManager::start_spam(
    std::vector<std::shared_ptr<TelegramClient>>& clients,
    std::int64_t target,
    const std::string& text,
    double speed_seconds) {

    if (active_.load()) return;
    active_ = true;

    worker_ = std::thread([this, clients, target, text, speed_seconds]() {
        loop(clients, target, text, speed_seconds);
    });
}

void SpamManager::stop_spam() {
    active_ = false;
    if (worker_.joinable()) worker_.join();
}

void SpamManager::loop(
    std::vector<std::shared_ptr<TelegramClient>> clients,
    std::int64_t target,
    std::string text,
    double speed) {

    std::cout << "[SPAM] Loop started. Target: " << target << "\n";

    while (active_.load()) {
        for (auto& client : clients) {
            if (!active_.load()) break;
            try {
                client->send_message(target, text);
                std::cout << "[SPAM] Sent to " << target << "\n";
            } catch (const std::exception& e) {
                std::cout << "[SPAM] Error: " << e.what() << "\n";
            }
        }
        if (active_.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(speed * 1000))
            );
        }
    }

    std::cout << "[SPAM] Loop ended\n";
}

} // namespace bot