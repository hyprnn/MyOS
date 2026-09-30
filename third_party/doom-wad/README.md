# DOOM1.WAD — бесплатная (shareware) версия DOOM 1.9

Файл игры первого эпизода DOOM («Knee-Deep in the Dead», 9 уровней)
от id Software. Shareware-версию id Software разрешила свободно
распространять без изменений и бесплатно — поэтому она лежит здесь,
а `make` кладёт её на загрузочную флешку (`esp/DOOM1.WAD`), установщик
(`tools/install-arch.sh`) — рядом с MyOS в `EFI/MyOS/`.

    размер 4 196 020 байт
    MD5    f0cefca49926d00903cf57551d901abe  (официальная DOOM 1.9 shareware)

Полная игра (`doom.wad`, `doom2.wad`) — не свободная: её нужно купить
и положить на флешку самому; MyOS найдёт её (`doom` ищет doom1.wad,
doom.wad, doom2.wad, freedoom1.wad, freedoom2.wad в корне дисков, в
папках `doom/` и `EFI/MyOS/`), или `doom -iwad путь`. Freedoom
(freedoom.github.io, лицензия BSD) — свободная замена, тоже работает.
