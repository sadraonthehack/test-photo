#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <memory>

namespace bot {

class TelegramClient;

class SpamManager {
public:
    SpamManager();
    ~SpamManager();

    void start_spam(std::vector<std::shared_ptr<TelegramClient>>& clients,
                    std::int64_t target,
                    const std::string& text,
                    double speed_seconds);

    void stop_spam();
    bool is_active() const { return active_.load(); }

private:
    void loop(std::vector<std::shared_ptr<TelegramClient>> clients,
              std::int64_t target,
              std::string text,
              double speed);

    std::atomic<bool> active_{false};
    std::thread worker_;
};

} // namespace bot