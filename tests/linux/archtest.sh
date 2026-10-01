#!/bin/bash
# tests/linux/archtest.sh - проверка программ Arch Linux в MyOS (этап 11,
# шаг 2). autotest кладёт его в образ корня Arch (/opt/myos) и запускает
# из шелла MyOS: bash /opt/myos/archtest.sh [HTTP-URL HTTPS-URL РАЗМЕР]
# (адреса тестовых серверов на хосте и размер big.bin). Каждая проверка -
# "ok имя" или "FAIL имя: ...", в конце - "ARCHTEST DONE pass=N fail=M".
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "ok   $1"; }
bad() { fail=$((fail+1)); echo "FAIL $1: $2"; }
check() { # имя, ожидаемое, команда...
    local name=$1 want=$2; shift 2
    local got; got=$("$@" 2>&1)
    if [ "$got" = "$want" ]; then ok "$name"; else bad "$name" "got [$got] want [$want]"; fi
}
check uname "Linux" uname -s
check pwd-root "/" bash -c 'cd / && pwd'
check readlink-f "/usr/lib/ld-linux-x86-64.so.2" readlink -f /lib64/ld-linux-x86-64.so.2
check readlink "usr/lib" readlink /lib64
check stat-link "symbolic link" stat -c %F /bin
check stat-dir "directory" stat -c %F /usr
check stat-ino-diff "yes" bash -c '[ "$(stat -c %i /etc/os-release)" != "$(stat -c %i /usr)" ] && echo yes'
check os-release "Arch Linux" bash -c '. /etc/os-release; echo $NAME'
check pipe-sort "a b c" bash -c 'printf "c\nb\na\n" | sort | tr "\n" " " | sed "s/ $//"'
check uniq-c "2 x" bash -c 'printf "x\nx\n" | uniq -c | awk "{print \$1, \$2}"'
check awk-sum "55" bash -c 'seq 1 10 | awk "{s+=\$1} END {print s}"'
check grep-count "1" bash -c 'grep -c "^root:" /etc/passwd'
check find-count "yes" bash -c '[ $(find /usr/lib -maxdepth 1 -name "libc.so*" | wc -l) -ge 1 ] && echo yes'
check subshell "42" bash -c 'x=$( (echo 42) ); echo $x'
check arith "1024" bash -c 'echo $((2**10))'
check array "3" bash -c 'a=(1 2 3); echo ${#a[@]}'
check heredoc "hi there" bash -c 'cat <<E
hi there
E'
check tmp-write "data" bash -c 'echo data > /tmp/t1 && cat /tmp/t1'
check mkdir-p "ok" bash -c 'mkdir -p /tmp/a/b/c && [ -d /tmp/a/b/c ] && echo ok'
check rm-rf "gone" bash -c 'rm -rf /tmp/a && [ ! -e /tmp/a ] && echo gone'
check ln-s "data" bash -c 'ln -sf /tmp/t1 /tmp/t1link 2>/dev/null; cat /tmp/t1'
check sha256 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" bash -c 'printf abc | sha256sum | cut -d" " -f1'
check md5 "900150983cd24fb0d6963f7d28e17f72" bash -c 'printf abc | md5sum | cut -d" " -f1'
check gzip "roundtrip" bash -c 'seq 1 2000 > /tmp/n && gzip -c /tmp/n > /tmp/n.gz && [ "$(gzip -dc /tmp/n.gz | md5sum)" = "$(md5sum < /tmp/n)" ] && echo roundtrip'
check xz "roundtrip" bash -c 'xz -c /tmp/n > /tmp/n.xz && [ "$(xz -dc /tmp/n.xz | md5sum)" = "$(md5sum < /tmp/n)" ] && echo roundtrip'
check zstd "roundtrip" bash -c 'zstd -q -c /tmp/n > /tmp/n.zst && [ "$(zstd -q -dc /tmp/n.zst | md5sum)" = "$(md5sum < /tmp/n)" ] && echo roundtrip'
check tar "n" bash -c 'cd /tmp && tar cf /tmp/x.tar n && mkdir -p /tmp/u && tar xf /tmp/x.tar -C /tmp/u && ls /tmp/u'
check openssl "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" bash -c 'printf abc | openssl dgst -sha256 -r | cut -d" " -f1'
check sqlite "3" bash -c 'rm -f /tmp/t.db; sqlite3 /tmp/t.db "create table t(x); insert into t values(1),(2),(3); select count(*) from t;"'
check file-elf "ELF" bash -c 'file -b /usr/bin/bash | cut -c1-3'
check pacman-q "yes" bash -c '[ $(pacman -Q 2>/dev/null | wc -l) -gt 50 ] && echo yes'
check date-year "yes" bash -c '[ $(date +%Y) -ge 2025 ] && echo yes'
check id "0" id -u
check kill-trap "trapped" bash -c 'trap "echo trapped; exit 0" USR1; kill -USR1 $$; sleep 1'
check bg-wait "done" bash -c 'sleep 0.2 & wait $!; echo done'
check myos-escape "yes" bash -c '[ -d /myos/tmp ] && echo yes'
# /home - отдельный том из /etc/fstab (UUID=...)
check fstab-home "hello from home" cat /home/user/hello.txt
# сеть: HTTP, большой файл, HTTPS (сертификат тестовый - без проверки)
if [ -n "$1" ]; then
    check curl-http "Hello from the host over HTTP!" curl -sS "$1/hello.txt"
    check curl-big "$3" bash -c "curl -sS -o /tmp/big.bin '$1/big.bin' && stat -c %s /tmp/big.bin"
    check curl-https "Hello from the host over HTTP!" curl -sS -k "$2/hello.txt"
fi
echo "ARCHTEST DONE pass=$pass fail=$fail"
