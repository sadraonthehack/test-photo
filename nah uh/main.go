package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/zelenin/go-tdlib/client"
)

// ============================================================
// CONFIG
// ============================================================
const (
	API_ID   = 22152659
	API_HASH = "7300603715676773c05db7fd7aab55fc"
	PHONE    = "+989053716748"
)

var ADMIN_IDS = map[int64]bool{
	7202211827: true,
}

// ============================================================
// STATE
// ============================================================
var (
	spamTarget   int64 = 0
	spamText           = "ONLINE"
	spamSpeed          = 1.0
	spamActive         = false
	spamMutex    sync.Mutex

	foshList []string

	tdClient *client.Client
)

// ============================================================
// HELPERS
// ============================================================
func isAdmin(uid int64) bool {
	return ADMIN_IDS[uid]
}

func toLower(s string) string {
	return strings.ToLower(s)
}

func sleepSeconds(sec float64) {
	time.Sleep(time.Duration(sec * float64(time.Second)))
}

// ============================================================
// FOSH FILE
// ============================================================
func loadFosh() {
	f, err := os.Open("fosh.txt")
	if err != nil {
		foshList = []string{"بیا پایین", "کصخل", "برو گمشو"}
		return
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line != "" {
			foshList = append(foshList, line)
		}
	}
	if len(foshList) == 0 {
		foshList = []string{"بیا پایین", "کصخل", "برو گمشو"}
	}
}

// ============================================================
// SEND MESSAGE
// ============================================================
func sendMessage(chatID int64, text string) error {
	_, err := tdClient.SendMessage(&client.SendMessageRequest{
		ChatId: chatID,
		InputMessageContent: &client.InputMessageText{
			Text: &client.FormattedText{
				Text: text,
			},
		},
	})
	return err
}

// ============================================================
// SPAM LOOP
// ============================================================
func spamLoop() {
	fmt.Println("[SPAM] Loop started")
	for {
		spamMutex.Lock()
		active := spamActive
		target := spamTarget
		text := spamText
		speed := spamSpeed
		spamMutex.Unlock()

		if !active {
			break
		}

		if err := sendMessage(target, text); err != nil {
			fmt.Printf("[SPAM] Error: %v\n", err)
			// FloodWait handling
			if strings.Contains(err.Error(), "FLOOD_WAIT") {
				re := regexp.MustCompile(`FLOOD_WAIT_(\d+)`)
				m := re.FindStringSubmatch(err.Error())
				if len(m) > 1 {
					secs, _ := strconv.Atoi(m[1])
					fmt.Printf("[SPAM] Flood wait: %ds\n", secs)
					time.Sleep(time.Duration(secs) * time.Second)
					continue
				}
			}
		} else {
			fmt.Printf("[SPAM] Sent to %d\n", target)
		}

		sleepSeconds(speed)
	}
	fmt.Println("[SPAM] Loop ended")
}

// ============================================================
// BOMB (placeholder - میتونی با net/http کاملش کنی)
// ============================================================
func runBomb(phone string) string {
	// TODO: پیاده‌سازی با net/http
	return "Bomb feature: use HTTP requests in Go with net/http"
}

// ============================================================
// COMMAND HANDLER
// ============================================================
func handleCommand(text string, chatID int64, userID int64) {
	if !isAdmin(userID) {
		fmt.Printf("[BOT] Ignored non-admin: %d\n", userID)
		return
	}

	lower := toLower(text)

	// HELP
	if lower == "help" {
		msg := "COMMANDS:\n" +
			"spam / spamoff\n" +
			"setid <id> / setfosh <text>\n" +
			"speed <sec> / speed lowend [n]\n" +
			"status / bot\n" +
			"bomb <phone>"
		sendMessage(chatID, msg)
		return
	}

	// BOT
	if lower == "bot" {
		sendMessage(chatID, "ONLINE")
		return
	}

	// STATUS
	if lower == "status" {
		spamMutex.Lock()
		msg := fmt.Sprintf(
			"TARGET: %d\nTEXT: %s\nSPEED: %f\nSPAM: %v",
			spamTarget, spamText, spamSpeed, spamActive,
		)
		spamMutex.Unlock()
		sendMessage(chatID, msg)
		return
	}

	// SPAM
	if lower == "spam" {
		spamMutex.Lock()
		if spamTarget == 0 {
			spamMutex.Unlock()
			sendMessage(chatID, "No target. Use setid <id>")
			return
		}
		if spamActive {
			spamMutex.Unlock()
			sendMessage(chatID, "Already running")
			return
		}
		spamActive = true
		spamMutex.Unlock()
		go spamLoop()
		sendMessage(chatID, "Spam started")
		return
	}

	// SPAMOFF
	if lower == "spamoff" {
		spamMutex.Lock()
		spamActive = false
		spamMutex.Unlock()
		sendMessage(chatID, "Spam stopped")
		return
	}

	// SETID
	if strings.HasPrefix(lower, "setid ") {
		idStr := strings.TrimSpace(text[6:])
		id, err := strconv.ParseInt(idStr, 10, 64)
		if err != nil {
			sendMessage(chatID, "Invalid ID")
			return
		}
		spamMutex.Lock()
		spamTarget = id
		spamMutex.Unlock()
		sendMessage(chatID, fmt.Sprintf("Target: %d", id))
		return
	}

	// SETFOSH
	if strings.HasPrefix(lower, "setfosh ") {
		spamMutex.Lock()
		spamText = text[8:]
		spamMutex.Unlock()
		sendMessage(chatID, "Text set")
		return
	}

	// SPEED
	if strings.HasPrefix(lower, "speed") {
		arg := strings.TrimSpace(text[5:])
		argLower := toLower(arg)

		// speed lowend
		if argLower == "lowend" {
			spamMutex.Lock()
			spamSpeed = 0.6
			spamMutex.Unlock()
			sendMessage(chatID, "Speed: 0.6")
			return
		}

		// speed lowend <n>
		if strings.HasPrefix(argLower, "lowend ") {
			nStr := strings.TrimSpace(arg[7:])
			n, err := strconv.Atoi(nStr)
			if err != nil || n < 0 || n > 300 {
				sendMessage(chatID, "Invalid number (0-300)")
				return
			}
			speed := 0.6
			for i := 0; i < n; i++ {
				speed /= 10
			}
			spamMutex.Lock()
			spamSpeed = speed
			spamMutex.Unlock()
			sendMessage(chatID, fmt.Sprintf("Speed: %g", speed))
			return
		}

		// speed <number>
		s, err := strconv.ParseFloat(arg, 64)
		if err != nil || s <= 0 || s > 60 {
			sendMessage(chatID, "Usage: speed <n> | speed lowend [n]")
			return
		}
		spamMutex.Lock()
		spamSpeed = s
		spamMutex.Unlock()
		sendMessage(chatID, fmt.Sprintf("Speed: %g", s))
		return
	}

	// BOMB
	if strings.HasPrefix(lower, "bomb") {
		parts := strings.Fields(text)
		if len(parts) < 2 {
			sendMessage(chatID, "Usage: bomb <10-digit-phone>")
			return
		}
		phone := parts[1]
		if len(phone) != 10 {
			sendMessage(chatID, "Phone must be 10 digits (without 0)")
			return
		}
		res := runBomb(phone)
		sendMessage(chatID, res)
		return
	}

	sendMessage(chatID, "Unknown command: "+text)
}

// ============================================================
// MAIN
// ============================================================
func main() {
	fmt.Println("Starting NAUH bot (Go + TDLib)...")

	loadFosh()

	// Auth
	authorizer := client.ClientAuthorizer()
	go client.CliInteractor(authorizer)

	authorizer.TdlibParameters <- &client.SetTdlibParametersRequest{
		UseTestDc:           false,
		DatabaseDirectory:   "./td_db",
		FilesDirectory:      "./td_files",
		UseFileDatabase:     true,
		UseChatInfoDatabase: true,
		UseMessageDatabase:  true,
		UseSecretChats:      false,
		ApiId:               API_ID,
		ApiHash:             API_HASH,
		SystemLanguageCode:  "en",
		DeviceModel:         "Linux",
		SystemVersion:       "1.0",
		ApplicationVersion:  "1.0",
	}

	_, err := client.SetLogVerbosityLevel(&client.SetLogVerbosityLevelRequest{
		NewVerbosityLevel: 1,
	})
	if err != nil {
		log.Fatal(err)
	}

	tdClient, err = client.NewClient(authorizer)
	if err != nil {
		log.Fatal(err)
	}

	me, err := tdClient.GetMe()
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("[AUTH] Logged in as: %s %s (ID: %d)\n",
		me.FirstName, me.LastName, me.Id)

	// Listen for messages
	listener := tdClient.GetListener()

	fmt.Println("Bot running. Press Ctrl+C to stop.")

	for update := range listener.Updates {
		switch u := update.(type) {
		case *client.UpdateNewMessage:
			msg := u.Message
			chatID := msg.ChatId

			// Extract text
			var text string
			if mt, ok := msg.Content.(*client.MessageText); ok {
				text = mt.Text.Text
			}
			if text == "" {
				continue
			}

			// Get sender ID
			var senderID int64
			if su, ok := msg.SenderId.(*client.MessageSenderUser); ok {
				senderID = su.UserId
			}

			go handleCommand(text, chatID, senderID)
		}
	}
}
