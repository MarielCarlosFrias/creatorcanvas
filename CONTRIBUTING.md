# Contributing to CreatorCanvas

Obrigado por dedicar um tempo para olhar o CreatorCanvas! Este é um projeto jovem, orientado por marcos (milestones). Leia o [`DOCUMENTATION.md`](DOCUMENTATION.md) primeiro para entender a arquitetura e o estado atual antes de enviar uma mudança.

Por favor, leia e siga o nosso [Código de Conduta](CODE_OF_CONDUCT.md) em todas as interações.

---

## 📋 Como começar

1. **Leia a documentação**: [`DOCUMENTATION.md`](DOCUMENTATION.md) (arquitetura, modelo de dados, roadmap) e este arquivo.
2. **Abra uma issue primeiro**: para funcionalidades grandes ou mudanças de arquitetura, abra uma issue no GitHub e discuta antes de começar a implementar.
3. **Faça o fork do repositório**: crie um fork, clone localmente e crie uma branch a partir de `main`:

   ```bash
   git checkout -b feat/minha-mudanca
   # ou
   git checkout -b fix/minha-correcao
   ```

4. **Configure o ambiente de build**: veja a seção "Build" no [`README.md`](README.md#-building-from-source).
5. **Rode os testes antes de começar**:

   ```bash
   ctest --test-dir build/linux-debug --output-on-failure
   ```

   Todos os 31 testes devem passar em um checkout limpo.

---

## 📜 Regras de ouro

1. **Toda mutação de documento vai através de um `Command`** (`src/core/history/`). Se seu código de UI toca no estado do `Document`, adicione ou estenda um comando em vez de chamar um setter diretamente — isso mantém o desfazer/refazer correto.
2. **Não hardcode strings visíveis ao usuário.** Use `I18nService::t(namespace, key)` e adicione a chave a **ambos** `resources/locales/en/` e `resources/locales/pt-BR/` no mesmo PR. (Ao adicionar um novo idioma, veja "Adicionando ou atualizando uma tradução" abaixo.)
3. **`core/` não depende de QtWidgets.** Apenas QtCore/QtGui. Isso permite que a lógica central seja testada sem display (`QT_QPA_PLATFORM=offscreen`).
4. **Prefira `QStandardPaths` / `QDir` sobre `#ifdef`.** Não existe `src/platform/` ainda. Ramificações específicas de SO devem ficar no CMake ou em um helper claramente isolado — não espalhe `#ifdef _WIN32` pelo UI/core.
5. **`std::unique_ptr<T>` membros de uma classe `Q_OBJECT` precisam de um `#include` completo de `T`**, não uma declaração antecipada — o destruidor gerado pelo compilador precisa do tipo completo. Isto já quebrou o build mais de uma vez (M12, M13); verifique duas vezes ao adicionar um novo membro `unique_ptr` a um cabeçalho.
6. **Qualifique funções livres `cc::` explicitamente** ao chamá-las dentro de uma classe que define um membro com o mesmo nome (ex. `cc::saveDocument(...)` vs `MainWindow::saveDocument()`), senão o membro sombreará silenciosamente a função livre.
7. **Evite reescritas amplas com `sed`/regex em arquivos-fonte.** Prefira substituições de strings pequenas, exatas e revisadas, para que um patch aplique exatamente onde pretendido ou falhe alto — não um quase-acerto que toque silenciosamente na ocorrência errada.

---

## 🌍 Adicionando ou atualizando uma tradução

1. Comece a partir de `main`.
2. Copie `resources/locales/en/` para `resources/locales/<código>/`.
3. Traduza apenas os valores; mantenha todas as chaves JSON inalteradas.
4. Preencha o bloco `_meta` em `common.json`:

   ```json
   "_meta": { "nativeName": "Svenska", "englishName": "Swedish" }
   ```

5. Preserve placeholders positionais como `%1` e `%2`. Eles podem ser reordenados para caber no idioma de destino. CreatorCanvas não tem sistema de pluralização.
6. Adicione cada novo arquivo JSON a `resources/locales.qrc`.
7. Rode a suíte de testes de i18n:

   ```bash
   ctest --test-dir build/linux-debug --output-on-failure
   ```

8. Abra um pull request. Contribuições de tradução são bem-vindas.

Por exemplo, um valor como `"language": "Language: %1"` deve reter `%1` na tradução.

---

## 🔄 Fluxo de trabalho do PR

1. Construa e rode a suíte de testes existente antes de começar (`ctest --test-dir build/linux-debug --output-on-failure`) — todos os testes registrados em `tests/CMakeLists.txt` devem passar em um checkout limpo.
2. Faça sua mudança com o menor escopo razoável. Se for uma nova funcionalidade, verifique `DOCUMENTATION.md` → Roadmap primeiro — alguns itens listados lá já estão em `main` (multiseleção, templates, ferramentas raster extras); outros estão adiados de propósito.
3. Adicione ou atualize um teste unitário em `tests/unit/` para qualquer mudança de comportamento em `core/`, `rendering/` ou `services/`.
4. Rode o build com sanitizador se tocou código sensível a tempo de vida/propriedade:

   ```bash
   cmake --preset linux-asan && cmake --build --preset linux-asan
   ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/linux-asan --output-on-failure
   ```

5. Mantenha mensagens de commit marcadas por milestone/escopo quando relevante (o histórico existente usa `M<n>: <o que foi entregue>` — siga esse padrão para consistência, ou use Conventional Commits (`feat:`, `fix:`, `docs:`, etc.) para qualquer coisa fora do track de marcos).
6. Abra o pull request contra `main`. Preencha o template de PR (`.github/PULL_REQUEST_TEMPLATE.md`).

### Checklist antes de enviar o PR

- [ ] Li o `CONTRIBUTING.md` e o `DOCUMENTATION.md`
- [ ] Todos os testes existentes passam (`ctest --output-on-failure`)
- [ ] Adicionei ou atualizei testes para a mudança (se aplicável)
- [ ] Não há strings de usuário hardcoded — tudo vai via `I18nService::t()` para `en` e `pt-BR`
- [ ] `core/` não depende de QtWidgets (apenas QtCore/QtGui)
- [ ] Commit segue o padrão (`M<n>: ...` ou Conventional Commits)
- [ ] Testado em: [Linux / Windows / outro]

---

## 🐛 Reportando bugs

Inclua:
- Seu SO/compilador/versão do Qt
- O `cmake --preset` que você usou
- O erro exato do compilador ou comportamento em tempo de execução
- Se for um crash, um trecho do log (os logs ficam no diretório de dados da aplicação, veja `DOCUMENTATION.md` → Conventions)

Use o template de "Bug report" em `.github/ISSUE_TEMPLATE/bug_report.md`.

---

## ✏️ Solicitando novas funcionalidades

Use o template de "Feature request" em `.github/ISSUE_TEMPLATE/feature_request.md`. Descreva o caso de uso, por que é útil e como você imagina que funcione.
