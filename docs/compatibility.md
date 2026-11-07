# Shoda s původní implementací

Referenční commit Javy: `b50b0bf8e7c1dd1a5b31ce913b59d1f89733e131`.
Port je nativní C++11; Java se za běhu nespouští. V C++ zůstává stejný
repertoár Z80 včetně CB/ED/DD/FD/DDCB/FDCB a nedokumentovaných instrukcí,
pořadí přístupů na sběrnici, INT/NMI, příznaky a MEMPTR.

## Reprodukovatelné porovnání

S nainstalovaným JDK:

```sh
git clone https://github.com/omikron88/TNS.git ../tns-java-reference
git -C ../tns-java-reference checkout b50b0bf8e7c1dd1a5b31ce913b59d1f89733e131
python3 tools/compare_java.py --reference ../tns-java-reference --tns build/tns
```

Skript vytváří dočasnou kopii Javy, kompiluje originální CPU a periferie,
pouští orákula a porovnává pozorování s uloženými referenčními stopami a C++.
Nemění upstream checkout ani referenční stopy. Pro Eclipse compiler lze zadat
`--javac 'java -jar /cesta/ecj.jar'`.

V dočasné kopii jsou upraveny pouze dotazy AWT na stav zámků klávesnice,
opravena dvojitá středníková značka za importem pro Eclipse compiler a
nahrazeno překreslení Swing panelu prázdným callbackem. Klávesy pro test
zavádění jsou vloženy do původního vstupního registru. Samotné instrukce,
paměť, grafické vykreslování a řadiče Javy zůstávají původní. ROM jsou načteny
z původních resource souborů. Diskové operace testů používají pracovní kopie.

- **3584 případů CPU**: všech 256 hodnot opkódu v sedmi skupinách, dvě
  deterministické sady registrů a 64KiB paměti pro každý případ. Porovnávají
  se hlavní i alternativní registry, PC/SP/IX/IY/IR, MEMPTR, HALT/IFF/EI,
  takty, součet celé paměti a stopa výstupu na porty. Není to úplný důkaz
  správnosti všech možných programů ani samostatné spuštění ZEXALL.
- **GC a ITK, 100 snímků každá**: po každém snímku registry CPU, součet
  fyzické RAM i VRAM, takty a součet každého pixelu rastru 640×256.
- **CP/M, 500 snímků**: originální `tnsdemo.8sd`, klávesy F v čase snímku
  100, 0 ve 125 a Enter ve 150. Skutečné čtení přes FDC, zavedení CP/M
  a prompt `00.A>`. Stejná kontrola CPU/paměti/taktů/rastru po každém snímku.
- Doplňkové C++ testy ověřují RAM, ROM overlay, mapování/PFL s prodlevou,
  EI/NMI, klávesové tabulky, přímý i bufferovaný zápis/čtení FDC, HDD,
  neplatný sektor, vyjmutí aktivní mechaniky a chování bez média.
- SDL test ovládá skutečné události myši a klávesnice, pauzu, nastavení,
  dialog souboru, nápovědu a čte vykreslené pixely. Používá softwarový
  renderer a dummy videodriver; nekontroluje interaktivní chování všech
  hostitelských desktopů. Výstup `ui.ppm` je v adresáři sestavení.

## Vědomé odlišnosti a opravy

- Swing byl nahrazen SDL dialogy a souborovým prohlížečem. Rozložení lišty,
  ikony, rastr emulovaného obrazu a rozměry zůstávají blízké originálu;
  dekorace a ovládání hostitelských dialogů nejsou pixelovou kopií Swingu.
- Přímý zápis do datového registru FDC v Javě pouze zvyšoval čítač; port
  zapisuje data do sektorového bufferu a dokončený sektor uloží. Původní
  bufferovaný zápis funguje také. Zkrácené obrazy, chyby I/O a média pouze
  pro čtení vracejí chybu místo předstíraného úspěchu.
- Přístup k bufferu FDC je ohraničen; opakované čtení po konci přenosu
  nezpůsobí přístup mimo paměť. Nový obraz nepřepíše již existující soubor.
- Port B0 v režimu ITK potvrzuje požadavek příslušné ITK karty; originál
  omylem volal GC. Reset také maže čekající časovač a grafický obraz při
  přepnutí karty. CPU registr R je při inkrementaci omezen na sedm bitů,
  zatímco jeho bit 7 zůstává samostatný; tím se zabrání přetečení po dlouhém běhu.
- Nastavení se zapisuje jako jednoduché UTF-8 `KEY=value`. Původní běžné
  položky DRIVE1–4/HDD/RAMSIZE/VIDEOBW/VIDEOITK jsou podporované; nejde
  o úplný parser všech escape sekvencí Java Properties. Umístění lze zvolit
  parametrem `--config`. Výchozí je pracovní adresář.
- Hostitelské cesty používají UTF-8 (na Windows převod na široké souborové
  API). Dialogový font je původní jednoduchý ITK; ne-ASCII názvy se zobrazují
  zástupnými znaky, ale cesta se uchovává celá.
- Nejsou přidány periférie, které upstream neemuluje, například zvuk.
  Obrazy ROM a médií lze používat bez připojení k síti.

## Co bylo ověřeno v tomto pracovním prostředí

Linux: dynamické i statické sestavení, testy s dummy SDL a kontrola
AddressSanitizer/UndefinedBehaviorSanitizer. Živé porovnání původní Javy
proběhlo přes Eclipse compiler 3.42.0. Windows: cross překlad MinGW-w64
GCC 14 a kontrola importů výsledného EXE (pouze systémové DLL).
Nativní spuštění Windows/MSYS2 zde nebylo provedeno; workflow GitHub Actions
je připraveno, ale bez spuštění CI není jeho výsledek doložen.
