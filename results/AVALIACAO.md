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

## 5. O que isto quer dizer para a avaliação

- Taxa de compressão: muito boa. Ao nível do JPEG-XL e7–e9 e muito acima do JPEG-LS, PNG
  e compressores genéricos. Em imagens desconhecidas, contar com algo **ligeiramente pior
  do que o JPEG-XL e9**, e não melhor.
- Velocidade: boa a codificar, **fraca a descodificar** face ao JPEG-XL/JPEG-LS.
- Memória: boa (depois do ponto 2): ~5 bytes/píxel, das mais baixas dos codecs medidos (abaixo do JPEG-LS e do JPEG-XL; o bzip2 usa menos).
