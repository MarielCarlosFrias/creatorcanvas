#include "services/textstylestore.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace cc;

// O recurso de presets vive na lib estática cc_services: precisa de init
// explícito (a app faz o mesmo para locales em main.cpp).
static struct PresetsResourceInit
{
    PresetsResourceInit() { Q_INIT_RESOURCE(presets); }
} s_presetsResourceInit;

namespace {

TextStyle makeStyle(const QString& name, const QColor& color)
{
    TextStyle s;
    s.name = name;
    s.fontFamily = QStringLiteral("Test Font");
    s.sizePt = 72;
    s.bold = true;
    s.italic = false;
    s.color = color;
    s.letterSpacingPx = 2.5;
    s.lineHeightMult = 1.15;
    s.effects.outline.enabled = true;
    s.effects.outline.color = QColor(10, 20, 30);
    s.effects.outline.width = 6;
    s.effects.shadow.enabled = true;
    s.effects.shadow.color = QColor(40, 50, 60);
    s.effects.shadow.offsetX = 3;
    s.effects.shadow.offsetY = 4;
    s.effects.shadow.blur = 5;
    s.effects.gradient.enabled = true;
    s.effects.gradient.type = 1;
    s.effects.gradient.startColor = QColor(1, 2, 3);
    s.effects.gradient.endColor = QColor(4, 5, 6);
    s.effects.gradient.angleDeg = 33;
    return s;
}

bool stylesEqual(const TextStyle& a, const TextStyle& b)
{
    return a.name == b.name && a.fontFamily == b.fontFamily
        && qFuzzyCompare(a.sizePt, b.sizePt) && a.bold == b.bold
        && a.italic == b.italic && a.color == b.color
        && qFuzzyCompare(a.letterSpacingPx, b.letterSpacingPx)
        && qFuzzyCompare(a.lineHeightMult, b.lineHeightMult)
        && a.effects == b.effects;
}

} // namespace

class TestTextStyles final : public QObject
{
    Q_OBJECT

private slots:
    void builtInsAndUserStyles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("text_styles.json"));

        TextStyleStore store(path);
        // Built-ins embarcidos (recurso :/presets/text_styles.json).
        QVERIFY2(store.all().size() >= 4,
                 qPrintable(QStringLiteral("built-ins: %1").arg(store.all().size())));
        QVERIFY(store.find(QStringLiteral("Título YouTube")).has_value());
        QVERIFY(!store.isUserStyle(QStringLiteral("Título YouTube")));

        // Adicionar um estilo do utilizador persiste.
        const TextStyle mine = makeStyle("Meu Estilo", QColor(200, 100, 50));
        QVERIFY(store.addUserStyle(mine));
        QVERIFY(store.isUserStyle("Meu Estilo"));
        QCOMPARE(store.all().size(), 5u);

        // Reabrir: o estilo do utilizador continua lá, com tudo igual.
        TextStyleStore reopened(path);
        QCOMPARE(reopened.all().size(), 5u);
        const auto found = reopened.find(QStringLiteral("Meu Estilo"));
        QVERIFY(found.has_value());
        QVERIFY2(stylesEqual(*found, mine),
                 "round-trip do estilo perdeu dados (efeitos, gradiente ou medições)");

        // Nome duplicado é recusado.
        QVERIFY(!reopened.addUserStyle(makeStyle("Meu Estilo", Qt::red)));
        // Nome vazio é recusado.
        TextStyle empty = makeStyle("   ", Qt::red);
        QVERIFY(!reopened.addUserStyle(empty));

        // Built-in não se remove; o do utilizador sim.
        QVERIFY(!reopened.removeUserStyle(QStringLiteral("Título YouTube")));
        QVERIFY(reopened.removeUserStyle(QStringLiteral("Meu Estilo")));
        QCOMPARE(reopened.all().size(), 4u);
        TextStyleStore afterRemoval(path);
        QCOMPARE(afterRemoval.all().size(), 4u);
        QVERIFY(!afterRemoval.find(QStringLiteral("Meu Estilo")).has_value());
    }

    void missingUserFileMeansBuiltInsOnly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        TextStyleStore store(dir.filePath(QStringLiteral("nao_existe.json")));
        QVERIFY(store.all().size() >= 4);
        QVERIFY(!store.isUserStyle(store.all().first().name));
    }
};

QTEST_GUILESS_MAIN(TestTextStyles)
#include "test_textstyles.moc"
