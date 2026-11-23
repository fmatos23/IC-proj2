# Avaliação do codec sem perdas (Parte II): resultados honestos

Data: 21 nov 2026. Versão: fim da etapa 3, mais a correção da escolha de modo (ver 4).
Máquina: VM, Intel Xeon @ 2,1 GHz, 2 CPUs. Os tempos variam ±15% de execução para
execução e têm de ser medidos de novo na máquina final.

O enunciado avalia: **taxa de compressão (bpp médio), velocidade de codificação e de
descodificação e memória**, num **conjunto de imagens privado**. O codec só pode ser
avaliado com honestidade em imagens que não foram usadas para o afinar.

## 1. Sem perdas: OK

- `make test`: 122 testes passam. 82 são do codec: 50 idas e voltas byte a byte com as
  imagens da cadeira (RAW também em little-endian), 29 casos especiais e 3 entradas corrompidas.
- 27 imagens novas (conjunto separado, ver 2): todas reconstruídas byte a byte.
- **Determinismo:** o ficheiro comprimido é igual byte a byte com `g++ -O0`, `g++ -O3`,
  `g++ -O3 -march=native` e `clang++ -O2`, e cada versão descodifica os ficheiros das
  outras (21 verificações, 0 falhas). Uma imagem comprimida numa máquina descomprime noutra.

## 2. Compressão: imagens da cadeira vs imagens novas

As **imagens da cadeira** foram usadas para afinar todos os parâmetros. Os resultados
nelas são otimistas.

| Grupo (cadeira) | Nosso | JPEG-LS | JPEG-XL e7 | JPEG-XL e9 |
|---|---|---|---|---|
| PGM (7) | **3,631** | 3,883 | 3,718 | 3,647 |
| PPM (12) | **11,656** | 13,938 | 12,199 | 11,747 |
| RAW 16 (4) | 6,930 | 7,254 | 7,079 | **6,924** |
| Kodak, 256 cores (23) | **3,132** | 12,848 | 4,301 | 3,899 |

O **conjunto separado** (`scripts/get_holdout_images.py`, nunca usado para afinar)
tem 27 imagens: 16 amostras do scikit-image e imagens de teste do libjxl. O "flower"
vem do conjunto de teste do próprio JPEG-XL.

| Grupo (novo) | Nosso | JPEG-LS | JPEG-XL e7 | JPEG-XL e9 | vs JPEG-LS: nosso / e7 / e9 |
|---|---|---|---|---|---|
| PGM (13) | 3,308 | 3,618 | 3,334 | **3,305** | −11,7% / −13,1% / −14,1% |
| PPM (10) | 7,868 | 10,161 | 8,033 | **7,834** | −22,6% / −20,7% / −23,7% |
| RAW 16 (4)\* | **2,762** | 8,211 | 2,917 | 2,905 | −65,0% / −63,0% / −63,2% |
| Todas (27) | | | | | −26,6% / −26,0% / −27,5% |

\* As 4 RAW são a mesma imagem (flower_small) a 10, 12, 14 e 16 bits. Não são independentes.

**Conclusão honesta:** em imagens que o codec nunca viu, ficamos **entre o JPEG-XL e7
e o e9**, ligeiramente atrás do e9. Nas imagens da cadeira parecíamos à frente do e9:
essa diferença (~1–2%) é o efeito da afinação nessas imagens. Melhores do que o
JPEG-LS em todas as 27 imagens novas. Melhores do que o e9 em 18 das 27.

**Onde perdemos claramente** (imagens novas):
- `sk_moon` (+58% face ao e9): imagem ampliada com píxeis duplicados. O nosso modelo
  não explora píxeis repetidos em posições fixas.
- `sk_color` (+14%): imagem sintética (gráfico de cores).
- `flower.ppm` (+6%) e `chelsea` (+4%): fotografias em que o JPEG-XL é simplesmente melhor.
- Ganhamos sobretudo em fotografias naturais de 500×500 e em dados com poucos valores usados.

## 3. Velocidade e memória (imagens novas; Mpix/s, mais é melhor)

| | Nosso enc / dec | JPEG-LS | JPEG-XL e7 | JPEG-XL e9 |
|---|---|---|---|---|
| PGM | 3,1 / 4,9 | 20,4 / 25,1 | 1,2 / 8,0 | 0,10 / 6,3 |
| PPM | 1,2 / 1,6 | 6,7 / 7,2 | 0,45 / 3,5 | 0,04 / 3,2 |
| Memória máx. PPM enc / dec (antes do ponto 2) | 91 / 66 MB | 40 / 32 MB | 266 / 72 MB | 558 / 72 MB |

Numa imagem grande (PPM 4536×3024, 13,7 Mpix, 41 MB):

| | Codificação | Descodificação | bpp |
|---|---|---|---|
| Nosso | 9,3 s, 357 MB | 8,5 s, 257 MB | 6,21 |
| JPEG-XL e7 | 42,4 s, 1256 MB | 4,4 s, 227 MB | 5,86 |
| JPEG-LS | 2,0 s, 124 MB | 1,7 s, 116 MB | — |

Avaliação honesta:
- **Codificação:** 2–3× mais rápida do que o JPEG-XL e7, ~30× mais rápida do que o e9.
- **Descodificação:** **o nosso ponto fraco.** ~2× mais lenta do que o JPEG-XL e 3–5× mais
  lenta do que o JPEG-LS. O codec é simétrico: o descodificador faz o mesmo trabalho que o
  codificador.
- **Memória:** (antes do ponto 2) ~26 bytes/píxel a codificar PPM, porque a imagem inteira
  ficava em memória. Ver a atualização abaixo.

**Atualização (ponto 2, memória):** o codec passou a processar a imagem por linhas,
sem a guardar inteira (o ficheiro comprimido é o mesmo). Na imagem de 13,7 Mpix:
**73 MB a codificar e 57 MB a descodificar** (antes 357 / 257 MB). Nas imagens da cadeira,
o máximo em PPM é agora 10 / 8 MB, menos do que o JPEG-LS (12 / 12 MB). A memória
deixou de ser um ponto fraco.

**Atualização (ponto 3, imagens sintéticas):** em 15 imagens sintéticas de teste (gráficos,
texto, capturas de ecrã, tabelas, cartas de cores; `results/sota_synthetic.md`), o modelo de
correspondências baixou a média de 2,011 para **1,700 bpp**: igual ao JPEG-XL e7 (1,702), melhor
do que xz/bzip2/PNG, mas **~19% atrás do JPEG-XL e9** (1,495). Antes estávamos +90% atrás do e9.
As fotografias não mudaram.

## 4. Problema encontrado e corrigido com este teste

Nas imagens novas, as imagens em tons de cinzento com ≤ 256 valores distintos mas não
consecutivos (por exemplo, dados de 8 bits guardados em 12 ou 16 bits) iam para o modo de
paleta, porque a escolha não comparava com o modo "valores empacotados". Resultado: RAW
3,60 bpp, contra 2,90 do JPEG-XL. Depois da correção (o encoder compara os três modos):
**2,76 bpp**. Isto não mudou nada nas imagens da cadeira (é uma correção da lógica de
escolha, não uma afinação). Mesmo assim, os resultados das RAW novas deixam de ser 100%
"nunca vistos".

Nota: o PNG (optipng) aparece como "não sem perdas" numa RAW de 16 bits porque reduz a
imagem para 8 bits (todos os valores eram múltiplos de 257). Na prática é sem perdas;
o nosso script de verificação é que compara a representação. Não afeta o nosso codec.

## 4b. Ponto 4: imagens de 16 bits reais e imagens grandes

Três conjuntos novos, nunca usados para afinar (`scripts/get_more_images.py`), comparados com
todos os codecs de referência (`results/sota_p4_*.md`). Tudo verificado sem perdas.

| Conjunto | nosso | JPEG-LS | JPEG-XL e7 | JPEG-XL e9 | melhor genérico |
|---|---|---|---|---|---|
| Médicas, cinzento 10–15 bits em 16 (5: TAC, RM, radiografia) | **4,027** | 4,467 | 4,189 | 4,141 | 5,309 (bzip2) |
| Cor 16 bits (4: fotograma 4K e 3 fotogramas de cinema 2K) | **25,64** | 33,15 | 27,33 | 27,19 | 39,26 (xz) |
| Fotografias 8 bits grandes (3, até 5 Mpix) | 7,102 | 9,164 | 7,054 | **6,788** | 9,073 (bzip2) |

(bpp médio; nas de cor, 48 bpp sem compressão. Valores do nosso codec já com o ponto 5.)

- **Médicas:** melhor em 4 de 5 (o e9 ganha na radiografia RG3 por 3%).
- **Cor 16 bits:** melhor nas 4. **Problema encontrado e corrigido:** um dos fotogramas
  (`X_6`) tem dados de 12 bits guardados nos bits altos de 16 bits (todos os valores múltiplos
  de 16). Só os cinzentos tinham tratamento para isto; na cor gastávamos 4 bits a mais por
  amostra: **30,57 bpp contra 18,68 do e9**. Novo passo "escala de valores" (por canal,
  `v = off + k·passo`, codifica-se `k`): **18,50 bpp**. Não mudou nada nas imagens da cadeira
  nem no conjunto separado (só atua quando todos os valores de um canal estão numa grelha).
- **Fotografias grandes:** pior do que o e9 nas 3 (+4,6%) e do que o e7 numa (Bretagne2,
  7,27 contra 6,66). Esta imagem foi, quase de certeza, descodificada de um JPEG (blocos 8×8,
  croma subamostrado): ver o ponto 5 abaixo.
- **Imagem muito grande:** 80,6 Mpix (10368×7776, 242 MB): codificação 65 s / 382 MB,
  descodificação 63 s / 317 MB, sem perdas. A memória é ~o tamanho do ficheiro + 75–140 MB
  (o ficheiro inteiro é lido para memória; o resto é processado por linhas).
- **Velocidade nas de 16 bits:** cerca de 1 Mpix/s (cor) e 3,6 Mpix/s (cinzento) nos dois
  sentidos; o JPEG-XL e7 descodifica 2,4× mais depressa mas codifica 4× mais devagar e usa
  10× mais memória (1092 MB contra 107 MB no 4K).

## 4c. Ponto 5: imagens descodificadas de JPEG

Separação feita antes de começar (`scripts/get_jpeg_images.py`): **dev** = 7 fotografias do
conjunto separado comprimidas em JPEG e descodificadas (para desenhar); **teste** = 5 JPEG reais
dos dados de teste do libjxl + as 3 fotografias grandes do ponto 4 (só para medir).

Diagnóstico: o problema está nos canais de cor (R−G, B−G). Numa imagem de dev, só o canal G
fica 6% *melhor* do que o JPEG-XL, mas os canais de cor sozinhos ficam 10% piores. Com o
preditor mais simples possível (W), o JPEG-XL já faz 1,14 bpp nesse canal, contra 1,25 do
nosso: a vantagem dele está nos contextos (a árvore usa os gradientes exatos), não na
predição.

Alteração: um 3.º modelo, com os gradientes locais com sinal (NE−N, N−NW, NW−W, 13 níveis cada,
2197 contextos) como contexto, misturado na decisão "resíduo = 0" e no sinal.

| bpp médio | antes | depois | JPEG-XL e7 | JPEG-XL e9 |
|---|---|---|---|---|
| JPEG, dev (7) | 6,449 | 6,417 | 6,259 | 6,227 |
| **JPEG, teste (8)** | 6,223 | **6,171** (−0,8%) | 6,117 | 5,966 |
| Cadeira, PGM / PPM | 3,632 / 11,656 | 3,616 / 11,634 | 3,718 / 12,199 | 3,647 / 11,747 |
| Conjunto separado, PGM / PPM / RAW | 3,308 / 7,868 / 2,762 | 3,303 / 7,843 / 2,758 | | |
| Sintéticas, teste (15) | 1,700 | 1,685 | 1,702 | 1,495 |

Honestamente: **ganho pequeno** (−0,8% nas JPEG, −0,1 a −0,4% no resto) e **descodificação
~5–10% mais lenta** (medido no mesmo momento, sem/com o modelo). Continuamos ~3% atrás do e9 e
~1% atrás do e7 nas imagens vindas de JPEG.

Tentado sem ganho que justificasse (removido do código): modelos separados nas bordas dos blocos
8×8 (−0,2%), preditores de croma que resolvem R e B a partir de Cb/Cr previstos (ganho só em
1 de 5 imagens), o preditor MED nos canais de cor (ajuda nas JPEG, piora na cadeira), o 3.º
modelo também nos bits de classe (+0,15% de ganho por ~10% de tempo).

## 5. O que isto quer dizer para a avaliação

- Taxa de compressão: muito boa. Ao nível do JPEG-XL e7–e9 e muito acima do JPEG-LS, PNG
  e compressores genéricos. Em imagens desconhecidas, contar com algo **ligeiramente pior
  do que o JPEG-XL e9**, e não melhor.
- Velocidade: boa a codificar, **fraca a descodificar** face ao JPEG-XL/JPEG-LS.
- Memória: boa (depois do ponto 2): ~5 bytes/píxel, das mais baixas dos codecs medidos (abaixo do JPEG-LS e do JPEG-XL; o bzip2 usa menos).
