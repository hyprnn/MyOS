#!/bin/sh
# Пересоздать user/tls/roots.c - корневые сертификаты для TLS.
# Нужны: собранная утилита brssl из BearSSL и список Mozilla
# (пакет ca-certificates: /usr/share/ca-certificates/mozilla/*.crt).
# НЕ берите /etc/ssl/certs/ca-certificates.crt: туда корпоративные
# прокси и антивирусы добавляют свои сертификаты.
#   ./tools/mkroots.sh /путь/к/brssl
set -e
BRSSL=${1:-brssl}
MOZ=${MOZ:-/usr/share/ca-certificates/mozilla}
cat "$MOZ"/*.crt > /tmp/myos-roots.pem
{
  sed -n '1,9p' user/tls/roots.c
  "$BRSSL" ta /tmp/myos-roots.pem
  printf '\nconst br_x509_trust_anchor *const g_tls_roots = TAs;\nconst size_t g_tls_roots_num = TAs_NUM;\n'
} > /tmp/myos-roots.c
mv /tmp/myos-roots.c user/tls/roots.c
echo "user/tls/roots.c: $(grep -c 'BR_X509_TA_CA' user/tls/roots.c) roots"
