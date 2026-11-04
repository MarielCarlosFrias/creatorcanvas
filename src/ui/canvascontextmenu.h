#pragma once

#include <QPoint>
#include <memory>
#include "core/layers/Layer.h"

class QWidget;
class QMenu;

namespace cc {

class Document;
class CommandStack;
class I18nService;
class CanvasView;

namespace CanvasContextMenu {

/// Constrói o menu de contexto com todas as opções aplicáveis para a camada e estado da seleção
std::unique_ptr<QMenu> buildMenu(QWidget* parent,
                                 Document& document,
                                 const LayerId& layerId,
                                 I18nService* i18n);

/// Constrói, exibe na posição global informada e processa a ação selecionada pelo usuário
bool show(QWidget* parent,
          Document& document,
          CommandStack* history,
          I18nService* i18n,
          const LayerId& layerId,
          const QPoint& globalPos,
          CanvasView* canvas);

} // namespace CanvasContextMenu
} // namespace cc
