# Kompatybilność Sprzętowa PiFmRds-Enchanted (Stan na 2026 r.)

W projekcie **PiFmRds-Enchanted** całkowicie wyeliminowano potrzebę rekompilacji kodu pod konkretny model Raspberry Pi. Detekcja platformy, adresów fizycznych pamięci peryferiów oraz częstotliwości taktowania PLL odbywa się **dynamicznie w czasie uruchomienia** (runtime detection).

---

## 1. Macierz wsparcia modeli Raspberry Pi

| Model Raspberry Pi | SoC | Architektura | Bezpośrednie RF (GPIO 4) | Baza Peryferiów | PLL Zegara |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Pi 1 Model A, B, A+, B+** | BCM2835 | ARMv6 (32-bit) | ✅ **Wspierane** | `0x20000000` | 500 MHz |
| **Pi Zero, Zero W, Zero WH**| BCM2835 | ARMv6 (32-bit) | ✅ **Wspierane** | `0x20000000` | 500 MHz |
| **Pi 2 Model B (v1.1)** | BCM2836 | ARMv7 (32-bit) | ✅ **Wspierane** | `0x3F000000` | 500 MHz |
| **Pi 2 Model B (v1.2)** | BCM2837 | ARMv8 (64-bit) | ✅ **Wspierane** | `0x3F000000` | 500 MHz |
| **Pi 3 Model B, B+** | BCM2837 | ARMv8 (64-bit) | ✅ **Wspierane** | `0x3F000000` | 500 MHz |
| **Pi Zero 2 W** | BCM2837 | ARMv8 (64-bit) | ✅ **Wspierane** | `0x3F000000` | 500 MHz |
| **Pi 4 Model B, 400, CM4** | BCM2711 | ARMv8 (64-bit) | ✅ **Wspierane** | `0xFE000000` | 750 MHz |
| **Pi 5, CM5, Pi 500** | BCM2712 | ARMv8 (64-bit) | ⚠️ **Wymaga SDR / MPX** | *Brak GPCLK0 na GPIO* | N/A |
| **Komputery PC / Mac / VM** | Dowolny | x86_64 / arm64 | ⚠️ **Tryb MPX / WAV / SDR**| N/A | N/A |

---

## 2. Dlaczego Raspberry Pi 5 nie wspiera bezpośredniego nadawania z pinu GPIO 4?

W modelach Pi 1–4 piny 40-pinowego złącza GPIO były bezpośrednio podłączone do głównego układu SoC (Broadcom BCM283x / BCM2711). Umożliwiało to bezpośrednie mapowanie generatora zegara `GPCLK0` oraz kontrolera DMA w przestrzeni adresowej procesora i generowanie fali prostokątnej o częstotliwości FM (~100 MHz).

W **Raspberry Pi 5 (BCM2712)** zaszła kluczowa zmiana architektoniczna:
1. Wszystkie linie GPIO zostały przeniesione do dedykowanego mostka południowego (southbridge) o nazwie **RP1**.
2. Układ RP1 komunikuje się z procesorem BCM2712 za pośrednictwem magistrali **PCIe 2.0 x4**.
3. Magistrala PCIe wprowadza opóźnienia pakietowe oraz fluktuacje czasowe (jitter), co uniemożliwia bezpośrednią, nanosekundową syntezę fali nośnej radiowej na pinach GPIO.

### Jak używać PiFmRds-Enchanted na Raspberry Pi 5 i PC?
PiFmRds-Enchanted posiada uniwersalny tryb **Universal Baseband MPX & SDR Output**:
- Program potrafi wygenerować pełny, zmodulowany sygnał stereofoniczny MPX z podnośną 19 kHz, 38 kHz oraz 57 kHz RDS w standardzie 228 kHz WAV lub przekazywać go rurociągiem (pipe) do nadajników SDR, np. **HackRF One**, **LimeSDR**, czy przetworników **FL2k VGA**.
- Przykład użycia:
  ```bash
  pi_fm_rds -a audio.wav -o broadcast_mpx.wav --ps "ENCHNTED" --title "Hit" --artist "Band"
  ```

---

## 3. Konfiguracja jądra Linux 6.x na Raspberry Pi OS (Bookworm / Trixie)

Nowoczesne jądra Linuksa (6.x+) domyślnie włączają ochronę pamięci `CONFIG_STRICT_DEVMEM`. Jeśli program zgłasza błąd dostępu do `/dev/mem`:
1. Upewnij się, że uruchamiasz program z uprawnieniami administratora (`sudo`).
2. W pliku `/boot/firmware/cmdline.txt` (lub `/boot/cmdline.txt`) dopisz na końcu linii parametr:
   ```text
   iomem=relaxed
   ```
3. Zrestartuj urządzenie: `sudo reboot`.
