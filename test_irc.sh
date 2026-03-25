#!/bin/bash

HOST="127.0.0.1"
PORT="6667"
PASS="pass"

GREEN="\033[0;32m"
RED="\033[0;31m"
NC="\033[0m"

pass_test() { echo -e "${GREEN}✅ $1${NC}"; }
fail_test() { echo -e "${RED}❌ $1${NC}"; }

run_test() {
    NAME="$1"
    CMDS="$2"
    EXPECT="$3"

    echo "---- $NAME ----"

    OUTPUT=$(
    {
        echo -e "$CMDS" | while IFS= read -r line; do
            printf "%s\r\n" "$line"
            sleep 0.1
        done
    } | nc -w 2 $HOST $PORT 2>/dev/null
    )

    echo "$OUTPUT"

    if [ -z "$EXPECT" ]; then
        if echo "$OUTPUT" | grep -qE "4[0-9][0-9]"; then
            fail_test "$NAME (unexpected error)"
        else
            pass_test "$NAME"
        fi
    else
        echo "$OUTPUT" | grep -q "$EXPECT"
        if [ $? -eq 0 ]; then
            pass_test "$NAME"
        else
            fail_test "$NAME (expected: $EXPECT)"
        fi
    fi
    echo
}

echo "🚀 IRC FULL TEST SUITE"
echo

# ========================
# 🧪 1. AUTH / REGISTER
# ========================

run_test "JOIN without register" \
"NICK test\nJOIN #chan\n" "451"

run_test "USER without NICK" \
"USER test 0 * :real\n" ""

run_test "Full register" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\n" "001"

run_test "Wrong PASS" \
"PASS wrong\nNICK test\nUSER test 0 * :real\n" "464"

run_test "Double USER" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\nUSER test 0 * :real\n" "462"

run_test "Double NICK (allowed)" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\nNICK newnick\n" "001"

# ========================
# 🧪 2. CHANNEL
# ========================

run_test "JOIN normal" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\nJOIN #chan\n" "353"

run_test "JOIN invalid channel" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\nJOIN chan\n" "403"

run_test "JOIN twice" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\nJOIN #chan\nJOIN #chan\n" "353"

run_test "PART valid" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\nJOIN #chan\nPART #chan\n" "PART"

run_test "PART not in channel" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\nPART #chan\n" "442"

# ========================
# 🧪 3. MULTI CLIENTS
# ========================

echo "🧪 Multi-client tests"

(
{
    echo "PASS $PASS"
    sleep 0.1
    echo "NICK a"
    sleep 0.1
    echo "USER a 0 * :a"
    sleep 0.1
    echo "JOIN #chan"
} | nc -w 2 $HOST $PORT &
sleep 0.5

OUTPUT=$(
{
    echo "PASS $PASS"
    sleep 0.1
    echo "NICK b"
    sleep 0.1
    echo "USER b 0 * :b"
    sleep 0.1
    echo "JOIN #chan"
} | nc -w 2 $HOST $PORT
)

echo "$OUTPUT" | grep -q "353"

if [ $? -eq 0 ]; then
    pass_test "Multi-client JOIN"
else
    fail_test "Multi-client JOIN"
fi
)

# ========================
# 🧪 4. PRIVMSG
# ========================

run_test "PRIVMSG no target" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nPRIVMSG\n" "461"

run_test "PRIVMSG no message" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nPRIVMSG test\n" "461"

run_test "PRIVMSG unknown nick" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nPRIVMSG ghost :hi\n" "401"

run_test "PRIVMSG not in channel" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nPRIVMSG #chan :hi\n" "404"

# ========================
# 🧪 5. MODE
# ========================

run_test "MODE not operator" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nJOIN #chan\nMODE #chan +i\n" "482"

run_test "MODE +i" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nJOIN #chan\nMODE #chan +i\n" "MODE"

# ========================
# 🧪 6. INVITE
# ========================

echo "🧪 INVITE test"

(
{
    echo "PASS $PASS"
    sleep 0.1
    echo "NICK a"
    sleep 0.1
    echo "USER a 0 * :a"
    sleep 0.1
    echo "JOIN #chan"
    sleep 0.1
    echo "MODE #chan +i"
} | nc -w 2 $HOST $PORT &
sleep 0.5

OUTPUT=$(
{
    echo "PASS $PASS"
    sleep 0.1
    echo "NICK b"
    sleep 0.1
    echo "USER b 0 * :b"
    sleep 0.1
    echo "JOIN #chan"
} | nc -w 2 $HOST $PORT
)

echo "$OUTPUT" | grep -q "473"

if [ $? -eq 0 ]; then
    pass_test "Invite-only block"
else
    fail_test "Invite-only block"
fi
)

# ========================
# 🧪 7. KICK
# ========================

run_test "KICK basic" \
"PASS $PASS\nNICK a\nUSER a 0 * :a\nJOIN #chan\nKICK #chan a\n" "KICK"

run_test "KICK non-op" \
"PASS $PASS\nNICK b\nUSER b 0 * :b\nJOIN #chan\nKICK #chan b\n" "482"

# ========================
# 🧪 8. QUIT
# ========================

echo "🧪 Fragmentation test"

OUTPUT=$(
{
    printf "PA"
    sleep 0.05
    printf "SS pass\r\nNI"
    sleep 0.05
    printf "CK test\r\nUS"
    sleep 0.05
    printf "ER test 0 * :real\r\n"
} | nc -w 2 $HOST $PORT
)

run_test "Weird spacing" \
"PASS $PASS\nNICK    test\nUSER   test   0   *   :real\nJOIN    #chan\n" "353"

echo "$OUTPUT" | grep -q "001"

if [ $? -eq 0 ]; then
    pass_test "Fragmented input handling"
else
    fail_test "Fragmented input handling"
fi


run_test "QUIT" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nQUIT :bye\n" "QUIT"

# ========================
# 🧪 9. WHO
# ========================

run_test "WHO" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nJOIN #chan\nWHO #chan\n" "352"

# ========================
# 🧪 10. PING
# ========================

run_test "PING/PONG" \
"PING 123\n" "PONG"

# ========================
# 💣 HARD TESTS
# ========================

run_test "Nick collision" \
"PASS $PASS\nNICK test\nUSER test 0 * :real\n" "001"

(
echo -e "PASS $PASS\nNICK test\nUSER test 0 * :real\n" | nc $HOST $PORT &
sleep 1
echo -e "PASS $PASS\nNICK test\nUSER test 0 * :real\n" | nc $HOST $PORT | grep -q "433"

if [ $? -eq 0 ]; then
    pass_test "Nick collision"
else
    fail_test "Nick collision"
fi
)

run_test "Channel delete auto" \
"PASS $PASS\nNICK t\nUSER t 0 * :t\nJOIN #chan\nPART #chan\nJOIN #chan\n" "353"

echo "🧪 Spam JOIN/PART"

(
echo -e "PASS $PASS\nNICK t\nUSER t 0 * :t\n" | nc $HOST $PORT &
sleep 1
for i in {1..10}; do
    echo -e "JOIN #chan\nPART #chan\n" | nc -w 1 $HOST $PORT
done
pass_test "Spam JOIN/PART (no crash)"
)

echo
echo "🏁 ALL TESTS DONE"
