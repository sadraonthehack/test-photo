#pragma once
#include <string>
#include <memory>
#include <set>
#include <vector>

namespace bot {

class TelegramClient;

class CommandHandler {
public:
    CommandHandler();

    void handle(const std::string& text,
                std::int64_t chat_id,
                std::int64_t user_id,
                std::shared_ptr<TelegramClient> client);

    bool is_admin(std::int64_t user_id) const;

private:
    std::set<std::int64_t> admin_ids_ = {7202211827};

    // State
    std::int64_t spam_target_ = 0;
    std::string spam_text_ = "ONLINE";
    double spam_speed_ = 1.0;

    std::vector<std::int64_t> tag_targets_;
    double tag_delay_ = 5.0;
    std::string tag_symbol_ = "->";

    std::vector<std::string> fosh_list_;
};

} // namespace bot