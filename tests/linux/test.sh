# tests/linux/test.sh - сценарий для busybox sh на MyOS (этап 11, прогон
# linux в tools/autotest.py). Каждая команда busybox - отдельный fork +
# execve; конвейеры - каналы; $(...) - подоболочки.
BB=/usb0p1/busybox
fail() { echo "SCRIPT FAIL: $1"; exit 1; }

echo "script start: $($BB uname -s) $($BB uname -m)"

x=0
for i in 1 2 3 4 5; do x=$((x + i)); done
[ "$x" = 15 ] || fail "loop ($x)"

n=$(echo "hello world" | $BB wc -w)
[ "$n" = 2 ] || fail "pipe to wc ($n)"

$BB seq 1 100 > /tmp/seq.txt
c=$($BB wc -l < /tmp/seq.txt)
[ "$c" = 100 ] || fail "seq > file, wc < file ($c)"

s=$($BB sort -rn /tmp/seq.txt | $BB head -n 1)
[ "$s" = 100 ] || fail "sort | head ($s)"

g=$($BB grep -c 7 /tmp/seq.txt)
[ "$g" = 19 ] || fail "grep -c ($g)"

t=$(echo abc | $BB sed 's/b/X/')
[ "$t" = aXc ] || fail "sed ($t)"

$BB ls /usb0p1 | $BB grep -q busybox || fail "ls | grep"

$BB rm /tmp/seq.txt
[ -e /tmp/seq.txt ] && fail "rm"

( exit 3 )
[ $? = 3 ] || fail "subshell exit code"

$BB sleep 1 &
wait $! || fail "background job"

echo "SCRIPT OK"
