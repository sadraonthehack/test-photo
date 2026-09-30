#include "handlers.hpp"
#include "telegram_client.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>

namespace bot {

CommandHandler::CommandHandler() {
    std::ifstream f("fosh.txt");
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) fosh_list_.push_back(line);
    }
    if (fosh_list_.empty()) {
        fosh_list_ = {"بیا پایین", "کصخل", "برو گمشو"};
    }
}

bool CommandHandler::is_admin(std::int64_t user_id) const {
    return admin_ids_.count(user_id) > 0;
}

void CommandHandler::handle(
    const std::string& text,
    std::int64_t chat_id,
    std::int64_t user_id,
    std::shared_ptr<TelegramClient> client) {

    if (!is_admin(user_id)) {
        std::cout << "[BOT] Ignored non-admin: " << user_id << "\n";
        return;
    }

    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "help") {
        client->send_message(chat_id, "Help: spam, spamoff, setid, setfosh, speed, ...");
    } else if (lower == "spam") {
        if (spam_target_ == 0) {
            client->send_message(chat_id, "No target set");
            return;
        }
        client->send_message(chat_id, "Spam started");
        // TODO: hook into SpamManager
    } else if (lower == "spamoff") {
        client->send_message(chat_id, "Spam stopped");
    } else if (lower.rfind("setid ", 0) == 0) {
        spam_target_ = std::stoll(text.substr(6));
        client->send_message(chat_id, "Target set: " + std::to_string(spam_target_));
    } else if (lower.rfind("setfosh ", 0) == 0) {
        spam_text_ = text.substr(8);
        client->send_message(chat_id, "Text set");
    } else if (lower == "status") {
        std::ostringstream oss;
        oss << "Target: " << spam_target_ << "\n"
            << "Text: " << spam_text_ << "\n"
            << "Speed: " << spam_speed_ << "\n"
            << "Admins: " << admin_ids_.size();
        client->send_message(chat_id, oss.str());
    }
    // ... ۵۰+ دستور دیگه که باید دستی اضافه کنی
}

} // namespace bot