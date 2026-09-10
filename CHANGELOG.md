# 📜 Changelog - PiFmRds-Enchanted

Wszystkie istotne zmiany i wydania projektu **PiFmRds-Enchanted** są dokumentowane w tym pliku.  
Format jest oparty na [Keep a Changelog](https://keepachangelog.com/pl/1.0.0/) oraz zgodny z [Semantic Versioning](https://semver.org/).

---

## [2.1.0] - 2026-09-11

### ✨ Dodano (Added)
- **Komenda `ver` / `version`**: Nowe polecenie CLI (`pifm ver`, `pifm version`, `pifm ctl ver`, `pi_fm_rds ver`) wyświetlające szczegółowe informacje o wersji, dacie kompilacji, architekturze, kompilatorze oraz aktywnych modułach silnika audio i modulacji.
- **Wsparcie dla formatów studyjnych Hi-Fi (24-bit i 32-bit PCM)**:
  - Bezpośredni odczyt 24-bitowego PCM (3 bajty little-endian) używanego w plikach studyjnych i ripach winylowych o wysokiej rozdzielczości (24-bit 96 kHz / 192 kHz) ze skalowaniem bit-accurate do zakresu `[-1.0, 1.0]`.
  - Obsługa 32-bitowego całkowitoliczbowego PCM (int32) oraz 32-bitowego IEEE Float.
  - Obsługa 8-bitowego PCM unsigned.
- **Audiofilski resampler Catmull-Rom Cubic Spline**:
  - Zastąpiono prosty resampler liniowy 4-punktowym filtrem kubicznym **Hermite / Catmull-Rom Spline** ($C^1$-continuous).
  - Precyzyjne odwzorowanie transjentów i krystaliczna czystość pasma przy resamplingu z 48 kHz, 88.2 kHz, 96 kHz czy 192 kHz do 228 kHz MPX.
- **Transparentny auto-dekoder formatów audio**:
  - Bezpośrednie odtwarzanie plików `.flac`, `.mp3`, `.m4a`, `.ogg`, `.opus`, `.aac` i `.wma` bez konieczności ich wcześniejszej ręcznej konwersji do WAV (automatyczny potok `ffmpeg` w tle).
  - Prawidłowe zamykanie procesów potomnych i potoków (`pclose`).
- **Test jednostkowy `test_hifi_wav`**:
  - Syntetyzuje plik 24-bit 96 kHz PCM i weryfikuje bit-perfect odczyt oraz skalowanie próbek.
- **Rozszerzona normalizacja znaków międzynarodowych**:
  - Dodano obsługę znaków języków hiszpańskiego, czeskiego, słowackiego, francuskiego i niemieckiego (`Ñ, Š, Č, Ř, Ž, Ů, ñ` itd.).

### 🚀 Zoptymalizowano (Optimized)
- **Zero-copy przewijanie w Dynamic PS**: Wstępne formatowanie bufora pierścieniowego w `rds_ps_paginator_set_text` eliminujące cykliczne wywołania `snprintf()` i alokacje na stosie.
- **Trygonometria SDR I/Q**: Zastąpiono podwójną precyzję `cos()` i `sin()` szybkimi operacjami `cosf()` i `sinf()` w pętli 2 000 000 próbek/s.
- **Odporny parser bloków RIFF WAV**: Bezpieczne ignorowanie niestandardowych metadanych `LIST`, `JUNK`, `BEXT`, `ID3` i lokalizacja właściwego bloku `data`.
- **Kolejka pierścieniowa IPC**: Obsługa wieloliniowych serii poleceń oddzielonych `\n` w `control_poll()` bez gubienia żadnego pakietu.

### 🛡️ Bezpieczeństwo (Security & Robustness)
- Dodano pełne bariery pamięciowe `__sync_synchronize()` na wielordzeniowych platformach ARM (Pi 2/3/4) chroniące spójność pamięci pomiędzy CPU a DMA.
- Bezpieczny stan pinu GPIO 4 przy zamykaniu (`rpi_hw_shutdown`) – natychmiastowe przestawienie w stan wysokiej impedancji (`INPUT`), czyste wygaszenie nośnej RF.
- Odmapowywanie peryferiów MMIO (`unmapmem`) przy zamykaniu procesu.

---

## [2.0.0] - 2026-09-10

### 🚀 Duża Modernizacja (Major Overhaul)
- **Nowa, intuicyjna składnia poleceń**:
  - Automatyczne rozpoznawanie pliku i częstotliwości: `pifm sound.wav 107.9` lub `pifm 107.9 sound.wav`.
  - Wbudowane podpolecenia sterowania w locie `pifm ctl [track|ps|dynamic|rt|ta|stop]`.
- **Broadcast Audio DSP**:
  - 4-biegunowy filtr dolnoprzepustowy Butterwortha 15 kHz (Brickwall LPF) chroniący pilota stereo 19 kHz.
  - Broadcast AGC (Automatyczna Kontrola Wzmocnienia) oraz szybki limiter algebraiczny chroniący przed przekroczeniem 75 kHz dewiacji.
  - Ochrona przed wartościami zdenormalizowanymi (anti-denormal noise `1e-18`).
- **Tablicowy generator MPX**:
  - Zastąpienie wywołań funkcji `sin()` stabelaryzowanymi nośnymi fazowymi dla 19 kHz i 38 kHz, oszczędzając ponad 70% CPU.
- **Pełny standard RDS / RBDS (Wydanie 2026)**:
  - Grupy 0A (PS / AF - alternatywne częstotliwości).
  - Grupy 2A (RadioText 64 znaki).
  - Grupy 3A i 11A (RadioText Plus - RT+ z polami Tytuł i Wykonawca).
  - Grupa 4A (Zegar radiowy CT z obliczaniem zmodyfikowanego dnia juliańskiego MJD i strefy UTC).
  - Dynamic PS: stronicowanie wyrazowe oraz płynne przewijanie.
- **Wsparcie dla Raspberry Pi 5 i tryb SDR I/Q**:
  - Dynamiczne wykrywanie platformy w runtime.
  - Precyzyjna synteza kwadraturowa I/Q (`s8`, `u8`, `s16`, `f32`) ze strumieniowaniem do stdout/pliku dla HackRF One, LimeSDR, FL2k i komputerów PC/Mac.
- **Nowoczesna architektura C17 i CMake**:
  - Usunięcie zewnętrznych zależności audio (`libsndfile`), samodzielny czytnik/zapisywacz WAV.
  - Integracja CMake, CTest (4 automatyczne testy jednostkowe) oraz CPack dla pakietów Debian `.deb` i tarballi `.tar.gz`.
  - Usługa systemd (`pifm-enchanted.service`) i plik konfiguracyjny `~/.config/pifm/pifm.conf`.

---

## [1.0.0] - 2014-04-10

### 📦 Wersja Pierwotna (Legacy PiFmRds)
- Oryginalne wydanie autorstwa Christophe Jacquet (F8FTK), Richarda Hirsta i Olivera Mattosa.
- Bezpośrednia modulacja GPCLK0 na GPIO 4 dla procesora BCM2835 (Raspberry Pi 1).
- Prosty koder RDS (Grupa 0A i 2A).
- Tradycyjna składnia flagowa (`-f`, `-a`, `-p`, `-r`).
- Zależność od biblioteki `libsndfile` i starego systemu budowania GNU Make.
