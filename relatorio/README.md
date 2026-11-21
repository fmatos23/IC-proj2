# Estrutura dos relatórios

Plano dos dois relatórios pedidos na Parte IV do enunciado. Serve para acompanhar o que já está feito e o que falta medir.

| Relatório | Limite | Conteúdo |
|---|---|---|
| **Relatório 1**: descrição técnica | 12 páginas, sem código | decisões, métodos, resultados, manual |
| **Relatório 2**: uso de ferramentas de IA | 6 páginas | que ferramentas, para quê, avaliação crítica |

Prazos: entrega a **17 Dez 2026**, discussão a **21 Dez 2026**.

**Estado:** ✅ feito · 🟡 em curso · ⬜ por fazer

---

## Relatório 1: descrição técnica (≤ 12 páginas)

A coluna "Págs." é uma distribuição indicativa para não passar o limite.

| # | Secção | Págs. | Estado |
|---|---|---|---|
| 1 | Introdução | 0.5 | ⬜ |
| 2 | Ferramentas base (Parte I) | 2 | 🟡 |
| 3 | Codec sem perdas (Parte II) | 3.5 | ⬜ |
| 4 | Codec com perdas (Parte III) | 3.5 | ⬜ |
| 5 | Conclusões | 0.5 | ⬜ |
| 6 | Manual de utilização | 1 | 🟡 |
| — | Referências | 0.5 | ⬜ |

### 1. Introdução
- Objetivos do trabalho e organização do relatório.
- **Escolha da linguagem:** C++17 para tudo o que é computacionalmente pesado, porque tempo e memória contam para a avaliação. Python/matplotlib apenas para gráficos.
- Conjunto de teste: 7 imagens PGM de 8 bpp, 13 PPM de 24 bpp e 4 RAW de 16 bpp (astronomia/medicina). Indicar a origem e as dimensões.

### 2. Ferramentas base (Parte I)

**2.1 Leitura e escrita de imagens**
- Um único módulo para RAW, PGM (P2/P5) e PPM (P3/P6), com 8 e 16 bpp. Os RAW de 16 bpp podem ser big ou little-endian.
- Em memória, as amostras ficam sempre em `uint16_t`, intercaladas por canal. O mesmo código serve para todas as profundidades.
- Este módulo é reutilizado pelos codecs das Partes II e III.

**2.2 `img_hist`: histogramas e entropia**
- Definições: entropia de 1.ª ordem H(X) = −Σ p log₂ p. Diferenças horizontais x(r,c) − x(r,c−1) e verticais x(r,c) − x(r−1,c). A primeira linha e a primeira coluna não entram nas diferenças.
- Intervalo de bits: o valor considerado é (x >> lo) & (2^(hi−lo+1) − 1).
- Verificação: os resultados coincidem com um cálculo independente em numpy.

**Resultados a incluir (já medidos):**

Entropia em bits/amostra: H(x) dos valores, H(dₕ) das diferenças horizontais, H(dᵥ) das verticais.

| Imagem (PGM 8 bpp) | H(x) | H(dₕ) | H(dᵥ) |
|---|---|---|---|
| barbara | 7.466 | 6.025 | 5.340 |
| bird | 6.729 | 4.630 | 4.328 |
| dewey_defeats_truman | 7.246 | 4.913 | 4.873 |
| lena | 7.445 | 5.049 | 4.663 |
| sample-1280x853 | 7.810 | 5.840 | 5.970 |
| sample-1920x1280 | 6.663 | 4.368 | 4.471 |
| us000029.7 | 3.040 | 2.574 | 2.795 |

| Imagem (PPM) | H(x) R / G / B | H(dₕ) R / G / B | H(dᵥ) R / G / B |
|---|---|---|---|
| airplane | 6.72 / 6.80 / 6.21 | 4.69 / 4.77 / 4.57 | 4.73 / 4.80 / 4.56 |
| anemone | 7.76 / 7.68 / 7.57 | 5.53 / 5.57 / 5.55 | 5.59 / 5.65 / 5.62 |
| arial | 6.69 / 6.86 / 6.74 | 5.96 / 5.93 / 5.93 | 6.15 / 6.13 / 6.12 |
| baboon | 7.71 / 7.47 / 7.75 | 6.37 / 6.55 / 6.63 | 6.67 / 6.88 / 6.80 |
| bike3 | 6.84 / 6.71 / 6.98 | 5.32 / 4.88 / 4.94 | 5.31 / 4.89 / 4.97 |
| boat | 7.31 / 7.64 / 7.21 | 5.59 / 5.98 / 5.79 | 5.62 / 6.02 / 5.78 |
| girl | 4.34 / 6.66 / 6.43 | 3.58 / 5.26 / 4.94 | 3.39 / 5.02 / 4.73 |
| house | 6.43 / 6.54 / 6.23 | 4.56 / 4.65 / 4.85 | 5.21 / 5.06 / 5.00 |
| lena | 7.25 / 7.59 / 6.97 | 4.90 / 5.33 / 5.41 | 4.48 / 5.00 / 5.13 |
| monarch | 7.40 / 7.08 / 6.98 | 4.51 / 4.54 / 4.63 | 4.55 / 4.56 / 4.66 |
| peppers | 7.34 / 7.50 / 7.06 | 5.42 / 5.31 / 5.27 | 5.40 / 5.22 / 5.15 |
| tulips | 7.15 / 7.66 / 7.59 | 4.80 / 5.23 / 5.31 | 4.68 / 5.02 / 5.14 |

| Imagem (RAW 16 bpp) | linhas × colunas | H(x) | H(dₕ) | H(dᵥ) |
|---|---|---|---|---|
| ERIS.2024-01-01 | 2046 × 2048 | 12.829 | 12.608 | 12.551 |
| int20040906 | 4200 × 2154 | 7.280 | 6.685 | 6.673 |
| jkt19980403 | 1124 × 1124 | 5.985 | 5.496 | 5.481 |
| wht19951018 | 1124 × 1124 | 4.570 | 4.724 | 4.718 |

**Pontos a discutir:**
- Nas imagens naturais, as diferenças baixam a entropia 1.5 a 2.5 bits. É a motivação direta para a codificação preditiva da Parte II.
- Nas imagens de 16 bpp o ganho é pequeno. Em `wht…` as diferenças têm até **mais** entropia que os valores: imagem com pouco conteúdo e/ou ruído.
- O ERIS tem ~12.8 bits de entropia: ruído dominante nos bits baixos, logo é difícil de comprimir sem perdas.
- Os canais R, G e B têm entropias e estrutura semelhantes. Há correlação entre canais, o que motiva uma transformada de cor.
- Análise por planos de bits (`-bits`): os bits menos significativos são quase aleatórios (entropia ≈ número de bits) e os mais significativos têm estrutura. Incluir 1 ou 2 figuras, por exemplo lena com bits 0–3 vs 4–7.
- Figuras: histograma de uma PGM e de uma PPM (gerados pelo `img_hist`).

**2.3 `img_quant`: quantização escalar uniforme**
- Índice q = ⌊x·L/(M+1)⌋ com L = 2ᵇ níveis. Reconstrução no ponto médio do intervalo (por omissão), no limite inferior (`-floor`) ou escrita dos índices (`-index`).
- Justificar o ponto médio: minimiza o erro máximo (Δ/2) e o MSE para dados uniformes dentro do intervalo.

**2.4 `img_cmp`: erro máximo e PSNR**
- PSNR = 10·log₁₀(pico²/MSE), em que o pico é o maxval do original (`-peak` para mudar).

**Resultados a incluir (já medidos), PSNR (dB) vs bits por amostra:**

| bits | erro máx. | lena.ppm | barbara.pgm | teórico ≈ 6.02·b + 10.8 |
|---|---|---|---|---|
| 7 | 1 | 51.21 | 51.14 | 52.9 |
| 6 | 2 | 46.35 | 46.36 | 46.9 |
| 5 | 4 | 40.69 | 40.72 | 40.9 |
| 4 | 8 | 34.69 | 34.68 | 34.9 |
| 3 | 16 | 28.65 | 28.93 | 28.9 |
| 2 | 32 | 22.94 | 22.90 | 22.8 |
| 1 | 64 | 17.27 | 16.70 | 16.8 |

- Discutir a regra dos ~6 dB/bit. Com poucos bits removidos o erro é inteiro (0 ou 1 para b=7), por isso a aproximação Δ²/12 é menos exata.
- Figura: lena com 8, 4, 2 e 1 bits, para mostrar a posterização.

### 3. Codec sem perdas (Parte II)
- **3.1 Arquitetura:** diagrama de blocos (transformada de cor → predição → modelação de contexto → codificação entrópica) e formato do ficheiro comprimido (cabeçalho, metadados para reproduzir o ficheiro original byte a byte).
- **3.2 Predição:** preditores avaliados (diferença simples, JPEG-LS/MED, CALIC/GAP, adaptativos...), com a entropia residual de cada um.
- **3.3 Transformada de cor:** para PPM, comparar RGB, diferenças entre canais e YCoCg-R reversível.
- **3.4 Codificação entrópica e contextos:** Golomb-Rice vs aritmética adaptativa, número de contextos, mistura de modelos.
- **3.5 Imagens de 16 bpp:** particularidades do RAW (byte order, gama efetiva, ruído).
- **3.6 Resultados:** bpp médio por conjunto de imagens, tempo de codificação/descodificação e memória máxima.
- **3.7 Comparação:** com JPEG-LS, JPEG-XL (modo sem perdas), PNG/OptiPNG, zstd, xz/lzma, bzip2. Tabela de bpp, tempo e memória e gráfico bpp vs tempo.
  - 🟡 Referências já medidas com `scripts/benchmark_sota.py` (`results/`). Repetir na máquina final para os tempos do relatório.
  - As imagens Kodak (`*.256.ppm`) têm só 256 cores (paleta): JPEG-LS fica em ~12.8 bpp e bzip2/PNG/JPEG-XL e7 em ~4 bpp. O codec precisa de detetar imagens com poucas cores.
- Metodologia de medição: `/usr/bin/time -v` para tempo e memória (max RSS), média de várias execuções, máquina usada.

### 4. Codec com perdas (Parte III)
- **4.1 Arquitetura e formato:** diagrama de blocos e significado do parâmetro `-q 1..100`.
- **4.2 Decisões de desenho, cada uma justificada com experiências:** transformada (DCT por blocos, wavelet, ou predição com quantização near-lossless), subamostragem de croma, quantização (passo, zona morta), codificação entrópica.
- **4.3 Curvas taxa-distorção:** PSNR vs bpp para várias imagens.
- **4.4 Comparação:** com JPEG, JPEG 2000, JPEG-XL (e WebP/AVIF, se houver tempo). Curvas taxa-distorção sobrepostas, tempo e memória.

### 5. Conclusões
- Resumo dos resultados, limitações e trabalho futuro.

### 6. Manual de utilização
- Compilação (`make`), dependências e chamada de cada programa com exemplos. Pode ser uma versão resumida do [README principal](../README.md).

---

## Relatório 2: uso de ferramentas de IA (≤ 6 páginas)

Para cada utilização relevante, responder às perguntas do enunciado: *Qual era o problema? O que propôs a IA? Como foi verificado? Estava correto? O que foi alterado? O que se aprendeu?*

| # | Secção | Estado |
|---|---|---|
| 1 | Ferramentas usadas (ex.: Claude Code) e para quê | 🟡 |
| 2 | Casos de uso, um por cada parte do trabalho | 🟡 |
| 3 | Avaliação crítica: onde ajudou, onde falhou, riscos | ⬜ |
| 4 | Conclusão: o que se aprendeu sobre usar IA | ⬜ |

**Registo já disponível (Parte I):**
- **Problema:** implementar `img_hist`, `img_quant` e `img_cmp` com suporte para RAW/PGM/PPM, 8 e 16 bpp, BE e LE.
- **Proposta da IA:** C++17 com uma biblioteca comum de I/O, gráficos em Python/matplotlib, Makefile e script de testes.
- **Verificação:**
  - entropias comparadas com numpy;
  - PSNR comparado com a fórmula teórica;
  - leituras e escritas confirmadas como idênticas byte a byte, incluindo um RAW convertido para little-endian com `dd conv=swab`.
- **Erros encontrados:**
  - os primeiros gráficos tinham eixos com caudas vazias e notação "1e4" sobreposta ao título;
  - a IA afirmou que `img_quant -index` convertia RAW em PGM, o que era falso: o formato de saída é sempre o da entrada;
  - as dimensões das imagens RAW foram inicialmente trocadas num teste. A orientação certa foi confirmada pela entropia das diferenças, que é menor na orientação correta.
- **Lição:** confirmar sempre as afirmações da IA executando o código, sem confiar na explicação.

> Dica: ir acrescentando aqui uma entrada por cada sessão de trabalho com IA. No fim, o relatório 2 escreve-se quase sozinho.
