# PiFmRds-Enchanted (Wydanie 2026)

[![CI & Packaging](https://github.com/deloskiytbackup/PiFmRds-Enchanted/actions/workflows/ci.yml/badge.svg)](https://github.com/deloskiytbackup/PiFmRds-Enchanted/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Standard: C17](https://img.shields.io/badge/Standard-C17-orange.svg)]()
[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%201--4%20%7C%20Pi%205%20%7C%20PC%20%7C%20SDR-brightgreen.svg)]()

**PiFmRds-Enchanted** to zmodernizowany, profesjonalny nadajnik FM ze stereofonią MPX, procesorem dźwięku DSP, pełnym systemem RDS (Radio Data System) oraz bezpośrednim streamingiem I/Q dla transmiterów SDR (HackRF, LimeSDR, FL2k) i mikrokomputerów Raspberry Pi, przepisany pod standardy technologiczne **2026 roku**.

Oryginalny kod: Christophe Jacquet, Richard Hirst, Oliver Mattos.  
Wydanie **Enchanted**: Marcel Siepielski (2026).

---

## 🌟 Najważniejsze funkcje i nowości (Standard 2026)

### 1. 🎚️ Profesjonalny procesor dźwięku DSP (Broadcast Audio Processor)
- **Stromy filtr dolnoprzepustowy 15 kHz (Brickwall LPF)**: 4-biegunowy filtr Butterwortha skutecznie odcinający pasmo powyżej 15 kHz, zapobiegając zniekształceniom i interferencjom z pilotem stereofonicznym 19 kHz.
- **Automatyczna kontrola wzmocnienia (Broadcast AGC)**: Wyrównuje poziom cichych i głośnych fragmentów, zapewniając soczyste, profesjonalne brzmienie radia komercyjnego.
- **Limiter szczytowy MPX**: Miękki limiter zapobiegający przekroczeniu przepisowej dewiacji $\pm 75\text{ kHz}$ nawet przy silnej preemfazie.

### 2. 📡 Bezpośrednie wyjście SDR I/Q (Raspberry Pi 5, PC, Mac, HackRF, LimeSDR, FL2k)
- Bezpośrednia modulacja kwadraturowa I/Q w czasie rzeczywistym (`--iq-out -`).
- Możliwość bezpośredniego strumieniowania do nadajników SDR bez plików pośrednich:
  - Obsługa formatów: `s8` (HackRF), `u8` (FL2k), `s16` (LimeSDR, BladeRF), `f32` (GNU Radio, SDR++).
  - Rozwiązuje ograniczenie układu RP1 w **Raspberry Pi 5** i pozwala nadawać czysty sygnał radiowy z dowolnego komputera!

### 3. 🔍 Dynamiczne wykrywanie sprzętu (Runtime Detection)
- Pojedynczy plik binarny automatycznie rozpoznaje model Raspberry Pi (Pi 1, 2, 3, 4, Compute Module, Zero, Zero 2W) i dobiera właściwe adresy rejestrów SoC (`0x20000000`, `0x3F000000`, `0xFE000000`) oraz taktowanie PLL (500 MHz / 750 MHz).
- Pełna zgodność z 64-bitowymi systemami **Raspberry Pi OS (Debian 12 Bookworm / Debian 13 Trixie)** z jądrem Linux 6.x+.

### 4. 📻 Zaawansowany silnik RDS (IEC 62106 / RBDS)
- **RT+ (RadioText Plus - Grupy 3A / 11A)**: Przesyłanie znaczników wykonawcy (*Artist*) i utworu (*Title*) dla nowoczesnych systemów multimedialnych w samochodach.
- **Dynamiczny PS z inteligentnym podziałem słów**: Płynne stronicowanie lub przewijanie długich nazw stacji bez ucinania słów w połowie.
- **Automatyczny zegar CT (Clock Time - Grupa 4A)**: Odbiorniki radiowe automatycznie synchronizują swoją datę i godzinę z czasem systemowym Raspberry Pi (NTP).
- **Kody PTY & AF**: Obsługa profili stacji (RDS/RBDS) oraz listy alternatywnych częstotliwości (AF) i flag TP/TA.
- **Normalizacja diakrytyków**: Pełna obsługa i bezpieczna konwersja polskich znaków (ą, ć, ę, ł, ń, ó, ś, ź, ż).

### 5. 📦 Paczkowanie `.deb` i CI/CD (GitHub Actions)
- Automatyczne kompilacje multi-arch (`arm64` dla Raspberry Pi OS 64-bit oraz `x86_64`).
- Gotowe pakiety instalacyjne `.deb` generowane przez `cpack`.

---

## 🚀 Szybki start (Kompilacja i instalacja)

### 1. Wymagania systemowe
Na Raspberry Pi OS / Ubuntu / Debian:
```bash
sudo apt update
sudo apt install -y build-essential cmake
```

### 2. Kompilacja i uruchomienie testów
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
ctest --output-on-failure
sudo make install
```

### 3. Tworzenie instalatora `.deb` (opcjonalnie)
```bash
cpack -G DEB
sudo dpkg -i pifmrds-enchanted_2.0.0_arm64.deb
```

---

## 📻 Przykłady użycia

### 1. Bezpośrednie nadawanie z GPIO 4 (Raspberry Pi 1, 2, 3, 4, Zero)
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

### 3. Nadawanie na Raspberry Pi 5 lub PC za pomocą HackRF One (SDR)
```bash
pi_fm_rds -a sound.wav --ps "ENCHNTED" --title "Starboy" --artist "The Weeknd" --iq-out - \
  | hackrf_transfer -t - -f 107900000 -s 2000000 -a 1 -x 20
```

### 4. Nadawanie przez przetwornik VGA FL2k (osmo-fl2k)
```bash
pi_fm_rds -a sound.wav --ps "ENCHNTED" --iq-format u8 --iq-rate 100000000 --iq-out - \
  | fl2k_file -s 100000000 -
```

### 5. Strumieniowanie ze stacji internetowej lub odtwarzacza (PipeWire / ffmpeg)
```bash
ffmpeg -i "https://stream.radio.example/live.mp3" -f s16le -ar 44100 -ac 2 - \
  | sudo pi_fm_rds -f 107.9 -a - --ps "LIVE-WEB"
```

### 6. Zmiana metadanych w czasie rzeczywistym (`pifm-ctl.py`)
W osobnym terminalu lub skrypcie:
```bash
pifm-ctl --title "Nowy Utwor" --artist "Znany Artysta" --ta on
```

---

## ⚠️ Ostrzeżenia techniczne i prawne

- **Filtr dolnoprzepustowy (LPF):** Sygnał z pinu GPIO 4 jest falą prostokątną i posiada silne nieparzyste harmoniczne (zwłaszcza 3. harmoniczną na częstotliwościach ~300 MHz). Aby nie powodować zakłóceń, **bezwzględnie stosuj filtr LPF**. Szczegółowy schemat i opis elementów znajdziesz w dokumencie: [doc/lpf_filter.md](doc/lpf_filter.md).
- **Zgodność sprzętowa:** Pełną analizę kompatybilności układów SoC (BCM2835 do BCM2712) znajdziesz w dokumencie: [doc/hardware_compat.md](doc/hardware_compat.md).

---

## 📜 Licencja

Projekt udostępniony jest na warunkach licencji **GNU General Public License v3 (GPLv3)**.
