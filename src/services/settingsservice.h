#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVariant>

#include "localization/languagepreferences.h"

class QSettings;

namespace cc {

/// Typed wrapper over persistent application settings (INI format).
///
/// When constructed without |filePath|, QSettings resolves the platform
/// location from the organisation/application names that main() sets BEFORE
/// constructing this service. Tests inject an explicit file path.
class SettingsService final : public QObject, public LanguagePreferenceStore
{
    Q_OBJECT
public:
    explicit SettingsService(const QString &filePath = {},
                             QObject *parent = nullptr);

    // General -------------------------------------------------------------
    QString language() const override;              // BCP-47-ish code: "en", "pt-BR"
    void setLanguage(const QString &code) override; // English is the default/fallback

    int autosaveIntervalMinutes() const;   // 0 = autosave disabled; default 5
    void setAutosaveIntervalMinutes(int minutes);

    int historyLimit() const;
    void setHistoryLimit(int limit);

    /// Tecto de memoria do histórico em MB (0 = sem limite por memória).
    /// Omissao: 512 MB (~10 snapshots de 4000x3000).
    int historyMemoryBudgetMb() const;
    void setHistoryMemoryBudgetMb(int mb);

    // Performance ----------------------------------------------------------
    bool gpuAccelerationEnabled() const;   // default false (software renderer)
    void setGpuAccelerationEnabled(bool enabled);

    bool darkModeEnabled() const;
    void setDarkModeEnabled(bool enabled);
    bool confirmBeforeExit() const;
    void setConfirmBeforeExit(bool enabled);
    bool showGridByDefault() const;
    void setShowGridByDefault(bool enabled);
    bool snapToGridByDefault() const;
    void setSnapToGridByDefault(bool enabled);

    /// Barra de dicas para iniciantes ("prima Ctrl+K..."): uma vez fechada
    /// pelo utilizador, não volta a aparecer.
    bool hintsBarDismissed() const;
    void setHintsBarDismissed(bool dismissed);

    /// Diálogo de boas-vindas (idioma) e tutorial guiado: aparecem uma vez.
    bool welcomeShown() const;
    void setWelcomeShown(bool shown);
    bool tourCompleted() const;
    void setTourCompleted(bool completed);

    /// Usado pelos "Primeiros passos": o utilizador já exportou alguma vez.
    bool hasExported() const;
    void setHasExported(bool exported);

    /// Painel "Primeiros passos": uma vez fechado, não volta a aparecer.
    bool onboardingDismissed() const;
    void setOnboardingDismissed(bool dismissed);

    /// Última pasta usada numa exportação (diálogos de ficheiro).
    QString lastExportDir() const;
    void setLastExportDir(const QString& dir);

    /// Modo avançado: esconde as ferramentas profissionais (histograma,
    /// edição de nós, refine edge, gradientes multi-stop, texto em
    /// caminho, ajustes no inspector). A omissão é DESLIGADO — quem
    /// começa vê o essencial; quem procura precisão liga uma vez e fica
    /// gravado.
    bool advancedModeEnabled() const;
    void setAdvancedModeEnabled(bool enabled);

    // Window layout / workspace ---------------------------------------------
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);
    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);

signals:
    /// Emitted once after any value actually changes.
    void changed();

private:
    QVariant value(const QString &key, const QVariant &fallback) const;
    void setValue(const QString &key, const QVariant &value);

    QSettings *m_settings = nullptr;
};

} // namespace cc
