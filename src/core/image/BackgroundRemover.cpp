#include "BackgroundRemover.h"
#include <onnxruntime_cxx_api.h>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <vector>

namespace cc {

Q_LOGGING_CATEGORY(lcBgRemover, "cc.ai.bgremover")

static constexpr float MEAN[3] = {0.485f, 0.456f, 0.406f};
static constexpr float STD[3]  = {0.229f, 0.224f, 0.225f};
static constexpr int INPUT_SIZE = 320;

class BackgroundRemoverImpl
{
public:
    explicit BackgroundRemoverImpl(const std::string& modelPath)
        : m_env(ORT_LOGGING_LEVEL_WARNING, "CreatorCanvas_AiBgRemover"),
          m_session(nullptr)
    {
        Ort::SessionOptions sessOpts;
        sessOpts.SetIntraOpNumThreads(2);
        sessOpts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        std::wstring wpath(modelPath.begin(), modelPath.end());
        m_session = Ort::Session(m_env, wpath.c_str(), sessOpts);
#else
        m_session = Ort::Session(m_env, modelPath.c_str(), sessOpts);
#endif

        Ort::AllocatorWithDefaultOptions allocator;
        Ort::AllocatedStringPtr inPtr  = m_session.GetInputNameAllocated(0, allocator);
        Ort::AllocatedStringPtr outPtr = m_session.GetOutputNameAllocated(0, allocator);
        m_inputName  = std::string(inPtr.get());
        m_outputName = std::string(outPtr.get());
    }

    QImage process(const QImage& input, const std::atomic<bool>* cancelFlag)
    {
        if (cancelFlag && cancelFlag->load())
            return QImage();

        std::vector<float> blob = buildInputBlob(input);
        if (cancelFlag && cancelFlag->load())
            return QImage();

        std::vector<int64_t> inputShape = {1, 3, INPUT_SIZE, INPUT_SIZE};

        Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memInfo, blob.data(), blob.size(),
            inputShape.data(), inputShape.size());

        const char* inName  = m_inputName.c_str();
        const char* outName = m_outputName.c_str();

        if (cancelFlag && cancelFlag->load())
            return QImage();

        auto outputs = m_session.Run(
            Ort::RunOptions{nullptr},
            &inName, &inputTensor, 1,
            &outName, 1);

        if (cancelFlag && cancelFlag->load())
            return QImage();

        float* maskPtr = outputs[0].GetTensorMutableData<float>();
        QImage alpha = buildAlphaMask(maskPtr, input.width(), input.height());

        if (cancelFlag && cancelFlag->load())
            return QImage();

        return composite(input, alpha);
    }

private:
    std::vector<float> buildInputBlob(const QImage& img) const
    {
        QImage rgb = img.convertToFormat(QImage::Format_RGB32);
        QImage resized = rgb.scaled(
            INPUT_SIZE, INPUT_SIZE,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation);

        int plane = INPUT_SIZE * INPUT_SIZE;
        std::vector<float> blob(1UL * 3 * plane);

        for (int y = 0; y < INPUT_SIZE; ++y) {
            for (int x = 0; x < INPUT_SIZE; ++x) {
                QRgb px = resized.pixel(x, y);
                float r = qRed(px) / 255.0f;
                float g = qGreen(px) / 255.0f;
                float b = qBlue(px) / 255.0f;

                int idx = y * INPUT_SIZE + x;
                blob[0 * plane + idx] = (r - MEAN[0]) / STD[0];
                blob[1 * plane + idx] = (g - MEAN[1]) / STD[1];
                blob[2 * plane + idx] = (b - MEAN[2]) / STD[2];
            }
        }
        return blob;
    }

    QImage buildAlphaMask(const float* mask, int origW, int origH) const
    {
        float minV = mask[0];
        float maxV = mask[0];
        for (int i = 1; i < INPUT_SIZE * INPUT_SIZE; ++i) {
            if (mask[i] < minV) minV = mask[i];
            if (mask[i] > maxV) maxV = mask[i];
        }
        float range = maxV - minV;
        if (range < 1e-8f) range = 1.0f;

        QImage maskImg(INPUT_SIZE, INPUT_SIZE, QImage::Format_Grayscale8);
        for (int y = 0; y < INPUT_SIZE; ++y) {
            quint8* row = maskImg.scanLine(y);
            for (int x = 0; x < INPUT_SIZE; ++x) {
                float val = (mask[y * INPUT_SIZE + x] - minV) / range * 255.0f;
                row[x] = static_cast<quint8>(std::clamp(val, 0.0f, 255.0f));
            }
        }
        return maskImg.scaled(
            origW, origH,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation);
    }

    QImage composite(const QImage& img, const QImage& alpha) const
    {
        QImage result = img.convertToFormat(QImage::Format_RGBA8888);
        for (int y = 0; y < result.height(); ++y) {
            const QRgb* rgbRow = reinterpret_cast<const QRgb*>(result.constScanLine(y));
            const quint8* alphaRow = alpha.constScanLine(y);
            QRgb* outRow = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < result.width(); ++x) {
                QRgb rgb = rgbRow[x];
                quint8 a = alphaRow[x];
                outRow[x] = qRgba(qRed(rgb), qGreen(rgb), qBlue(rgb), a);
            }
        }
        return result;
    }

    Ort::Env m_env;
    Ort::Session m_session;
    std::string m_inputName;
    std::string m_outputName;
};

BackgroundRemover::BackgroundRemover(const QString& modelPath)
{
    QString path = modelPath;
    if (path.isEmpty()) {
        path = findModelPath();
    }
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        try {
            m_impl = std::make_unique<BackgroundRemoverImpl>(path.toStdString());
        } catch (const std::exception& e) {
            qCWarning(lcBgRemover) << "Erro ao carregar ONNX:" << e.what();
            m_impl.reset();
        }
    }
}

BackgroundRemover::~BackgroundRemover() = default;

bool BackgroundRemover::isLoaded() const
{
    return m_impl != nullptr;
}

QImage BackgroundRemover::removeBackground(const QImage& input, const std::atomic<bool>* cancelFlag)
{
    if (!m_impl || input.isNull())
        return input;
    if (cancelFlag && cancelFlag->load())
        return QImage();
    try {
        QImage result = m_impl->process(input, cancelFlag);
        if (result.isNull() && cancelFlag && cancelFlag->load())
            return QImage();
        return result.isNull() ? input : result;
    } catch (const std::exception& e) {
        qCWarning(lcBgRemover) << "Erro durante inferência ONNX:" << e.what();
        return input;
    }
}

QString BackgroundRemover::findModelPath(const QString& preferredName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    const QStringList candidateDirs = {
        appDir + QStringLiteral("/../share/creatorcanvas/models"),
        appDir + QStringLiteral("/share/creatorcanvas/models"),
        appDir + QStringLiteral("/models"),
        appDir,
        appDataDir + QStringLiteral("/models"),
        appDataDir
    };

    for (const QString& dir : candidateDirs) {
        const QString full = dir + QStringLiteral("/") + preferredName;
        if (QFileInfo::exists(full))
            return full;
    }

    // Fallback: search for any *.onnx in candidate directories
    for (const QString& dir : candidateDirs) {
        QDir qdir(dir);
        if (qdir.exists()) {
            const QStringList list = qdir.entryList({QStringLiteral("*.onnx")}, QDir::Files);
            if (!list.isEmpty()) {
                return qdir.absoluteFilePath(list.first());
            }
        }
    }

    return QString();
}

bool BackgroundRemover::isAvailable()
{
    return !findModelPath().isEmpty();
}

} // namespace cc
