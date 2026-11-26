# Pacotes de recursos do CreatorCanvas

Cada subpasta de `resources/packs/` é um pacote independente com um
`pack.json` que descreve os seus itens. Os ficheiros são embutidos no
binário através de `resources/packs.qrc` (prefixo `:/packs`).

## Formato do `pack.json`

| campo | descrição |
|---|---|
| `id` | identificador estável do pacote (usado nos caminhos dos recursos) |
| `version` | versão do pacote (informativa) |
| `items[].file` | nome do ficheiro dentro da pasta do pacote |
| `items[].key` | chave i18n do rótulo (namespace `editor`) |
| `items[].category` | `background`, `texture`, `overlay` ou `template` |

As categorias são lidas dos manifestos: o filtro do navegador
(`PackBrowser`) mostra as que existirem, com rótulos de
`pack.category.<categoria>`.

## Pacotes incluídos

### `starter` — 23 itens
Fundos para miniaturas, texturas e três ecrãs finais de vídeo.

### `texturas` — 32 itens
Papeis (grunge, pardo, branco, liso, amassado, manchado, rasgado), parede
rachada, tapete, plásticos, topografias, abstractos, riscos e linhas
onduladas. JPEG progressivo, qualidade 80, lado maior limitado a 1280 px.

### `sobreposicoes` — 16 itens
Sete de poeira/ruído e dez de grunge, com canal alfa. PNG em modo
cinzento+alfa (LA) a 900–1100 px — o cinzento poupa ~44% face a RGBA sem
perda visível. Quando aplicadas a um documento aberto entram como camada de
cobertura com modo de mesclagem **Screen** (`MainWindow::applyPackItem`).

## Tamanho total e proveniência

Todo o conteúdo vem de `~/Documentos/Pack de ediçao` (pastas *Texturas*,
*thumb e video*, *intro de finais de video* e *EFEITOS DE EDIÇÃO*),
reencodado e sem metadados de origem. Total: **~13 MB** (starter 2,9 MB,
texturas 4,9 MB, sobreposições 4,7 MB).

**Direitos:** confirmado pelo utilizador que tem o direito de redistribuir
estas imagens. Substituir um ficheiro exige apenas actualizá-lo no disco e no
`pack.json`; o teste `test_pack_store` garante que manifesto, recursos,
rótulos traduzidos e filtros continuam coerentes.
