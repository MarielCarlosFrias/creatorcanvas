# Projetos de Exemplo e Prática — CreatorCanvas

Esta pasta contém **12 projetos prontos no formato oficial (`.creatorcanvas`)**, criados especificamente para você praticar cada uma das ferramentas do programa de forma direta, sem precisar começar do zero.

---

## Como Abrir um Projeto de Exemplo:
1. Abra o **CreatorCanvas** (`./build/linux-debug/bin/CreatorCanvas`).
2. Clique em **"Abrir Projeto"** na tela inicial ou vá em **Arquivo > Abrir...** (`Ctrl + O`).
3. Navegue até esta pasta: `~/creatorcanvas/exemplos_projetos/`
4. Selecione o projeto desejado e clique em **Abrir**.
5. Você também pode acessar diretamente pelo menu **Ajuda > Projetos de Exemplo para Prática...**.

---

## Lista dos Projetos de Prática:

### 1. `01_Cenario_Banner_Social.creatorcanvas` (1080×1080 px)
* **Objetivo:** Praticar criação e alinhamento de posts para redes sociais.
* **Ferramentas para Treinar:**
  * **Mover / Seleção (`V`):** Mova os elementos e observe o alinhamento com guias magnéticas azuis.
  * **Texto (`T`):** Edite os textos, altere fontes e teste efeitos de sombra e contorno no Inspetor à direita.
  * **Formas (`U`):** Altere as cores do card central e do botão nos painéis de propriedades.
  * **Gradiente (`G`):** Mude as cores do fundo degradê.

### 2. `02_Cenario_Retoque_Foto.creatorcanvas` (1280×800 px)
* **Objetivo:** Remover manchas, espinhas e arranhões em fotos e texturas.
* **Ferramentas para Treinar:**
  * **Pincel de Recuperação (`J`):** Selecione a camada da foto. Segure `Shift` ou `Ctrl` e clique (ou use o **Botão Direito do Mouse**) em uma área lisa de pele/textura para coletar a amostra. Em seguida, clique sobre as manchas pretas para removê-las perfeitamente sem borrar!
  * **Carimbo de Clonagem (`S`):** Use para cobrir o risco com textura clonada.
  * **Borrão (Smudge):** Suavize transições marcadas.

### 3. `03_Cenario_Recorte_Objeto.creatorcanvas` (1080×1080 px)
* **Objetivo:** Isolar e recortar um objeto (troféu dourado) para aplicá-lo em um novo cenário ao fundo.
* **3 Métodos para Treinar:**
  * **Método 1 (IA):** Selecione a camada `Objeto_Para_Recortar` e acione o menu **Imagem > Remover Fundo com IA**.
  * **Método 2 (Varinha `W`):** Pressione `W`, clique sobre o fundo ciano sólido brilhante e aperte `Delete`.
  * **Método 3 (Tesoura / Laço `L`):** Pressione `L`, contorne o troféu clicando nos vértices e aperte `Enter` para fechar a seleção! Em seguida aperte `Ctrl + J`.

### 4. `04_Cenario_Vetor_Logotipo.creatorcanvas` (1200×800 px)
* **Objetivo:** Dominar curvas suaves de Bézier e nós vetoriais.
* **Ferramentas para Treinar:**
  * **Caneta (`P`):**
    * **Clique e solte:** Cria nós de canto retos.
    * **Clique e arraste:** Puxa alças tangentes de curvatura Bézier em tempo real, gerando curvas suaves e orgânicas!
    * **Linha elástica com prévia de curva:** Enquanto move o cursor, o programa mostra exatamente como a curva vai ficar antes de você clicar.
    * **Fechar caminho:** Clique de volta sobre o primeiro ponto (destacado em verde com anel pulsante) ou aperte `Enter`.
  * **Edição de Nós (`A`):** Clique em qualquer nó das camadas `Simbolo_Asa_Curvas` ou `Detalhe_Interno`. Puxe as alças circulares para ajustar a curvatura, ou dê duplo clique para adicionar nós.

### 5. `05_Cenario_Web_Slices.creatorcanvas` (1920×1080 px)
* **Objetivo:** Fatiar um layout web completo em seções para exportação rápida.
* **Ferramentas para Treinar:**
  * **Fatias para Web (Menu Arquivo > Fatias para Web...):**
  * O Topo e o Rodapé já estão configurados com fatias.
  * Na janela de prévia, use o cursor de mira (`+`) e arraste sobre o **Banner Hero Central** para criar a fatia que falta.
  * Ajuste o tamanho puxando os 4 cantos na prévia e clique em **"Adicionar fatia"**.
  * Clique em **"Exportar todas..."** para gerar todas as fatias salvas em disco de uma vez!

### 6. `06_Cenario_Enquadramento_Crop.creatorcanvas` (1920×1080 px)
* **Objetivo:** Re-enquadrar uma foto panorâmica com proporções fixas e regra dos terços.
* **Ferramentas para Treinar:**
  * **Corte (`C`):** Pressione `C` e mude na barra superior a proporção para **1:1** (Quadrado para Instagram) ou **9:16** (Vertical para Reels/Stories).
  * Posicione a grade sobre o Sol poente e o horizonte.
  * Pressione `Enter` para aplicar o corte ou `Esc` para cancelar.

### 7. `07_Cenario_Tipografia_Efeitos.creatorcanvas` (1080×1350 px - Cartaz Vertical)
* **Objetivo:** Criar cartazes e peças publicitárias com efeitos tipográficos avançados.
* **Ferramentas para Treinar:**
  * **Texto (`T`) & Inspetor de Texto:** Selecione a camada `Titulo_Principal`.
  * **Efeitos de Texto:** Experimente ativar e desativar **Sombra Projetada**, **Contorno (Stroke)** e alterar fontes instaladas.
  * **Alinhamento & Distribuição:** Alinhe os blocos de texto e explore o espaçamento entre linhas.

### 8. `08_Cenario_Gradientes_Composicao.creatorcanvas` (1200×800 px)
* **Objetivo:** Explorar Modos de Mesclagem de camadas (Blend Modes) e sobreposição de iluminação criativa.
* **Ferramentas para Treinar:**
  * **Painel de Camadas:** Selecione `Luz_Magenta (Screen)` ou `Luz_Ciano (Screen)`.
  * **Modos de Mesclagem:** Alterne entre **Normal**, **Screen**, **Multiply**, **Overlay** e **Color Dodge** para ver a fusão das luzes e do fundo instantaneamente.
  * **Opacidade da Camada:** Experimente dosar a intensidade de cada camada com o controle deslizante de opacidade.

---

## Projetos do Pacote Inicial (09 a 12)

Estes quatro projetos são construídos sobre os recursos do **Pacote Inicial**
incluído no programa (`resources/packs/starter/`) e aparecem como cartões na
secção *Cenários de Prática* do ecrã inicial. Todos têm textos de exemplo para
editares — basta selecionar a camada e escrever.

### 9. `09_Cenario_Miniatura_Fundo.creatorcanvas` (1280×720 px)
* **Objetivo:** miniatura de YouTube sobre um fundo do pacote.
* **Ferramentas para Treinar:**
  * **Texto (`T`):** edita o título e a chamada; o contorno está configurado.
  * **Fundos & Texturas (`Ctrl+B`):** troca o fundo por outro do pacote sem recomeçar.
  * **Exportação rápida:** `Ctrl+Shift+E` guarda o PNG num passo.

### 10. `10_Cenario_Ecra_Final.creatorcanvas` (1920×1080 px)
* **Objetivo:** ecrã final de vídeo com duas zonas para vídeos e uma para sugestão.
* **Ferramentas para Treinar:**
  * **Texto (`T`):** substitui o nome do canal e a mensagem.
  * **Importar imagem:** arrasta as tuas miniaturas para cima das duas caixas pretas.

### 11. `11_Cenario_Post_Textura.creatorcanvas` (1080×1350 px)
* **Objetivo:** publicação vertical do Instagram sobre textura de hexágonos.
* **Ferramentas para Treinar:**
  * **Formas:** ajusta o cartão central (cor, opacidade) no Inspetor de Forma.
  * **Fundos & Texturas (`Ctrl+B`):** experimenta outra textura do pacote.

### 12. `12_Cenario_Banner_Brasas.creatorcanvas` (1500×500 px)
* **Objetivo:** banner de canal ou site sobre fundo de brasas.
* **Ferramentas para Treinar:**
  * **Alinhar (`A`):** centraliza o bloco de texto no banner.
  * **Exportação:** experimenta os preset de plataforma no diálogo de exportação.

> Os projectos 09 a 12 são gerados por `tools/generate_pack_samples.cpp`
> (`ninja generate_pack_samples && ./tests/generate_pack_samples`). O teste
> `test_sample_projects` garante que todos os doze abrem, têm camadas e
> miniatura embutida.
