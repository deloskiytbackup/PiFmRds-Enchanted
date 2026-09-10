<div align="center">

# 📻 PiFmRds-Enchanted
### Profesjonalny nadajnik FM/RDS nowej generacji dla Raspberry Pi, SDR i PC (Wydanie 2026)

[![Release](https://img.shields.io/github/v/release/deloskiytbackup/PiFmRds-Enchanted?color=brightgreen&label=Wydanie%20v2.1.0)](https://github.com/deloskiytbackup/PiFmRds-Enchanted/releases)
[![CI & Packaging](https://img.shields.io/github/actions/workflow/status/deloskiytbackup/PiFmRds-Enchanted/ci.yml?branch=master&label=Testy%20%26%20Build)](https://github.com/deloskiytbackup/PiFmRds-Enchanted/actions)
[![License: GPL v3](https://img.shields.io/badge/Licencja-GPLv3-blue.svg)](LICENSE)
[![Standard: C17](https://img.shields.io/badge/Standard-C17-orange.svg)]()
[![Platform](https://img.shields.io/badge/Platforma-Raspberry%20Pi%201--4%20%7C%20Pi%205%20%7C%20PC%20%7C%20SDR-purple.svg)]()
[![Hi-Fi Audio](https://img.shields.io/badge/Audio-24--bit%2096k%20%7C%20FLAC%20%7C%20Catmull--Rom-gold.svg)]()

**PiFmRds-Enchanted** to całkowicie zmodernizowana, bezkompromisowa wersja kultowego projektu PiFmRds.  
Wprowadza **wsparcie dla formatów Hi-Fi (24-bit/32-bit PCM, FLAC, MP3)**, audiofilski **resampler Catmull-Rom Cubic Spline**, broadcastowy **procesor dźwięku DSP** (15 kHz brickwall LPF + AGC limiter), bezpośrednią modulację **SDR I/Q** (HackRF, LimeSDR, FL2k, Raspberry Pi 5, PC/Mac), pełny standard **RDS 2026** (RT+, Dynamic PS, zegar CT) oraz intuicyjną składnię poleceń bez archaicznych flag.

[📜 Zobacz pełny Dziennik Zmian (CHANGELOG.md)](CHANGELOG.md)

</div>

---

## ⚡ Szybki Start (Nowa, intuicyjna składnia)

Zapomnij o pamiętaniu skomplikowanych parametrów z 2012 roku (`-f`, `-a`, `--ps`, `--rt`):

```bash
# 1. Po prostu wskaż plik muzyczny (WAV, FLAC, MP3 itp. - domyślnie 107.9 MHz):
sudo pifm "album_master_24bit.flac"

# 2. Wskaż plik i częstotliwość w dowolnej kolejności:
sudo pifm song.mp3 98.5
sudo pifm 101.2 "utwór.wav" --station "HITS"

# 3. Sprawdź wersję oprogramowania i aktywne moduły:
pifm ver

# 4. Zmiana utworu w RDS (RT+) w locie z drugiego terminala:
pifm ctl track "Blinding Lights" "The Weeknd"

# 5. Włączenie komunikatu drogowego TA:
pifm ctl ta on
```

---

## 🔍 Informacje o Wersji (`pifm ver`)

Wbudowana komenda `ver` wyświetla szczegółowy status silnika audio i modulacji:

```text
$ pifm ver
============================================================
📻 PiFmRds-Enchanted (Wersja 2026)
   Wersja:        2.1.0-enchanted-2026
   Kompilacja:    Sep 11 2026 00:09:37
   Kompilator:    Clang 21.0.0 / GCC
   Architektura:  64-bit
------------------------------------------------------------
Silnik Audio i Modulacji:
   [✓] Formaty Hi-Fi:     24-bit PCM, 32-bit int/float, 16-bit PCM, 8-bit
   [✓] Resampling:        4-punktowy Splajn Kubiczny Catmull-Rom (48k/96k/192k)
   [✓] Auto-dekoder:      FLAC, MP3, AAC, M4A, OGG, Opus (w locie bez konwersji)
   [✓] Broadcast DSP:     15 kHz Butterworth Brickwall LPF + AGC Soft Limiter
   [✓] RDS / RBDS:        Grupy 0A (PS/AF), 2A (RT), 3A (ODA), 11A (RT+ Title/Artist)
                          Grupa 4A (CT zegar MJD), Dynamic PS (Paging & Smooth Scroll)
   [✓] SDR I/Q Mode:      HackRF, LimeSDR, FL2k, Raspberry Pi 5, PC/Mac
   [✓] Sterowanie IPC:    Nieblokujący UNIX Domain Datagram Socket + FIFO
------------------------------------------------------------
Autorzy pierwotni (2012): Christophe Jacquet, Richard Hirst, Oliver Mattos
Edycja Enchanted (2026):  Marcel Siepielski & Współtwórcy
GitHub: https://github.com/deloskiytbackup/PiFmRds-Enchanted
============================================================
```

---

## 📊 Porównanie: Oryginalny PiFmRds vs PiFmRds-Enchanted (2026)

| Funkcja | Oryginalny PiFmRds (2014) | **PiFmRds-Enchanted (2026)** |
| :--- | :---: | :---: |
| **Wersja projektu** | v1.0.0 (archiwalna) | **v2.1.0-enchanted-2026** |
| **Składnia CLI** | Skomplikowana (`-f`, `-a`, `--ps`, `--rt`) | **Intuicyjna (`pifm sound.wav 107.9`)** |
| **Sterowanie w locie** | Ręczne pisanie do FIFO pipe | **Wbudowane `pifm ctl [track/ps/ta/stop]`** |
| **Formaty Hi-Fi (24-bit / 32-bit)** | ❌ Tylko 16-bit PCM | ✅ **Natywne 24-bit, 32-bit int, 32-bit float** |
| **Bezpośredni FLAC / MP3 / OGG** | ❌ Wymagała ręcznej konwersji do WAV | ✅ **Automatyczne dekodowanie w locie** |
| **Jakość resamplingu (48k..192k)** | Zwykła interpolacja liniowa (aliasing) | ✅ **Audiofilski Catmull-Rom Cubic Spline** |
| **Wsparcie dla Raspberry Pi 5** | ❌ Brak (niekompatybilne z RP1) | ✅ **Pełne (przez bezpośredni tryb SDR I/Q)** |
| **Kompatybilność PC / Mac / VM** | ❌ Tylko stary Raspberry Pi | ✅ **Działa wszędzie (tryb MPX WAV i SDR)** |
| **RadioText Plus (RT+)** | ❌ Brak | ✅ **Pełne (Tytuł utworu & Wykonawca)** |
| **Zegar radiowy (CT Group 4A)** | ⚠️ Uproszczony | ✅ **Pełna synchronizacja z czasem NTP (MJD)** |
| **Dynamiczny PS** | ❌ Ucina słowa w połowie | ✅ **Inteligentny podział słów i płynny scroll** |
| **Filtr audio 15 kHz (Brickwall)** | ❌ Brak (ptaszkowanie stereo) | ✅ **4-biegunowy filtr Butterwortha 15 kHz** |
| **Procesor dynamiki (AGC / Limiter)**| ❌ Brak (ryzyko przesterowania) | ✅ **Broadcast AGC + limiter dewiacji 75 kHz** |
| **Zużycie CPU (generowanie MPX)** | Wysokie (wywołania `sin()` w pętli) | ⚡ **Zredukowane o >70% (tablice LUT)** |
| **System budowania & Testy** | Przestarzały Makefile | ✅ **Nowoczesny CMake + CTest (100% pass)** |
| **Format paczek** | Brak | ✅ **Gotowe pakiety Debian `.deb` (CPack)** |

---

## 🌟 Główne moduły i możliwości

### 🎧 1. Formaty Audio Hi-Fi i Auto-dekoder
- **Natywne 24-bit PCM & 32-bit PCM/Float**: Obsługa nagrań w studyjnej rozdzielczości 24-bit 96 kHz lub 192 kHz z dokładnością bit-accurate.
- **Bezpośrednie odtwarzanie FLAC, MP3, AAC, M4A, OGG, Opus**: Brak konieczności uprzedniej konwersji plików – silnik w locie dekoduje pliki dźwiękowe.
- **Resampler Catmull-Rom**: 4-punktowa interpolacja kubiczna Hermite'a gwarantuje brak aliasingu i zachowanie dynamiki transjentów.

### 🎚️ 2. Broadcast Audio DSP (Procesor Dźwięku)
- **15 kHz Brickwall Low-Pass Filter**: Stromy filtr dolnoprzepustowy (4. rzędu) chroni podnośną pilota stereofonicznego (19 kHz) przed interferencją z wysokimi częstotliwościami audio.
- **Broadcast AGC (Automatyczna Kontrola Wzmocnienia)**: Wyrównuje poziom głośności pomiędzy różnymi nagraniami, nadając transmisji profesjonalne, głośne brzmienie komercyjnej rozgłośni.
- **Algebraiczny soft-knee limiter**: Szybki ogranicznik nasycenia $x / (1 + x)$ z ochroną przed wartościami zdenormalizowanymi (`1e-18`).

### 📡 3. Bezpośredni silnik SDR I/Q (HackRF, FL2k, LimeSDR)
Umożliwia nadawanie na **Raspberry Pi 5** (które ze względu na układ I/O RP1 po PCIe nie posiada bezpośredniego GPCLK0 na pinach) oraz na zwykłych komputerach PC/Mac:
```bash
# Nadawanie przez HackRF One w czasie rzeczywistym:
pifm "muzyka.flac" --iq-out - | hackrf_transfer -t - -f 107900000 -s 2000000 -a 1 -x 20

# Nadawanie przez tani konwerter VGA FL2k (osmo-fl2k):
pifm sound.wav --iq-format u8 --iq-rate 100000000 --iq-out - | fl2k_file -s 100000000 -
```

### 📻 4. Zaawansowany silnik RDS (IEC 62106 / RBDS)
- **RT+ (RadioText Plus)**: Ekrany nowoczesnych samochodów i radioodbiorników wyświetlają dedykowane pola metadanych: wykonawcę i tytuł piosenki.
- **Dynamiczny PS (Program Service)**: Stronicowanie nazw dłuższych niż 8 znaków z poszanowaniem granic słów lub tryb płynnego przewijania.
- **Zegar CT (Clock Time - Grupa 4A)**: Odbiorniki radiowe w samochodach automatycznie ustawiają zegar i strefę czasową z systemowego NTP.
- **Wsparcie dla PTY, AF i TP/TA**: Wybór profili muzycznych/informacyjnych (rozpoznaje nazwy, np. `--pty rock`, `--pty pop`), alternatywne częstotliwości i priorytet komunikatów drogowych.
- **Normalizacja diakrytyków**: Automatyczna, bezpieczna konwersja znaków polskich, niemieckich, hiszpańskich, czeskich i słowackich.

### ⚙️ 5. Plik konfiguracyjny (`~/.config/pifm/pifm.conf`)
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
sudo pifm "utwór.flac"
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
sudo apt install -y build-essential cmake ffmpeg
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
sudo dpkg -i pifmrds-enchanted_2.1.0_arm64.deb
```

---

## 📚 Dokumentacja techniczna

- [Dziennik zmian wydań (CHANGELOG.md)](CHANGELOG.md)
- [Kompatybilność układów SoC (Pi 1..5, RP1, PCIe)](doc/hardware_compat.md)
- [Schemat filtru dolnoprzepustowego 108 MHz (LPF)](doc/lpf_filter.md)

---

## 📜 Licencja

Projekt udostępniony jest na licencji **GNU General Public License v3 (GPLv3)**.  
Oryginalny kod: © 2012–2015 Christophe Jacquet, Richard Hirst, Oliver Mattos.  
Wydanie Enchanted: © 2026 Marcel Siepielski.
