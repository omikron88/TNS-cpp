# Původ a licence

Port vychází z [omikron88/TNS](https://github.com/omikron88/TNS), commit
`b50b0bf8e7c1dd1a5b31ce913b59d1f89733e131`. Původní repozitář je distribuován
pod GNU GPL verze 3; úplný text je v `LICENSE`. Tento odvozený port zachovává
GPL-3.0. Zdrojová struktura, ROM, ikony převedené do `src/icons.hpp` a ukázková
média pocházejí z tohoto commitu. Samostatná licenční oznámení pro ROM či média
v původním repozitáři nejsou. Při další distribuci zachovejte tato oznámení,
licenci a povinnosti zpřístupnění odpovídajících zdrojových kódů.

Jádro `src/z80.hpp` je přímý port `src/z80core/Z80.java`. Původní autoři:
Alberto Sánchez Terrén (copyright 2004) a José Luis Sánchez Villanueva.
Implementace jednotlivých instrukcí, příznaků, MEMPTR a časování byla zachována;
byly upraveny typy, syntaxe, inicializace a callbacky pro C++11. Změny oproti
původnímu projektu popisuje `docs/compatibility.md`.

SDL2 2.32.10, commit `5d249570393f7a37e037abf22cd6012a4cc56a71`, je volitelně
staženo a sestaveno přes CMake FetchContent; není součástí zdrojového stromu.
SDL používá licenci zlib, viz `licenses/SDL2.txt`. Statické distribuční balíčky
musí obsahovat toto oznámení. Připnutí commitu lze při offline sestavení doplnit
parametrem `FETCHCONTENT_SOURCE_DIR_SDL2` na již připravený zdrojový strom SDL.

Referenční stopy v `tests/java-*.trace` byly získány ze skutečného běhu původní
Javy, nikoli z portovaného emulátoru. Zdroje orákul a postup opakování jsou
v `tests/reference` a `tools/compare_java.py`. Java není potřeba k sestavení
ani spuštění C++ emulátoru.
