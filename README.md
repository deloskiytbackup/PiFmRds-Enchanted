<div align="center">

# 📻 PiFmRds-Enchanted
### Profesjonalny nadajnik FM/RDS nowej generacji dla Raspberry Pi, SDR i PC (Wydanie 2026)

[![Release](https://img.shields.io/github/v/release/deloskiytbackup/PiFmRds-Enchanted?color=brightgreen&label=Release)](https://github.com/deloskiytbackup/PiFmRds-Enchanted/releases)
[![CI & Packaging](https://github.com/deloskiytbackup/PiFmRds-Enchanted/actions/workflows/ci.yml/badge.svg)](https://github.com/deloskiytbackup/PiFmRds-Enchanted/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Standard: C17](https://img.shields.io/badge/Standard-C17-orange.svg)]()
[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%201--4%20%7C%20Pi%205%20%7C%20PC%20%7C%20SDR-purple.svg)]()

**PiFmRds-Enchanted** to całkowicie przepisana i zmodernizowana wersja kultowego projektu PiFmRds.  
Wprowadza broadcastowy procesor dźwięku DSP, bezpośrednią modulację kwadraturową I/Q dla nadajników SDR (HackRF, LimeSDR, FL2k), wsparcie dla Raspberry Pi 5, pełny standard RDS (RT+, Dynamic PS, zegar CT) oraz intuicyjną składnię poleceń bez archaicznych flag.

</div>

---

## ⚡ Szybki Start (Nowa, intuicyjna składnia)

Zapomnij o pamiętaniu skomplikowanych parametrów z 2012 roku (`-f`, `-a`, `--ps`, `--rt`):

```bash
# 1. Po prostu wskaż plik audio (domyślnie 107.9 MHz):
sudo pifm sound.wav

# 2. Wskaż plik i częstotliwość w dowolnej kolejności:
sudo pifm sound.wav 98.5
sudo pifm 101.2 sound.wav --station "HITS"

# 3. Zmiana utworu w RDS (RT+) w locie z drugiego terminala:
pifm ctl track "Blinding Lights" "The Weeknd"

# 4. Włączenie komunikatu drogowego TA:
pifm ctl ta on
```

---

## 📊 Porównanie: Oryginalny PiFmRds vs PiFmRds-Enchanted (2026)

| Funkcja | Oryginalny PiFmRds (2014) | **PiFmRds-Enchanted (2026)** |
| :--- | :---: | :---: |
| **Składnia CLI** | Skomplikowana (`-f`, `-a`, `--ps`, `--rt`) | **Intuicyjna (`pifm sound.wav 107.9`)** |
| **Sterowanie w locie** | Ręczne pisanie do FIFO pipe | **Wbudowane `pifm ctl [track/ps/ta/stop]`** |
| **Wsparcie dla Raspberry Pi 5** | ❌ Brak (niekompatybilne) | ✅ **Pełne (przez bezpośredni tryb SDR I/Q)** |
| **Kompatybilność PC / Mac / VM** | ❌ Tylko stary Raspberry Pi | ✅ **Działa wszędzie (tryb MPX WAV i SDR)** |
| **RadioText Plus (RT+)** | ❌ Brak | ✅ **Pełne (Tytuł utworu & Wykonawca)** |
| **Zegar radiowy (CT Group 4A)** | ⚠️ Uproszczony | ✅ **Pełna synchronizacja z czasem NTP** |
| **Dynamiczny PS** | ❌ Ucina słowa w połowie | ✅ **Inteligentny podział słów i płynny scroll** |
| **Filtr audio 15 kHz (Brickwall)** | ❌ Brak (ptaszkowanie stereo) | ✅ **4-biegunowy filtr Butterwortha 15 kHz** |
| **Procesor dynamiki (AGC / Limiter)**| ❌ Brak (ryzyko przesterowania) | ✅ **Broadcast AGC + limiter dewiacji 75 kHz** |
| **System budowania** | Przestarzały Makefile | ✅ **Nowoczesny CMake (>= 3.20) + CTest** |
| **Format paczek** | Brak | ✅ **Gotowe pakiety Debian `.deb` (CPack)** |
| **Format plików audio** | Zależność od `libsndfile` | ✅ **Wbudowany silnik WAV (Zero Dependencies)** |

---

## 🌟 Główne moduły i możliwości

### 🎚️ 1. Broadcast Audio DSP (Procesor Dźwięku)
- **15 kHz Brickwall Low-Pass Filter**: Stromy filtr dolnoprzepustowy (4. rzędu) chroni podnośną pilota stereofonicznego (19 kHz) przed interferencją z wysokimi częstotliwościami audio.
- **Broadcast AGC (Automatyczna Kontrola Wzmocnienia)**: Wyrównuje poziom głośności pomiędzy różnymi nagraniami, nadając transmisji profesjonalne, głośne brzmienie komercyjnej rozgłośni.
- **Limiter dewiacji MPX**: Zabezpiecza nadajnik przed nielegalnym przekroczeniem pasma $\pm 75\text{ kHz}$.

### 📡 2. Bezpośredni silnik SDR I/Q (HackRF, FL2k, LimeSDR)
Umożliwia nadawanie na **Raspberry Pi 5** (które ze względu na układ I/O RP1 po PCIe nie posiada bezpośredniego GPCLK0 na pinach) oraz na zwykłych komputerach PC/Mac:
```bash
# Nadawanie przez HackRF One w czasie rzeczywistym:
pifm sound.wav --iq-out - | hackrf_transfer -t - -f 107900000 -s 2000000 -a 1 -x 20

# Nadawanie przez tani konwerter VGA FL2k (osmo-fl2k):
pifm sound.wav --iq-format u8 --iq-rate 100000000 --iq-out - | fl2k_file -s 100000000 -
```

### 📻 3. Zaawansowany silnik RDS (IEC 62106 / RBDS)
- **RT+ (RadioText Plus)**: Ekrany nowoczesnych samochodów i radioodbiorników wyświetlają dedykowane pola metadanych: wykonawcę i tytuł piosenki.
- **Dynamiczny PS (Program Service)**: Stronicowanie nazw dłuższych niż 8 znaków z poszanowaniem wyrazów lub tryb płynnego przewijania.
- **Zegar CT (Clock Time - Grupa 4A)**: Odbiorniki radiowe w samochodach automatycznie ustawiają zegar i strefę czasową z systemowego NTP.
- **Wsparcie dla PTY, AF i TP/TA**: Wybór profili muzycznych/informacyjnych, alternatywne częstotliwości i priorytet komunikatów drogowych.
- **Normalizacja diakrytyków**: Automatyczna, bezpieczna konwersja polskich znaków (ą, ć, ę, ł, ń, ó, ś, ź, ż).

### ⚙️ 4. Plik konfiguracyjny (`~/.config/pifm/pifm.conf`)
Możesz zapisać stałe preferencje w pliku konfiguracyjnym:
```ini
freq = 107.9
station = ENCHNTED
rt = Radio PiFmRds-Enchanted 2026
stereo = true
preemph = 50
gain = 1.0
```
Po zapisaniu pliku wystarczy uruchomić:
```bash
sudo pifm sound.wav
```

---

## 🔌 Podłączenie sprzętowe (Raspberry Pi 1, 2, 3, 4, Zero)

W modelach Raspberry Pi z bezpośrednim wyprowadzeniem SoC antenę testową (kawałek przewodu o długości ok. 70 cm) podłącza się do **GPIO 4**:

```text
Raspberry Pi 40-Pin Header:
  Pin 1 (3.3V)        [ ] [ ]  Pin 2 (5V)
  Pin 3 (SDA)         [ ] [ ]  Pin 4 (5V)
  Pin 5 (SCL)         [ ] [ ]  Pin 6 (GND)
  Pin 7 (GPIO 4 / RF) [X] [ ]  Pin 8 (TXD)  <--- WYJŚCIE RF (GPCLK0)
  Pin 9 (GND)         [X] [ ]  Pin 10 (RXD) <--- MASA / EKRAN FILTRU
```

> [!CAUTION]
> **Ostrzeżenie prawne i filtr dolnoprzepustowy (LPF):**
> Nadawanie w eterze bez licencji radiowej jest nielegalne. Fala prostokątna generowana przez GPIO posiada silne harmoniczne (zwłaszcza 3. harmoniczną na częstotliwościach ~300 MHz). **Bezwzględnie stosuj laboratoryjny filtr dolnoprzepustowy (LPF)**.
> Szczegółowy schemat i opis elementów znajdziesz w: [doc/lpf_filter.md](doc/lpf_filter.md).

---

## 🛠️ Kompilacja i instalacja

### 1. Wymagania systemowe
Na systemach Raspberry Pi OS / Debian / Ubuntu:
```bash
sudo apt update
sudo apt install -y build-essential cmake
```

### 2. Kompilacja i testy
```bash
git clone https://github.com/deloskiytbackup/PiFmRds-Enchanted.git
cd PiFmRds-Enchanted
make
make test
sudo make install
```

### 3. Instalacja z gotowego pakietu `.deb`
```bash
cpack -G DEB
sudo dpkg -i pifmrds-enchanted_2.0.0_arm64.deb
```

---

## 💡 Więcej przykładów użycia

### 1. Transmisja strumieniowa ze stacji internetowej lub PipeWire / ffmpeg
```bash
ffmpeg -i "https://stream.radio.pl/live.mp3" -f s16le -ar 44100 -ac 2 - \
  | sudo pifm - 107.9 --station "LIVE-WEB"
```

### 2. Płynnie przewijany długi tekst stacji (Dynamic PS Scroll)
```bash
sudo pifm sound.wav 107.9 \
  --dynamic-ps "NAJLEPSZE HITY LAT 80 I 90 TYLKO W RADIU ENCHANTED" \
  --ps-mode scroll --ps-rate 1000
```

### 3. Generowanie kompozytowego pliku MPX WAV (dla nadajników sprzętowych)
```bash
pifm sound.wav -o broadcast_mpx.wav --station "ENCHNTED"
```

### 4. Uruchomienie jako usługa systemowa w tle
Włącz automatyczny start przy uruchomieniu systemu:
```bash
sudo systemctl enable --now pifm-enchanted.service
```

---

## 📚 Dokumentacja techniczna

- [Kompatybilność układów SoC (Pi 1..5, RP1, PCIe)](doc/hardware_compat.md)
- [Schemat filtru dolnoprzepustowego 108 MHz (LPF)](doc/lpf_filter.md)

---

## 📜 Licencja

Projekt udostępniony jest na licencji **GNU General Public License v3 (GPLv3)**.  
Oryginalny kod: © 2012–2015 Christophe Jacquet, Richard Hirst, Oliver Mattos.  
Wydanie Enchanted: © 2026 Marcel Siepielski.
