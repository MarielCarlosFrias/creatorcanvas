# CreatorCanvas 0.3.0 — notas de lançamento

**Data:** 2026-11-26 · **Tag:** `v0.3.0` · **Commit:** `0f09767`

Primeira release com o foco em *facilidade de uso para quem começa* e nos
primeiros limites de memória. Tudo abaixo está coberto por testes (63 suites,
smoke test 100% saudável).

## Destaques

- 🔎 **Paleta de comandos `Ctrl+K`** — escreve o nome de qualquer comando e
  prime Enter. Cobre *todas* as acções, incluindo as 35 que antes não estavam
  registadas (formas extra, filtros artísticos, `Select ▸ Modify`, zonas
  seguras, layer comps, web slices, modo avançado, projectos de exemplo).
- 📦 **71 recursos incluídos em 3 pacotes** — fundos para miniaturas, texturas
  e 16 sobreposições (poeira/grunge) que se aplicam com modo **Screen**.
  Navegador com filtro por categoria e **pesquisa**; `Ctrl+Shift+B` ou o botão
  no ecrã inicial. Escolher um fundo sem documento aberto cria a tela no
  tamanho exacto da imagem.
- 🧭 **Primeira utilização guiada** — diálogo de boas-vindas com escolha de
  idioma, **tour de 4 passos** (repetível em *Ajuda ▸ Tour rápido*), painel
  **"Primeiros passos"** com objectivos que se marcam sozinhos, dicas vivas
  (cada ferramenta explica-se na barra de estado; canvas vazio diz como
  começar) e uma barra de dicas dispensável.
- ⚖️ **Limites de memória** — ouvir o histórico agora tem um **orçamento de
  memória** (512 MB por omissão, configurável nas Definições) além do limite
  de passos, e a cache de renderização **adapta-se à RAM da máquina**
  (96/192/256 MB).

## Também nesta versão

- `File ▸ Save PNG Copy…` (**`Ctrl+Shift+E`**) e `Layer ▸ Save Layer as PNG…`
  (**`Ctrl+Alt+E`**) — exportação num passo, confirmada na barra de estado.
- Os diálogos de exportação **lembram a última pasta** usada.
- **Modo avançado** também nas Definições, com aplicação imediata.
- `Image ▸ Image Size…`, `Select ▸ Transform Selection…`, **Smart Filters**
  (blur/ruído/twirl como camadas de ajuste), Batch com PDF/SVG e PSD, guias
  arrastáveis das réguas.
- Autosave já **não dispara por baixo de um diálogo modal**.

## Correcções

- Aplicar um item do pacote criava **duas** camadas (duplo clique emitia dois
  sinais).
- **`Ctrl+B` estava duplicado** (barra de ferramentas vs. fundos) — agora
  `Ctrl+Shift+B`, e um teste falha se dois comandos voltarem a partilhar
  atalho.
- Aplicar uma sobreposição podia **estragar a camada errada** (usava a
  selecção antiga em vez da camada importada).
- O painel "Primeiros passos" **não aparecia** ao entrar no editor e não tinha
  forma de o descartar.
- Fechar uma janela com alterações **pendurava** as execuções automatizadas
  (modal ignorava o modo suprimido).

## Artefactos

| Ficheiro | Tamanho | Notas |
|---|---|---|
| `CreatorCanvas-0.3.0-x86_64.AppImage` | ~47 MB | portable, inclui Qt + onnxruntime + pacotes |
| `creatorcanvas_0.3.0_amd64.deb` | ~19 MB | `sudo dpkg -i` |

## Actualização

Não há migração de projectos: os `.creatorcanvas` existentes abrem como
antes. As definições novas (memória do histórico, boas-vindas, primeiros
passos) ficam com valores por omissão sensatos.

## Limitações conhecidas

- Ainda **só imagens** (sem linha de tempo/vídeo) e o compositor continua em
  software (o caminho GPU/QRhi precisa de um ecrã real para ser validado).
- Importação de PSD minimalista (apenas compósito achatado).
- `mainwindow.cpp` continua a ser o maior ficheiro do projecto; a divisão
  incremental é a próxima tarefa de manutenção.
