# Bezpieczeństwo RF i Filtr Dolnoprzepustowy (LPF)

> [!CAUTION]
> **Ostrzeżenie prawne:**
> Nadawanie w paśmie radiowym UKF (87.5 – 108.0 MHz) bez zezwolenia radiowego jest nielegalne w większości krajów na świecie. Niniejsze oprogramowanie powstało wyłącznie w celach edukacyjnych i badawczych w warunkach laboratoryjnych (np. w klatce Faradaya lub z bezpośrednim połączeniem kablem koncentrycznym przez tłumik sygnału do odbiornika).

---

## 1. Problem harmonicznych fali prostokątnej

Sygnał generowany przez pin GPIO 4 Raspberry Pi jest **falą prostokątną**. Zgodnie z analizą Fouriera, fala prostokątna składa się z częstotliwości podstawowej oraz nieparzystych harmonicznych:
- **Częstotliwość nośna (1. harmoniczna):** np. 100 MHz (pasmo radiofoniczne FM)
- **3. harmoniczna:** 300 MHz (pasmo wojskowe / lotnicze UHF)
- **5. harmoniczna:** 500 MHz (pasmo telewizji cyfrowej DVB-T2 / LTE)
- **7. harmoniczna:** 700 MHz (pasmo 5G / sieci komórkowe)

Bez zastosowania filtru dolnoprzepustowego energia emitowana na harmonicznych może zakłócać pasma krytyczne, w szczególności **lotnictwo cywilne (Airband 118–137 MHz)**!

---

## 2. Schemat 7-biegunowego filtru dolnoprzepustowego (LPF 108 MHz)

Do wytłumienia harmonicznych zaleca się zbudowanie prostego filtru Czebyszewa lub Butterwortha 7-go rzędu o impedancji falowej 50 $\Omega$ i częstotliwości odcięcia ok. 108 MHz.

### Schemat połączeń:

```text
GPIO 4 o───[ L1 ]───┬───[ L2 ]───┬───[ L3 ]───o Wyjście (Antena/Tłumik)
                    │            │            
                   [C1]         [C2]          
                    │            │            
GND    o────────────┴────────────┴────────────o GND
```

### Wartości elementów (dla $f_c \approx 108\text{ MHz}$, $Z_0 = 50\ \Omega$):
- **L1:** 56 nH (ok. 4 zwoje drutu emaliowanego 0.5 mm nawinięte na średnicy 4 mm)
- **L2:** 100 nH (ok. 6 zwojów na średnicy 4 mm)
- **L3:** 56 nH (ok. 4 zwoje na średnicy 4 mm)
- **C1:** 39 pF (kondensator ceramiczny NP0/C0G)
- **C2:** 39 pF (kondensator ceramiczny NP0/C0G)

### Tłumienie:
- Tłumienie w paśmie przepustowym (87.5–108 MHz): $< 0.8\text{ dB}$
- Tłumienie 3. harmonicznej (260–320 MHz): $> 45\text{ dB}$
- Tłumienie pasma lotniczego (118–137 MHz): $> 25\text{ dB}$
