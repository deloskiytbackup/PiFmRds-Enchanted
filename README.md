# PiFmRds-Enchanted (Wydanie 2026)

**PiFmRds-Enchanted** to zmodernizowany, zaawansowany nadajnik FM ze stereofonią MPX i pełnym systemem RDS (Radio Data System) dla Raspberry Pi, komputerów PC oraz transmiterów SDR, przepisany pod standardy technologiczne **2026 roku**.

Oryginalny kod: Christophe Jacquet, Richard Hirst, Oliver Mattos.  
Wydanie **Enchanted**: Marcel Siepielski (2026).

---

## 🌟 Najważniejsze nowości i zmiany (Standard 2026)

1. **Dynamiczne wykrywanie sprzętu w czasie działania (Runtime Detection)**:
   - Całkowicie usunięto sztywne wykrywanie modelu w starym Makefile (`cat /proc/device-tree/model`).
   - Pojedynczy plik binarny automatycznie rozpoznaje model Raspberry Pi (Pi 1, 2, 3, 4, Compute Module, Zero, Zero 2W), dobiera odpowiednie bazy rejestrów SoC (`0x20000000`, `0x3F000000`, `0xFE000000`) oraz taktowanie PLL (500 MHz / 750 MHz).
   - Wsparcie dla 64-bitowych systemów **Raspberry Pi OS (Debian 12 Bookworm / Debian 13 Trixie)** z jądrem Linux 6.x+.

2. **Kwestia Raspberry Pi 5 oraz tryb Universal SDR/MPX**:
   - W Raspberry Pi 5 piny GPIO są podłączone przez układ mostka południowego **RP1** po magistrali PCIe, co fizycznie uniemożliwia bezpośrednią syntezę radiową na pinie 4.
   - Program inteligentnie diagnozuje Pi 5 i oferuje tryb **Universal MPX/SDR**, generując pełny strumień composite baseband MPX (228 kHz WAV / stdout) do nadawania przez SDR (HackRF, LimeSDR, FL2k) lub do zewnętrznych transmiterów. Działa także na PC i Mac!

3. **Nowoczesny silnik RDS (IEC 62106 / RBDS)**:
   - **RT+ (RadioText Plus - Grupy 3A / 11A)**: Przesyłanie znaczników wykonawcy (*Artist*) i utworu (*Title*) – nowoczesne radia samochodowe wyświetlają dedykowane pola metadanych.
   - **Dynamiczny PS z inteligentnym podziałem słów**: Płynne stronicowanie lub przewijanie długich nazw stacji bez ucinania słów w połowie.
   - **Automatyczny zegar CT (Clock Time - Grupa 4A)**: Odbiorniki radiowe automatycznie synchronizują swoją datę i godzinę z czasem systemowym Raspberry Pi (NTP).
   - **Kody PTY**: Wybór między standardem europejskim RDS a północnoamerykańskim RBDS.
   - **Alternatywne częstotliwości (AF)** oraz flagi **TP/TA (Traffic Announcement)**.
   - Normalizacja polskich znaków diakrytycznych (ą, ć, ę, ł, ń, ó, ś, ź, ż) i znaków europejskich.

4. **Współczesny tor audio (PipeWire / ALSA / Stdin)**:
   - Wbudowana, niezależna obsługa plików WAV (PCM 16-bit) oraz wejścia standardowego `stdin` (np. bezpośredni rurociąg z `ffmpeg`, `mpv` czy strumieni internetowych).
   - Wybór preemfazy: 50 µs (Europa, Azja) lub 75 µs (Ameryka).
   - Precyzyjny generator podnośnej stereo 19 kHz / 38 kHz.

5. **Nowoczesna architektura i narzędzia**:
   - System budowania **CMake (>= 3.20)**, standard **C17/C23**.
   - Modularne biblioteki: `librds`, `libfm_mpx`, `libhw_rpi`, `libcontrol`.
   - Gotowe testy jednostkowe CTest (`test_rds_crc`, `test_rds_strings`, `test_rds_groups`).
   - Narzędzie sterujące w Pythonie 3: `pifm-ctl.py` (obsługa przez socket UNIX lub potok FIFO).
   - Gotowa usługa systemd: `pifm-enchanted.service`.

---

## 🚀 Szybki start (Kompilacja i instalacja)

### 1. Wymagania systemowe
Na Raspberry Pi OS:
```bash
sudo apt update
sudo apt install -y build-essential cmake
```

### 2. Kompilacja za pomocą CMake
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest --output-on-failure
sudo make install
```

---

## 📻 Przykłady użycia

### 1. Podstawowe nadawanie z pliku audio (Raspberry Pi 1–4)
Podłącz kabelek (antenę testową) do **GPIO 4 (fizyczny pin 7)**:
```bash
sudo pi_fm_rds -f 107.9 -a sound.wav --ps "ENCHNTED" --rt "Radio PiFmRds-Enchanted 2026"
```

### 2. Dynamiczny PS oraz metadane RT+ (Tytuł i Wykonawca)
```bash
sudo pi_fm_rds -f 107.9 -a sound.wav \
  --dynamic-ps "RADIO ENCHANTED 2026 ODDZIAL FM" \
  --ps-mode page --ps-rate 2000 \
  --title "Blinding Lights" --artist "The Weeknd"
```

### 3. Transmisja na żywo ze stacji internetowej lub odtwarzacza (PipeWire / ffmpeg)
```bash
ffmpeg -i "https://twoje-ulubione-radio.pl/stream.mp3" -f s16le -ar 44100 -ac 2 - \
  | sudo pi_fm_rds -f 107.9 -a - --ps "LIVE-WEB"
```

### 4. Zmiana metadanych w locie za pomocą narzędzia `pifm-ctl.py`
Uruchom nadajnik z włączonym gniazdem sterującym:
```bash
sudo pi_fm_rds -f 107.9 -a sound.wav --sock /tmp/pifm_sock
```
W osobnym terminalu lub skrypcie:
```bash
python3 tools/pifm-ctl.py --title "Nowy Utwor" --artist "Znany Artysta" --ta on
```

### 5. Generowanie pliku MPX WAV (Dla Raspberry Pi 5, PC, Mac lub nadajników SDR)
```bash
pi_fm_rds -a sound.wav -o broadcast_mpx.wav --ps "ENCHNTED" --title "Song" --artist "Artist"
```
Wygenerowany plik `broadcast_mpx.wav` można bezpośrednio przesłać do nadajnika SDR:
```bash
hackrf_transfer -t broadcast_mpx.wav -f 107900000 -s 228000 -a 1 -x 20
```

---

## ⚠️ Ostrzeżenia techniczne i prawne

- **Filtr dolnoprzepustowy (LPF):** Sygnał z pinu GPIO 4 jest falą prostokątną i posiada silne nieparzyste harmoniczne (zwłaszcza 3. harmoniczną na częstotliwościach ~300 MHz). Aby nie powodować zakłóceń, **bezwzględnie stosuj filtr LPF**. Szczegółowy schemat i opis elementów znajdziesz w dokumencie: [doc/lpf_filter.md](doc/lpf_filter.md).
- **Zgodność sprzętowa:** Pełną analizę kompatybilności układów SoC (BCM2835 do BCM2712) znajdziesz w dokumencie: [doc/hardware_compat.md](doc/hardware_compat.md).

---

## 📜 Licencja

Projekt udostępniony jest na warunkach licencji **GNU General Public License v3 (GPLv3)**.
