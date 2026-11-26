#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QImage>

#include "core/Document.h"
#include "core/layers/Layer.h"
#include "core/serialization/ProjectFile.h"

using namespace cc;

namespace {
// O directório dos exemplos vive na raiz do projecto: CC_PROJECT_ROOT e
// definido pelo helper cc_add_test (o cwd dos testes e a arvore de build).
QString samplesDir()
{
    const QStringList candidates = {
        QStringLiteral(CC_PROJECT_ROOT) + QStringLiteral("/exemplos_projetos"),
        QStringLiteral("exemplos_projetos"),
        QStringLiteral("../exemplos_projetos"),
        QStringLiteral("../../exemplos_projetos"),
    };
    for (const QString& dir : candidates) {
        if (QDir(dir).exists())
            return dir;
    }
    return {};
}
} // namespace

/// Garante que todos os projectos de exemplo (os 8 originais e os 4 do
/// Pacote Inicial) abrem, têm camadas e uma miniatura embedada.
class TestSampleProjects final : public QObject
{
    Q_OBJECT

private slots:
    void everyProjectLoads()
    {
        const QString dir = samplesDir();
        QVERIFY2(!dir.isEmpty(), "pasta exemplos_projetos nao encontrada");

        const QStringList expected = {
            QStringLiteral("01_Cenario_Banner_Social.creatorcanvas"),
            QStringLiteral("02_Cenario_Retoque_Foto.creatorcanvas"),
            QStringLiteral("03_Cenario_Recorte_Objeto.creatorcanvas"),
            QStringLiteral("04_Cenario_Vetor_Logotipo.creatorcanvas"),
            QStringLiteral("05_Cenario_Web_Slices.creatorcanvas"),
            QStringLiteral("06_Cenario_Enquadramento_Crop.creatorcanvas"),
            QStringLiteral("07_Cenario_Tipografia_Efeitos.creatorcanvas"),
            QStringLiteral("08_Cenario_Gradientes_Composicao.creatorcanvas"),
            QStringLiteral("09_Cenario_Miniatura_Fundo.creatorcanvas"),
            QStringLiteral("10_Cenario_Ecra_Final.creatorcanvas"),
            QStringLiteral("11_Cenario_Post_Textura.creatorcanvas"),
            QStringLiteral("12_Cenario_Banner_Brasas.creatorcanvas"),
        };

        for (const QString& name : expected) {
            const QString path = QDir(dir).filePath(name);
            QVERIFY2(QFileInfo::exists(path), qPrintable(name));

            QString error;
            auto doc = cc::loadDocument(path, &error);
            QVERIFY2(doc != nullptr, qPrintable(name + ": " + error));
            QVERIFY2(!doc->rootGroup()->children.empty(),
                     qPrintable(name + ": sem camadas"));

            const QImage thumb = cc::loadProjectThumbnail(path);
            QVERIFY2(!thumb.isNull(), qPrintable(name + ": sem miniatura"));
        }
    }
};

QTEST_MAIN(TestSampleProjects)
#include "test_sample_projects.moc"
