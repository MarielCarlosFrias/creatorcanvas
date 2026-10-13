// Garante que TODOS os templates possuem caixas de texto que comportam o
// próprio conteúdo padrão: para cada TextLayer com caixa fixa, a palavra mais
// larga e cada linha explícita precisam caber na largura da caixa (senão o
// texto transborda ou quebra de forma não planejada pelo designer).
#include "core/templates/TemplateFactory.h"
#include "core/Document.h"
#include "core/layers/Layer.h"

#include <QFont>
#include <QFontMetrics>
#include <QtTest>
#include <QGuiApplication>

using namespace cc;

namespace {

void collectTextLayers(const GroupLayer& group, QVector<const TextLayer*>& out)
{
    for (const auto& child : group.children) {
        if (auto* g = dynamic_cast<const GroupLayer*>(child.get()))
            collectTextLayers(*g, out);
        else if (auto* t = dynamic_cast<const TextLayer*>(child.get()))
            out.append(t);
    }
}

} // namespace

class TestTemplateTextFit final : public QObject
{
    Q_OBJECT

private slots:
    void everyTemplateTextBoxFitsItsContent()
    {
        const auto templates = TemplateFactory::availableTemplates();
        QVERIFY(!templates.isEmpty());

        QStringList problems;
        int textLayersChecked = 0;

        for (const auto& meta : templates) {
            auto doc = TemplateFactory::createTemplate(meta.kind);
            QVERIFY(doc != nullptr);

            QVector<const TextLayer*> texts;
            collectTextLayers(*doc->rootGroup(), texts);

            for (const TextLayer* text : texts) {
                if (text->box.isEmpty() || text->content.isEmpty())
                    continue; // auto-size cresce com o conteúdo
                ++textLayersChecked;

                QFont font(text->fontFamily);
                font.setBold(text->bold);
                font.setItalic(text->italic);
                font.setPointSizeF(text->sizePt > 0 ? text->sizePt : 1.0);
                if (text->letterSpacingPx != 0.0)
                    font.setLetterSpacing(QFont::AbsoluteSpacing,
                                          text->letterSpacingPx);
                const QFontMetrics metrics(font);

                const QStringList lines = text->content.split(QLatin1Char('\n'));
                // Linhas explicitas (conteudo com \n) sao intencao do designer
                // e devem caber; frase unica sem \n quebra por design (wrap).
                const bool hasExplicitLines = text->content.contains(QLatin1Char('\n'));
                for (const QString& line : lines) {
                    // Palavra mais larga que a caixa transborda sempre.
                    const QStringList words =
                        line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    for (const QString& word : words) {
                        const double w = metrics.horizontalAdvance(word);
                        if (w > text->box.width()) {
                            problems << QStringLiteral(
                                "%1: '%2' palavra '%3' (%4px) > box %5x%6")
                                .arg(meta.defaultTitle, text->name, word)
                                .arg(w, 0, 'f', 0)
                                .arg(text->box.width(), 0, 'f', 0)
                                .arg(text->box.height(), 0, 'f', 0);
                        }
                    }
                    // Linha explícita (\n) mais larga que a caixa quebra sem
                    // controle — o designer deveria dimensionar a caixa.
                    // Frase única sem \n quebra por design (wrap) — não falha.
                    if (!hasExplicitLines)
                        continue;
                    const double lw = metrics.horizontalAdvance(line);
                    if (lw > text->box.width()) {
                        problems << QStringLiteral(
                            "%1: '%2' linha '%3' (%4px) > box %5x%6 (quebrará)")
                            .arg(meta.defaultTitle, text->name, line)
                            .arg(lw, 0, 'f', 0)
                            .arg(text->box.width(), 0, 'f', 0)
                            .arg(text->box.height(), 0, 'f', 0);
                    }
                }
            }
        }

        // Sanity: o teste precisa ter verificado uma quantidade relevante
        QVERIFY(textLayersChecked >= 10);
        if (!problems.isEmpty())
            QFAIL(qPrintable(QStringLiteral(
                "Templates com texto maior que a própria caixa:\n  ")
                + problems.join(QStringLiteral("\n  "))));
    }
};

QTEST_MAIN(TestTemplateTextFit)
#include "test_template_textfit.moc"
