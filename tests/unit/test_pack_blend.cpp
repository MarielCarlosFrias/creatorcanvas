#include <QtTest>

#include "services/packstore.h"

using namespace cc;

/// O modo de mesclagem das sobreposicoes vem do pack.json (nao do caminho do
/// ficheiro): um pacote novo pode declara-lo sem tocar em codigo.
class TestPackBlend final : public QObject
{
    Q_OBJECT

private slots:
    void manifestDeclaresBlend()
    {
        const PackItem overlay = PackStore::itemForResource(
            QStringLiteral(":/packs/sobreposicoes/poeira-1.png"));
        QVERIFY(!overlay.isNull());
        QCOMPARE(overlay.blend, QLatin1String("screen"));
        QCOMPARE(overlay.category, QLatin1String("overlay"));

        // Texturas e fundos nao declaram blend (Normal).
        const PackItem texture = PackStore::itemForResource(
            QStringLiteral(":/packs/texturas/papel-pardo.jpg"));
        QVERIFY(!texture.isNull());
        QVERIFY(texture.blend.isEmpty());

        // Recurso inexistente devolve item nulo.
        QVERIFY(PackStore::itemForResource(QStringLiteral(":/packs/nao-existe.png")).isNull());
    }
};

QTEST_MAIN(TestPackBlend)
#include "test_pack_blend.moc"
