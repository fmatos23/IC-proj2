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
| 3 | Codec sem perdas (Parte II) | 3.5 | 🟡 |
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
  - 🟡 Etapa 1 (MED + contextos + aritmético): `results/sota_stage1.md`.
  - 🟡 Etapa 2 (cor, paleta, mistura de preditores, contextos): `results/sota_stage2.md`.
  - 🟡 Etapa 3 (NLMS, 2.º andar, empacotamento, velocidade): `results/sota_stage3.md`.
  - 🟡 Ponto 1 (velocidade): perfis `-fast`/`-fastest` e faixas em paralelo `-t n`: `results/sota_point1.md`.
  - 🟡 Ponto 2 (memória): `results/sota_point2.md`.
  - 🟡 Ponto 3 (imagens sintéticas, modelo de correspondências): `results/sota_synthetic.md`.
  - ✅ **Avaliação honesta em imagens nunca usadas na afinação: `results/AVALIACAO.md`** (usar estes números como os principais no relatório; os da cadeira são otimistas).

**Ablação da etapa 2 (bpp médio; PGM 7 imagens, PPM 12 imagens; usar no relatório):**

| Passo | PGM | PPM | Notas |
|---|---|---|---|
| Etapa 1: MED + contextos | 3.876 | 13.976 | ≈ JPEG-LS |
| + transformada de cor (escolha por entropia) + predição entre canais | 3.876 | 13.207 | |
| + modo de paleta | 3.876 | 12.419 | arial 13.95 → 4.49; Kodak 11.9 → ~4.3 |
| + mistura de 9 preditores + correção de viés | 3.772 | 12.118 | |
| + afinação da mistura (peso 1/erro, NW/NE, limite do viés) | 3.736 | 12.029 | |
| + preditores "mesmo gradiente do canal anterior" | 3.735 | 11.988 | |
| + transformada escolhida por codificação de amostra | 3.735 | 11.958 | = melhor transformada por imagem |
| + 2.º contexto misturado (zero e classe) | 3.724 | 11.924 | taxa de aprendizagem baixa é essencial |
| + contadores com taxa adaptativa | 3.711 | 11.884 | |
| + parte fracionária da predição no contexto (sinal, zero) | 3.703 | 11.859 | |
| + 3 bits de mantissa modelados, atividade com 2 níveis/oitava | 3.696 | 11.841 | |
| + reafinação (métrica, memória do peso entre canais) | **3.694** | **11.822** | |

**Ablação da etapa 3 (mesmas imagens):**

| Passo | PGM | PPM | RAW 16 | Notas |
|---|---|---|---|---|
| Fim da etapa 2 | 3.694 | 11.822 | 7.064 | |
| + preditor adaptativo NLMS (8 entradas, passo 1/2) | 3.665 | 11.762 | | o passo grande (adapta-se depressa) é o melhor |
| + 2.º NLMS mais lento | 3.658 | 11.737 | | |
| + 12 entradas (mais vizinhos, 2 canais de referência) | 3.648 | 11.721 | 7.012 | 16 entradas não ajudou |
| + 2.º andar NLMS sobre os resíduos vizinhos | 3.641 | 11.715 | 6.952 | sem o resíduo do canal 0 |
| − 3 preditores simples (MED, (W+NE)/2, 2W−WW): ficam 9 | 3.631 | 11.698 | | menos é melhor e mais rápido |
| − correção de viés (redundante com o 2.º andar) | 3.633 | 11.665 | 6.937 | |
| + dispersão das predições no contexto | 3.631 | 11.656 | | |
| + empacotamento de valores (RAW) | 3.631 | 11.656 | **6.930** | jkt 5.054 → 5.035 |

Resultado da etapa 3: **PGM 3.631 (e9: 3.647), PPM 11.656 (e9: 11.747), Kodak 3.132 (e9: 3.899), RAW 6.930 (e9: 6.924)**; o codec é o melhor em 36 das 46 imagens. Descodificação PPM ~22% mais rápida depois da otimização (norma partilhada pelos dois NLMS, recíprocos em vez de divisões, somas de erros por linha), sem mudar o resultado. Tentado sem ganho: 3.º modelo misturado nos resíduos, APM (SSE) (+0,05%), 16 entradas no NLMS, `-march=native`.

**Velocidade (ponto 1; imagens da cadeira, VM com 2 CPUs; Mpix/s cod. / desc.):**

| Variante | PGM bpp | PPM bpp | RAW bpp | PGM Mpix/s | PPM Mpix/s |
|---|---|---|---|---|---|
| normal | 3.632 | 11.656 | 6.930 | 3.2 / 4.6 | 0.89 / 1.48 |
| `-fast` (sem mistura, sem NLMS lento, sem 2.º andar) | 3.678 | 11.774 | 7.054 | 4.0 / 6.0 | 1.20 / 1.97 |
| `-fastest` (só MED + entre canais, sem NLMS) | 3.848 | 12.134 | 7.192 | 5.9 / 9.1 | 1.68 / 2.58 |
| `-t 2` (2 faixas em paralelo, 2 threads) | 3.637 | 11.673 | 6.928 | 4.3 / 7.8 | 1.06 / 2.65 |
| JPEG-XL e7 (1 thread) | 3.718 | 12.199 | 7.079 | 1.0 / 7.0 | 0.36 / 2.70 |

A otimização do ciclo principal sem mudar o resultado deu um ganho dentro do ruído de medição (0–5%): o tempo está espalhado por muitas operações. As faixas custam pouco nas fotografias (+0,1–0,9% com 2 faixas) e mais nas imagens de paleta (+0,9% com 2, +2–4% com 8), porque os modelos recomeçam em cada faixa. Comparar `-t 2` com o JPEG-XL de 1 thread não é justo (o JPEG-XL também pode usar várias threads); serve para mostrar o ganho real numa máquina com vários núcleos.

**Memória (ponto 2):** o codec deixou de guardar a imagem inteira (amostras lidas diretamente dos bytes do ficheiro, linhas transformadas criadas quando são precisas, janela de 3 linhas, descodificador a escrever cada linha diretamente no ficheiro de saída). O ficheiro comprimido ficou igual byte a byte (152 comparações).

| Imagem | Antes cod. / desc. | Depois cod. / desc. | JPEG-LS | JPEG-XL e7 |
|---|---|---|---|---|
| PPM 4536×3024 (13,7 Mpix) | 354 / 257 MB | **73 / 57 MB** | 124 / 116 MB | 1256 / 227 MB |
| RAW 4200×2154 16 bits | 129 / 64 MB | **38 / 29 MB** | 71 / 54 MB | 757 / 175 MB |
| PPM da cadeira (máx.) | 24 / 18 MB | **10 / 8 MB** | 12 / 12 MB | 117 / 24 MB |

O tempo não mudou. O que resta é praticamente o ficheiro de entrada mais o de saída (~5 bytes/píxel em PPM de 8 bits).

**Imagens sintéticas (ponto 3).** 21 imagens geradas por `scripts/get_synthetic_images.py` (gráficos, texto, capturas de ecrã, tabelas, cartas de cores): 6 usadas para desenvolver, **15 só para medir** (tabela abaixo, bpp). "Antes" é o codec no fim do ponto 2.

| Imagem | Antes | **Nosso** | JPEG-LS | JPEG-XL e7 | JPEG-XL e9 | PNG | xz | bzip2 |
|---|---|---|---|---|---|---|---|---|
| sk\_chessboard\_RGB.pgm | 0.071 | **0.050** | 0.557 | 0.074 | 0.048 | 0.054 | 0.043 | 0.032 |
| sk\_page.pgm | 3.955 | **3.955** | 4.315 | 4.113 | 4.062 | 4.635 | 4.917 | 5.205 |
| sk\_text.pgm | 4.003 | **4.003** | 4.227 | 4.115 | 4.070 | 4.408 | 4.859 | 4.977 |
| heatmap.ppm | 1.334 | **1.165** | 2.080 | 1.043 | 0.893 | 1.943 | 1.359 | 1.075 |
| histograms.ppm | 0.370 | **0.223** | 0.764 | 0.172 | 0.135 | 0.414 | 0.178 | 0.181 |
| jxl\_P3-sRGB-color-ring.ppm | 3.553 | **3.553** | 4.537 | 3.484 | 2.995 | 5.066 | 4.347 | 3.241 |
| jxl\_R2020-sRGB-color-bars.ppm | 3.673 | **3.673** | 4.688 | 3.417 | 3.182 | 4.767 | 5.578 | 8.121 |
| jxl\_R2020-sRGB-color-ring.ppm | 3.601 | **3.601** | 4.628 | 3.424 | 2.867 | 5.138 | 4.202 | 3.140 |
| jxl\_colorful\_chessboards.ppm | 1.586 | **0.618** | 12.617 | 1.078 | 0.425 | 0.902 | 0.529 | 0.537 |
| jxl\_ellipses.ppm | 1.681 | **1.329** | 1.941 | 1.416 | 1.158 | 1.503 | 1.106 | 1.343 |
| jxl\_rgb-to-gbr-test.ppm | 1.565 | **1.565** | 4.512 | 1.383 | 1.221 | 2.391 | 2.149 | 1.691 |
| pie\_chart.ppm | 0.579 | **0.435** | 0.540 | 0.408 | 0.307 | 0.513 | 0.389 | 0.381 |
| scatter.ppm | 1.092 | **0.986** | 1.197 | 0.869 | 0.694 | 1.069 | 0.921 | 0.867 |
| spreadsheet.ppm | 1.684 | **0.133** | 3.326 | 0.330 | 0.219 | 0.731 | 0.304 | 0.231 |
| web\_page.ppm | 1.419 | **0.212** | 2.593 | 0.204 | 0.146 | 0.568 | 0.204 | 0.497 |
| **média** | **2.011** | **1.700** | **3.502** | **1.702** | **1.495** | **2.273** | **2.072** | **2.101** |

- antes: +90% face ao JPEG-XL e9 (média geométrica por imagem)
- ours: +19% face ao JPEG-XL e9 (média geométrica por imagem)
- jpeg-xl-e7: +29% face ao JPEG-XL e9 (média geométrica por imagem)
- png-optipng: +81% face ao JPEG-XL e9 (média geométrica por imagem)
- xz-9: +32% face ao JPEG-XL e9 (média geométrica por imagem)
- bzip2-9: +31% face ao JPEG-XL e9 (média geométrica por imagem)


O modelo de correspondências dá −15% em média nestas imagens (e até −92% na folha de cálculo). Ficamos ao nível do JPEG-XL e7 e à frente do xz/bzip2/PNG, mas ainda ~19% atrás do e9 (sobretudo nos gradientes das cartas de cores, onde não há repetições). Nas fotografias não muda nada: o encoder só liga o modelo se poupar ≥ 1% na amostra (em 50 fotografias da cadeira e do conjunto separado, só ligou numa ecografia, que ficou 3,5% mais pequena). Descodificar com o modelo ligado é ~40% mais lento.

Tentado sem ganho: usar o valor previsto como mais um preditor na mistura (pior), mais de 3 tamanhos de janela, tabelas maiores.

RAW 16: 7.231 → 7.064 bpp (o maior ganho isolado veio da escala dos gradientes calculada pela gama central dos valores e não pelo maxval: 7.121 → 7.087).

**Experiências que não resultaram (para o relatório):** YCoCg-R e G,R−G,B−(R+G)/2 nunca são as melhores; contextos com pares de índices (hash) e WW/NN no modo de paleta; preditores N+NE−NNE e o 3.º preditor entre canais; taxa de aprendizagem do misturador alta (piora até 3%); tabela de recíprocos de 8 bits (introduzia um erro).
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

**Registo (Parte II, etapa 1: codec base):**
- **Problema:** codec sem perdas que reproduza o ficheiro original byte a byte, com a interface do enunciado.
- **Proposta da IA:** contentor com o cabeçalho PNM original e CRC-32; preditor MED; codificador aritmético binário adaptativo (estilo LZMA) com resíduos binarizados e contextos de atividade local; modo genérico (modelo de ordem 1 sobre bytes) para ficheiros que não dá para reconstruir; código do codificador e do descodificador partilhado num template.
- **Verificação:** testes de ida e volta com `cmp` em todas as imagens (incluindo Kodak e RAW em LE) e em 29 casos especiais gerados (`tests/make_special_images.py`): P2/P3, comentários, maxval 1/100/1000/4095, 1×1, ruído, RAW de 8 bits, bytes a mais, ficheiros vazios ou que não são imagens; ficheiros comprimidos corrompidos têm de ser rejeitados.
- **Problemas encontrados:** o ruído e os ficheiros minúsculos ficavam maiores do que o original (até +36%); foi acrescentado um modo "guardado" (só 18 bytes de cabeçalho).
- **Resultado:** ao nível do JPEG-LS (PGM −0,8%, PPM +0,3%, RAW 0,0%, Kodak −7,7%), ~12 Mpix/s em PGM.

**Registo (Parte II, etapa 2: cor, paleta, predição e contextos):**
- **Problema:** levar o codec do nível do JPEG-LS para a zona JPEG-XL e7–e9, sobretudo nos PPM.
- **Proposta da IA:** transformadas de cor reversíveis escolhidas por imagem, predição entre canais, modo de paleta com mistura logística de modelos, mistura ponderada de preditores com correção de viés, contextos adicionais. Cada passo medido e só mantido se melhorasse (ver a tabela de ablação).
- **Verificação:** `make test` (122 testes de ida e volta byte a byte) após cada alteração; benchmark completo no fim.
- **Erros encontrados:** (1) a IA assumiu que os PPM "difíceis" eram fotografias, mas o arial (tal como as Kodak) tem só 256 cores e precisava de outro modo; (2) uma tabela de recíprocos para acelerar a mistura tinha um *overflow* (peso 0 para o preditor perfeito), detetado porque a compressão piorou 0,4%; (3) uma variável de ambiente lida dentro do ciclo principal tornava o codec mais lento; (4) a primeira versão do misturador piorava a compressão: a taxa de aprendizagem era alta demais.
- **Lição:** medir cada alteração isoladamente; uma melhoria "óbvia" (misturar modelos) só funcionou depois de afinada.

**Registo (Parte II, etapa 3: preditores adaptativos):**
- **Problema:** ultrapassar o JPEG-XL e9 em compressão.
- **Proposta da IA:** preditores lineares adaptativos (NLMS) em aritmética inteira, um 2.º andar sobre os resíduos, empacotamento de valores para os RAW, e depois remover o que deixou de ajudar.
- **Verificação:** cada alteração medida isoladamente nas 19 imagens PGM/PPM (e RAW); `make test` (122 testes); otimizações de velocidade verificadas por igualdade byte a byte do ficheiro comprimido antes/depois.
- **Erros / surpresas:** a correção de viés e 3 dos preditores simples passaram a prejudicar depois de entrarem os NLMS (descoberto por ablação, não era esperado); a medição de tempos na máquina virtual tem ±15% de ruído, por isso só se aceitaram otimizações com ganho claro.

**Registo (Parte II, avaliação honesta):**
- **Problema:** os resultados tinham sido medidos nas mesmas imagens usadas para afinar o codec; o professor avalia com imagens privadas.
- **O que foi feito:** conjunto separado de 27 imagens públicas (`scripts/get_holdout_images.py`), benchmark completo, teste de determinismo entre compiladores, teste com uma imagem grande (13,7 Mpix).
- **Resultado:** nas imagens novas o codec fica entre o JPEG-XL e7 e o e9 (antes parecia melhor do que o e9: era otimismo da afinação, ~1–2%). Descodificação ~2× mais lenta do que o JPEG-XL.
- **Erro encontrado:** imagens cinzentas com poucos valores não consecutivos (8 bits guardados em 16) iam para o modo de paleta em vez do modo empacotado (3,60 → 2,76 bpp depois de corrigir). A IA não tinha previsto este caso; só apareceu com imagens novas.

**Registo (Parte II, ponto 1: velocidade):**
- **Proposta da IA:** otimizar o ciclo sem mudar o ficheiro (verificado byte a byte), perfis de velocidade e faixas em paralelo.
- **Resultado honesto:** as micro-otimizações não deram ganho mensurável (a IA esperava 1,3–1,8×; a estimativa estava errada). Os perfis e as faixas deram ganhos reais, à custa de um pouco de compressão. O modo por omissão continua igual (+14 bytes por ficheiro para o novo cabeçalho).
- **Verificação:** 145 testes (novos: perfis, faixas, imagens de 1 linha/coluna com mais faixas do que linhas, mesma saída com 1 e 4 threads).

**Registo (Parte II, ponto 2: memória):**
- **Proposta da IA:** processar por linhas sem guardar a imagem (leitura direta do ficheiro, janela de 3 linhas, escrita direta do resultado), mantendo exatamente as mesmas decisões do codificador.
- **Verificação:** 152 ficheiros comprimidos comparados byte a byte com a versão anterior (imagens da cadeira, Kodak, conjunto separado, perfis e faixas), o descodificador antigo a ler os ficheiros novos, 145 testes. Memória medida com `/usr/bin/time`.
- **Resultado:** 4–5× menos memória; agora menos do que o JPEG-LS (CharLS) e muito menos do que o JPEG-XL.

**Registo (Parte II, ponto 3: imagens sintéticas):**
- **Problema:** nas imagens sintéticas o codec era 2–7× pior do que o JPEG-XL e até do que o xz.
- **Proposta da IA:** modelo de correspondências (contextos exatos de vizinhos numa tabela de dispersão, 2 candidatos, só codificado quando tem acertado), ativado por imagem.
- **Metodologia:** imagens divididas em desenvolvimento e teste antes de começar; o resultado final foi medido só nas de teste; confirmado que as fotografias não pioram.
- **Problemas encontrados:** a primeira versão piorava as fotografias em tons de cinzento (+1,4%) e abrandava a descodificação ~40–85%; resolvido com a ativação condicional (por classe de contexto e por imagem).

**Registo (Parte II, ponto 4: imagens de 16 bits e grandes):**
- **O que foi feito:** 12 imagens novas de fontes públicas (pydicom-data: TAC, RM, radiografias; openjpeg-data: fotogramas de cinema 2K/4K de 16 bits e fotografias grandes), benchmark com todos os codecs, teste com uma imagem de 80 Mpix.
- **Erro encontrado:** imagens de cor com 12 bits nos bits altos de 16 (valores múltiplos de 16) não eram tratadas (+65% face ao JPEG-XL nesse fotograma). A IA tinha implementado o caso só para cinzentos. Corrigido com a escala de valores por canal (30,57 → 18,50 bpp); 3 imagens de teste novas (149 testes).
- **Resultado honesto:** melhores do que o JPEG-XL e9 nas imagens de 16 bits (médicas −2,6%, cor −5,7%), piores nas fotografias de 8 bits grandes (+5,7%), sobretudo numa imagem que vem de um JPEG.

> Dica: ir acrescentando aqui uma entrada por cada sessão de trabalho com IA. No fim, o relatório 2 escreve-se quase sozinho.
