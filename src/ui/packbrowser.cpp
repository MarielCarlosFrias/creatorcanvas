#include "ui/packbrowser.h"

#include "localization/i18nservice.h"
#include "services/packstore.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QShowEvent>

namespace cc {

namespace {
constexpr int kThumbWidth = 220;
}

PackBrowser::PackBrowser(I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_i18n(i18n)
{
    setObjectName(QStringLiteral("PackBrowser"));
    setMinimumSize(620, 460);
    resize(760, 560);

    auto* layout = new QVBoxLayout(this);

    auto* filterRow = new QHBoxLayout();
    m_category = new QComboBox(this);
    filterRow->addWidget(m_category);
    m_query = new QLineEdit(this);
    m_query->setClearButtonEnabled(true);
    m_query->setPlaceholderText(
        m_i18n ? m_i18n->t("editor", "pack.browser.search")
               : QStringLiteral("Search resources..."));
    filterRow->addWidget(m_query, 1);
    layout->addLayout(filterRow);

    m_grid = new QListWidget(this);
    m_grid->setViewMode(QListWidget::IconMode);
    m_grid->setResizeMode(QListWidget::Adjust);
    m_grid->setMovement(QListWidget::Static);
    m_grid->setSpacing(8);
    m_grid->setUniformItemSizes(true);
    m_grid->setWordWrap(true);
    // Celula fixa: miniaturas de 150 px + rotulo em duas linhas, para os
    // nomes nao ficarem cortados em nenhum tamanho de janela.
    m_grid->setIconSize(QSize(150, 150));
    m_grid->setGridSize(QSize(158, 196));
    layout->addWidget(m_grid, 1);

    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_category, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PackBrowser::onCategoryChanged);
    connect(m_query, &QLineEdit::textChanged,
            this, &PackBrowser::onQueryChanged);
    // APENAS itemActivated: o duplo-clique emite itemActivated E
    // itemDoubleClicked, e ligar os dois criava DUAS camadas por escolha.
    connect(m_grid, &QListWidget::itemActivated,
            this, &PackBrowser::onItemActivated);

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &PackBrowser::retranslateUi);
    retranslateUi();
}

void PackBrowser::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    // Reaberto depois de uma escolha: a guarda de activação repõe-se.
    m_activated = false;
}

void PackBrowser::retranslateUi()
{
    if (!m_i18n)
        return;
    setWindowTitle(m_i18n->t("editor", "pack.browser.title"));

    // O filtro e construido a partir das categorias realmente presentes nos
    // manifestos: um pacote novo com uma categoria nova aparece sozinho.
    const QString current = m_category->count() > 0
        ? m_category->currentData().toString() : QString();
    m_category->clear();
    m_category->addItem(m_i18n->t("editor", "pack.category.all"), QString());
    for (const QString& category : PackStore::categories()) {
        const QString label = m_i18n->t("editor",
                                        QStringLiteral("pack.category.") + category);
        m_category->addItem(label, category);
    }
    const int restored = m_category->findData(current);
    if (restored >= 0)
        m_category->setCurrentIndex(restored);

    if (m_emptyLabel)
        m_emptyLabel->setText(m_i18n->t("editor", "pack.browser.empty"));
    if (m_query)
        m_query->setPlaceholderText(m_i18n->t("editor", "pack.browser.search"));
    rebuild();
}

void PackBrowser::rebuild()
{
    if (!m_grid)
        return;
    m_grid->clear();

    const QString category = m_category->currentData().toString();
    const QString query = m_query ? m_query->text().trimmed().toLower() : QString();
    const QVector<PackItem> items = PackStore::allItems();
    for (const PackItem& item : items) {
        if (!category.isEmpty() && item.category != category)
            continue;
        if (!query.isEmpty()) {
            const QString label = item.label(m_i18n).toLower();
            const QString key = item.labelKey.toLower();
            if (!label.contains(query) && !key.contains(query))
                continue;
        }

        const QSize natural = item.naturalSize();
        QPixmap thumb;
        if (natural.isValid()) {
            QPixmap loaded(item.resource);
            loaded = loaded.scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (item.category == QLatin1String("overlay")) {
                // Sobreposicoes tem alfa: sem fundo parecem um quadrado
                // vazio. Compõe sobre cinza medio para o padrao ser visivel
                // (e o utilizador percebe que e uma camada por cima).
                QPixmap flat(loaded.size());
                flat.fill(QColor(0x80, 0x86, 0x90));
                QPainter composer(&flat);
                composer.drawPixmap(0, 0, loaded);
                composer.end();
                thumb = flat;
            } else {
                thumb = loaded;
            }
        }

        auto* row = new QListWidgetItem(item.label(m_i18n));
        row->setIcon(QIcon(thumb));
        row->setData(Qt::UserRole, item.resource);
        row->setData(Qt::UserRole + 1, natural);
        row->setTextAlignment(Qt::AlignCenter);
        m_grid->addItem(row);
    }
    if (m_emptyLabel)
        m_emptyLabel->setVisible(m_grid->count() == 0);
}

void PackBrowser::onCategoryChanged(int)
{
    rebuild();
}

void PackBrowser::onQueryChanged(const QString&)
{
    rebuild();
}

void PackBrowser::onItemActivated(QListWidgetItem* item)
{
    if (!item)
        return;
    // Segunda activação (Enter a seguir a um duplo clique, por exemplo) não
    // pode voltar a emitir: criaria uma camada repetida.
    if (m_activated)
        return;
    m_activated = true;
    const QString resource = item->data(Qt::UserRole).toString();
    const QSize natural = item->data(Qt::UserRole + 1).toSize();
    emit itemChosen(resource, natural);
    accept();
}

} // namespace cc
