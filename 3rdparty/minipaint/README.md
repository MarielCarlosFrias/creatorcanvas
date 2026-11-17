# MiniPaint

Módulo reutilizável de edição de desenho estilo **Microsoft Paint**, escrito em **C++17** e **SFML 3**.

O `MiniPaint` não cria nem controla janelas. Ele mantém um canvas off-screen em um `sf::RenderTexture`, recebe eventos do programa anfitrião e pode copiar o resultado para uma imagem ou para outro render target.

O **exemplo** (`minipaint-standalone`) é escrito apenas em SFML e reproduz o layout autêntico do **Microsoft Paint** (Windows 7/10). Os ícones são coloridos e estilizados com fidelidade ao Paint original, salvos em `example/icons/` e também embutidos em `example/EmbeddedIcons.hpp` (com suporte para carregamento de ícones personalizados de uma pasta `icons/`). A paleta de cores fica integrada no Ribbon superior junto com os seletores de **Cor 1** e **Cor 2**. O preview de formas (linha, retângulo, elipse) é desenhado em tempo real durante o arrasto.

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ [Logo Paint] [Salvar] [Desfazer] [Refazer]   Documento - Paint                         │  quick access
├────────────────────────────────────────────────────────────────────────────────────────┤
│ [Arquivo] | Início (Ativa) | Exibir                                                    │  tabs
├────────────────────────────────────────────────────────────────────────────────────────┤
│ Imagem     │ Ferramentas  │ Pincéis │ Formas   │ Tamanho │ Cores             │ Camadas     │  ribbon
│ Novo Abrir │ Lápis Balde A│ Pincel  │ \  □  ○  │ 1/3/6px │ [Cor1] [Cor2]     │ Undo / Redo │
│ Salvar Dim │ Borr Gotas 🔍│ Spray   │ Preencher│ Slider  │ [28 cores Paint]  │ Painel +/-  │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                        │
│                                        Canvas                                          │
│                    (folha branca + sombra, alças de redimensionamento)                 │
│                                                                                        │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ + Coordenadas (X, Y px)  │  Tamanho (W × H px)  │  Dicas  │  [-] ────●──── [+] 100%    │  status bar
└────────────────────────────────────────────────────────────────────────────────────────┘
```

## Funcionalidades

- Pencil com traço **suavizado** (curva de Bézier através dos pontos do rato) e pincel **anti-aliasing** (cobertura real por pixel), para curvas e diagonais sem degraus.
- Eraser com o mesmo traço suavizado.
- Spray de Airbrush com semente aleatória.
- Linha, retângulo e elipse com **preview em tempo real** durante o arrasto.
- Bucket Fill (flood fill) com tolerância opcional por canal.
- Eyedropper que lê a cor do canvas composto.
- Camadas com nome, visibilidade, bloqueio, opacidade e blend mode
  (`sf::BlendAlpha`, `Add`, `Multiply`, `Min`, `Max`, `None`).
- Undo/redo com limite configurável (20 passos por omissão), baseado em runs de pixels alterados.
- Flip horizontal/vertical, rotação de 90° e redimensionamento (undoable).
- Clear, flip, rotate e texto são comandos desfazíveis.
- Pintura com botão esquerdo (cor primária) e botão direito (cor secundária).
- Paleta de 28 cores (padrão Paint), sincronizada com a API de paleta.
- Diálogos nativos da app: **Abrir** (navegador de ficheiros), **Guardar** e **Redimensionar tela**; **Novo** documento em branco.
- Grelha de desenho (Ctrl+G), zoom 0.25x–8x, cursor de mira no canvas.

## Requisitos

- CMake 3.22 ou superior.
- Compilador com suporte a C++17.
- SFML 3.0 ou superior, com os componentes `Graphics`, `Window` e `System`.
- **Nenhuma outra dependência** para o exemplo (sem ImGui, sem OpenGL explícito).

> `sf::RenderTexture` precisa de um contexto OpenGL ativo. Crie a `MiniPaint` depois da janela principal (ou de outro contexto gráfico) estar aberta.

## Compilar

```bash
cd minipaint          # o CMakeLists.txt está nesta pasta
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Executáveis gerados:

- `build/minipaint-standalone`: editor de demonstração estilo Paint.
- `build/minipaint-tests`: testes de integração.

Quando `minipaint/` é incluído com `add_subdirectory()`, o exemplo, os testes e as regras de instalação ficam desligados por omissão e podem ser ativados com `MINIPAINT_BUILD_EXAMPLE`, `MINIPAINT_BUILD_TESTS` e `MINIPAINT_INSTALL`.

## Integrar no programa principal

### Opção A: como subdirectório

```cmake
add_subdirectory(path/para/minipaint)
target_link_libraries(minha_app PRIVATE MiniPaint::MiniPaint)
```

### Opção B: depois de instalado

```cmake
find_package(MiniPaint CONFIG REQUIRED)
target_link_libraries(minha_app PRIVATE MiniPaint::MiniPaint)
```

### Código de integração mínimo

```cpp
#include "MiniPaint.hpp"

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

int main()
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "Editor");
    MiniPaint paint(800, 600);

    const sf::FloatRect canvasArea({0, 0}, {800, 600});
    paint.setViewport(canvasArea);

    while (const std::optional event = window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            window.close();
        }

        paint.handleEvent(*event, canvasArea);
    }

    paint.draw(window, canvasArea);
    sf::Image result = paint.getResult();
    return 0;
}
```

## Controlos do exemplo

| Entrada | Ação |
|---|---|
| Botão esquerdo / direito | Desenhar com cor primária / secundária |
| Clique esquerdo na paleta | Definir cor primária |
| Clique direito na paleta | Definir cor secundária |
| `1`–`9` | Selecionar ferramenta |
| `H` / `V` | Flip horizontal / vertical do layer ativo |
| `R` | Rodar 90° para a direita |
| `Ctrl+Z` / `Ctrl+Y` / `Ctrl+Shift+Z` | Undo / redo |
| `Ctrl+S` | Guardar imagem (diálogo) |
| `Ctrl+O` | Abrir imagem (diálogo) |
| `Ctrl+N` | Nova imagem em branco |
| `Ctrl+0` | Zoom a 100% |
| `Ctrl+G` | Mostrar/ocultar a grelha do canvas |
| `+` / `-` / roda do mouse | Zoom |
| `Esc` | Sair (ou cancelar entrada de texto) |
| Clique na origem das cores | Trocar cor primária e secundária |

O separador **Ver** oferece Zoom (−, +, Ajustar à janela) e Mostrar (grelha, painel de camadas). O grupo **Imagem** tem Novo, Redimensionar, Abrir e Guardar. A barra de **acesso rápido** traz o ícone da aplicação, Guardar, Anular e Refazer, e o nome do ficheiro atual (`Sem título - MiniPaint` até à primeira vez que guardas ou abres uma imagem).

O painel **Camadas** (botão `Camadas` no ribbon) permite: ver/ocultar camadas (olho), bloquear/desbloquear (cadeado), selecionar camada ativa, adicionar e excluir camadas.

## API principal

| API | Finalidade |
|---|---|
| `setTool(ToolType)` | Pencil, Eraser, Line, Rectangle, Ellipse, BucketFill, Eyedropper, Airbrush, Text. |
| `setPrimaryColor()` / `setSecondaryColor()` | Cores de desenho. |
| `setBackgroundColor()` | Cor usada pelo Eraser. |
| `setBrushSize()` | Diâmetro do pincel (1–128 px). |
| `setFillShapes()` | Preenchimento de retângulos/elipses. |
| `setFillTolerance()` | Tolerância do Bucket Fill (0–255). |
| `setPalette()` / `selectPaletteColor()` | Define e seleciona o índice da paleta. |
| `addLayer()` / `removeLayer()` / `setActiveLayer()` | Gestão de camadas. |
| `setLayerOpacity()` / `setLayerVisibility()` / `setLayerLocked()` | Propriedades de cada camada. |
| `undo()` / `redo()` / `setUndoLimit()` | Histórico de comandos. |
| `clear()` / `flip*()` / `rotate*()` / `drawText()` / `resizeCanvas()` | Edições desfazíveis. |
| `loadFromImage()` / `loadFromFile()` / `saveToFile()` | PNG / imagem. |
| `setViewport()` | Área da janela usada para mapear o rato. |
| `getResult()` | Cópia do canvas composto. |

## Arquitetura

```text
include/
├── MiniPaint.hpp          # API principal (PImpl)
├── Tool.hpp               # Interface Strategy e fábrica de ferramentas
├── commands/Command.hpp   # Interface do padrão Command
└── tools/                 # Cabeçalhos das ferramentas e ToolContext
src/
├── MiniPaint.cpp          # Façade, eventos, viewport, histórico
├── Canvas.*               # RenderTexture + espelho de pixels
├── Layer.*                # Camadas e composição (blend modes)
├── commands/ImageCommand  # Undo por runs de pixels / imagens completas
└── tools/                 # Implementações das ferramentas
```

- **Strategy:** cada ferramenta implementa `Tool` e pode ser trocada em tempo de execução.
- **Command:** cada operação modificável cria um comando desfazível (`ImageCommand` usa runs horizontais de pixels para edits do mesmo tamanho, ou imagens completas para rotações).
- **Hitórico resiliente:** os comandos referenciam o índice da camada; remover camadas desativa ou remapeia os comandos afetados.
- **Preview:** formas são reconstruídas a partir do snapshot inicial do traço; no commit, a imagem final é capturada e transformada em comando.
- **RenderTexture:** o canvas visível é off-screen e pode ser composto diretamente no render target do anfitrião.

### Desempenho do traço (composição incremental)

Durante um traço de lápis/borracha/airbrush, cada evento do rato sincroniza apenas o **retângulo pintado**: a ferramenta declara a região alterada em `ToolContext::dirtyRect`, o `MiniPaint` recompõe apenas essa região (`compositeLayersIntoRegion()`, pixel-local — os pixels são idênticos a uma recomposição completa, garantido por teste) e o `Canvas::updateRegion()` atualiza o espelho + upload parcial da textura + um único blit na FBO.

O caminho antigo (recomposição de **todas** as camadas + upload do canvas inteiro) foi substituído; medido a 1920×1080 com duas camadas: **~210× mais rápido por evento** (≈50 ms → ≈0,24 ms), o que eliminava o atraso visível ao desenhar. Bucket fill, texto, formas comprometidas, flips, rotações, redimensionamentos e undo/redo continuam a usar a recomposição completa por segurança (operações estruturais ou que saem do retângulo traçado).

### Redraw apenas quando algo muda (exemplo standalone)

O exemplo redesenha **todo** o ribbon + canvas a 60 fps apenas quando algo muda: qualquer evento (rato, teclado, foco) marca `needsRedraw`, e uma rede de segurança repinta ~2×/s para cobrir a janela ser coberta e exposta (a SFML 3 não tem evento de *expose*). Sem eventos, o loop salta o bloco de desenho inteiro — CPU de idle ≈ 0. O `updateWindowTitle()` já só chamava `setTitle` quando o texto mudava.

## Testes

O teste smoke cobre: carregamento/gravação de PNG, pencil, undo/redo, bucket fill, eyedropper, clear, paleta, limites do pincel, shapes, elipses, re-composição multi-camada no undo, rotação desfazível, bloqueio de camada com clear, `getLayerName` fora de alcance, retângulos preenchidos dentro do limite e validade do histórico após `addLayer`/`resizeCanvas`. Inclui ainda duas garantias da composição incremental: `compositeLayersIntoRegion()` é pixel-idêntico a `compositeLayers()` recortado (50 combinações aleatórias de camadas/regiões) e o canvas visível após um traço incremental coincide com uma recomposição nova da pilha.
