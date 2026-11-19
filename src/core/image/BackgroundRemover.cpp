#include "BackgroundRemover.h"
#include <onnxruntime_cxx_api.h>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

namespace cc {

Q_LOGGING_CATEGORY(lcBgRemover, "cc.ai.bgremover")

static constexpr float MEAN[3] = {0.485f, 0.456f, 0.406f};
static constexpr float STD[3]  = {0.229f, 0.224f, 0.225f};

namespace {

// Blur caixa 3x3 em imagem Grayscale8 (suaviza a borda do recorte).
void boxBlurGray(QImage& img, int passes)
{
    for (int p = 0; p < passes; ++p) {
        const QImage src = img.copy();
        const int w = img.width(), h = img.height();
        for (int y = 0; y < h; ++y) {
            quint8* out = img.scanLine(y);
            for (int x = 0; x < w; ++x) {
                int sum = 0, count = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    const int sy = y + dy;
                    if (sy < 0 || sy >= h) continue;
                    const quint8* in = src.constScanLine(sy);
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int sx = x + dx;
                        if (sx < 0 || sx >= w) continue;
                        sum += in[sx];
                        ++count;
                    }
                }
                out[x] = static_cast<quint8>(sum / count);
            }
        }
    }
}

// Marca o fundo conectado as bordas (flood fill iterativo). Pixels do sujeito
// sao os que NAO sao alcancados a partir da moldura da imagem.
std::vector<quint8> backgroundFromBorders(const QImage& bin)
{
    const int w = bin.width(), h = bin.height();
    std::vector<quint8> background(static_cast<size_t>(w) * h, 0);
    std::vector<qint32> stack;
    auto push = [&](int x, int y) {
        const int i = y * w + x;
        if (!background[i] && bin.constScanLine(y)[x] < 128) {
            background[i] = 1;
            stack.push_back(i);
        }
    };
    for (int x = 0; x < w; ++x) { push(x, 0); push(x, h - 1); }
    for (int y = 0; y < h; ++y) { push(0, y); push(w - 1, y); }
    while (!stack.empty()) {
        const int i = stack.back();
        stack.pop_back();
        const int x = i % w, y = i / w;
        if (x > 0) push(x - 1, y);
        if (x < w - 1) push(x + 1, y);
        if (y > 0) push(x, y - 1);
        if (y < h - 1) push(x, y + 1);
    }
    return background;
}

QImage composite(const QImage& img, const QImage& alpha)
{
    QImage result = img.convertToFormat(QImage::Format_RGBA8888);
    // Endurecimento anti-detach: se a entrada já era RGBA8888 a conversão
    // devolve cópia rasa; o scanLine() de escrita destacaria o buffer no
    // meio do loop deixando rgbRow apontando para o bloco antigo.
    if (result.constBits() == img.constBits())
        result = result.copy();
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

} // namespace

class BackgroundRemoverImpl
{
public:
    explicit BackgroundRemoverImpl(const std::string& modelPath)
        : m_env(ORT_LOGGING_LEVEL_WARNING, "CreatorCanvas_AiBgRemover"),
          m_session(nullptr)
    {
        Ort::SessionOptions sessOpts;
        sessOpts.SetIntraOpNumThreads(std::max(1u, std::thread::hardware_concurrency()));
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
        m_inputSize  = detectInputSize(modelPath);
    }

    QImage process(const QImage& input, const std::atomic<bool>* cancelFlag,
                   const MatteOptions& options, QImage* rawAlphaOut)
    {
        if (cancelFlag && cancelFlag->load())
            return QImage();

        std::vector<float> blob = buildInputBlob(input);
        if (cancelFlag && cancelFlag->load())
            return QImage();

        std::vector<int64_t> inputShape = {1, 3, m_inputSize, m_inputSize};

        Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memInfo, blob.data(), blob.size(),
            inputShape.data(), inputShape.size());

        const char* inName  = m_inputName.c_str();
        const char* outName = m_outputName.c_str();

        if (cancelFlag && cancelFlag->load())
            return QImage();

        // Cancelamento REAL da inferência: RunOptions::SetTerminate é
        // seguro de outra thread e interrompe o Run da ONNX no ato. O
        // watchdog pola o flag enquanto o Run bloqueia o worker.
        Ort::RunOptions runOpts;
        std::atomic<bool> runDone{false};
        std::thread watchdog([&runOpts, cancelFlag, &runDone]() {
            while (!runDone.load()) {
                if (cancelFlag && cancelFlag->load()) {
                    runOpts.SetTerminate();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        struct WatchdogJoiner {
            std::thread& t;
            std::atomic<bool>& done;
            ~WatchdogJoiner() { done.store(true); t.join(); }
        } joiner{watchdog, runDone};

        auto outputs = m_session.Run(
            runOpts,
            &inName, &inputTensor, 1,
            &outName, 1);

        if (cancelFlag && cancelFlag->load())
            return QImage();

        float* maskPtr = outputs[0].GetTensorMutableData<float>();
        QImage alpha = buildAlphaMask(maskPtr, input.width(), input.height());

        if (cancelFlag && cancelFlag->load())
            return QImage();

        if (rawAlphaOut)
            *rawAlphaOut = alpha;

        return BackgroundRemover::applyMatte(input, alpha, options, cancelFlag);
    }

private:
    int detectInputSize(const std::string& modelPath) const
    {
        try {
            const std::vector<int64_t> shape =
                m_session.GetInputTypeInfo(0)
                    .GetTensorTypeAndShapeInfo()
                    .GetShape();
            if (shape.size() == 4 && shape[2] > 0 && shape[3] > 0) {
                if (shape[2] == shape[3]) {
                    return static_cast<int>(shape[2]);
                }
            }
        } catch (...) {
        }
        const QString name = QFileInfo(QString::fromStdString(modelPath)).fileName().toLower();
        if (name.contains(QStringLiteral("isnet"))) {
            return 1024;
        }
        return 320;
    }

    std::vector<float> buildInputBlob(const QImage& img) const
    {
        QImage rgb = img.convertToFormat(QImage::Format_RGB32);
        QImage resized = rgb.scaled(
            m_inputSize, m_inputSize,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation);

        int plane = m_inputSize * m_inputSize;
        std::vector<float> blob(1UL * 3 * plane);

        for (int y = 0; y < m_inputSize; ++y) {
            for (int x = 0; x < m_inputSize; ++x) {
                QRgb px = resized.pixel(x, y);
                float r = qRed(px) / 255.0f;
                float g = qGreen(px) / 255.0f;
                float b = qBlue(px) / 255.0f;

                int idx = y * m_inputSize + x;
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
        for (int i = 1; i < m_inputSize * m_inputSize; ++i) {
            if (mask[i] < minV) minV = mask[i];
            if (mask[i] > maxV) maxV = mask[i];
        }
        float range = maxV - minV;
        if (range < 1e-8f) range = 1.0f;

        QImage maskImg(m_inputSize, m_inputSize, QImage::Format_Grayscale8);
        for (int y = 0; y < m_inputSize; ++y) {
            quint8* row = maskImg.scanLine(y);
            for (int x = 0; x < m_inputSize; ++x) {
                float val = (mask[y * m_inputSize + x] - minV) / range * 255.0f;
                row[x] = static_cast<quint8>(std::clamp(val, 0.0f, 255.0f));
            }
        }
        return maskImg.scaled(
            origW, origH,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation);
    }

    Ort::Env m_env;
    Ort::Session m_session;
    std::string m_inputName;
    std::string m_outputName;
    int m_inputSize = 320;
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

QImage BackgroundRemover::removeBackground(const QImage& input,
                                           const std::atomic<bool>* cancelFlag,
                                           const MatteOptions& options,
                                           QImage* rawAlphaOut)
{
    if (cancelFlag && cancelFlag->load())
        return QImage();
    if (!m_impl || input.isNull())
        return input;
    try {
        QImage result = m_impl->process(input, cancelFlag, options, rawAlphaOut);
        if (result.isNull() && cancelFlag && cancelFlag->load())
            return QImage();
        return result.isNull() ? input : result;
    } catch (const std::exception& e) {
        qCWarning(lcBgRemover) << "Erro durante inferência ONNX:" << e.what();
        return input;
    }
}

QImage BackgroundRemover::applyMatte(const QImage& input, const QImage& rawAlpha,
                                     const MatteOptions& options,
                                     const std::atomic<bool>* cancelFlag)
{
    if (input.isNull() || rawAlpha.isNull())
        return input;

    QImage alpha = rawAlpha.convertToFormat(QImage::Format_Grayscale8);
    if (alpha.size() != input.size())
        alpha = alpha.scaled(input.size(), Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);

    const int w = alpha.width(), h = alpha.height();
    const int cutLow = int(std::clamp(options.cutLevel - options.softness, 0.02f, 0.98f) * 255.0f);
    const int cutHigh = int(std::clamp(options.cutLevel + options.softness, 0.04f, 1.0f) * 255.0f);

    // 1) Rampa suave: sujeito 100% opaco, fundo 100% transparente.
    for (int y = 0; y < h; ++y) {
        quint8* row = alpha.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const int v = row[x];
            int a;
            if (v <= cutLow) a = 0;
            else if (v >= cutHigh) a = 255;
            else a = (v - cutLow) * 255 / (cutHigh - cutLow);
            row[x] = static_cast<quint8>(a);
        }
    }

    if (cancelFlag && cancelFlag->load())
        return QImage();

    // 2) Decisao binaria para a limpeza topologica.
    QImage bin(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        const quint8* src = alpha.constScanLine(y);
        quint8* dst = bin.scanLine(y);
        for (int x = 0; x < w; ++x)
            dst[x] = src[x] >= 128 ? 255 : 0;
    }

    // 3) Preencher buracos internos (fantasmas no corpo viram solido).
    if (options.fillHoles) {
        const auto background = backgroundFromBorders(bin);
        for (int y = 0; y < h; ++y) {
            quint8* row = bin.scanLine(y);
            for (int x = 0; x < w; ++x) {
                const size_t i = static_cast<size_t>(y) * w + x;
                if (!background[i] && row[x] == 0)
                    row[x] = 255;
            }
        }
    }

    if (cancelFlag && cancelFlag->load())
        return QImage();

    // 4) Remover manchas: componentes conectados pequenos somem.
    if (options.removeSpecks) {
        std::vector<quint8> visited(static_cast<size_t>(w) * h, 0);
        std::vector<qint32> stack, component;
        qint64 largestArea = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int start = y * w + x;
                if (visited[start] || bin.constScanLine(y)[x] < 128)
                    continue;
                qint64 area = 0;
                visited[start] = 1;
                stack.push_back(start);
                while (!stack.empty()) {
                    const int i = stack.back();
                    stack.pop_back();
                    ++area;
                    const int px = i % w, py = i / w;
                    const auto visit = [&](int nx, int ny) {
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) return;
                        const int ni = ny * w + nx;
                        if (!visited[ni] && bin.constScanLine(ny)[nx] >= 128) {
                            visited[ni] = 1;
                            stack.push_back(ni);
                        }
                    };
                    visit(px - 1, py); visit(px + 1, py);
                    visit(px, py - 1); visit(px, py + 1);
                }
                largestArea = qMax(largestArea, area);
            }
        }
        const qint64 minArea = qMax<qint64>(24, largestArea / 200);
        std::fill(visited.begin(), visited.end(), quint8(0));
        for (int y = 0; y < h; ++y) {
            if (cancelFlag && (y & 15) == 0 && cancelFlag->load())
                return QImage();
            for (int x = 0; x < w; ++x) {
                const int start = y * w + x;
                if (visited[start] || bin.constScanLine(y)[x] < 128)
                    continue;
                component.clear();
                visited[start] = 1;
                stack.push_back(start);
                while (!stack.empty()) {
                    const int i = stack.back();
                    stack.pop_back();
                    component.push_back(i);
                    const int px = i % w, py = i / w;
                    const auto visit = [&](int nx, int ny) {
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) return;
                        const int ni = ny * w + nx;
                        if (!visited[ni] && bin.constScanLine(ny)[nx] >= 128) {
                            visited[ni] = 1;
                            stack.push_back(ni);
                        }
                    };
                    visit(px - 1, py); visit(px + 1, py);
                    visit(px, py - 1); visit(px, py + 1);
                }
                if (static_cast<qint64>(component.size()) < minArea) {
                    for (const int i : component) {
                        const int px = i % w, py = i / w;
                        bin.scanLine(py)[px] = 0;
                    }
                }
            }
        }
    }

    if (cancelFlag && cancelFlag->load())
        return QImage();

    // Seguranca: se o corte apagou tudo, devolve a rampa original.
    qint64 subjectPixels = 0;
    for (int y = 0; y < h; ++y) {
        const quint8* row = bin.constScanLine(y);
        for (int x = 0; x < w; ++x)
            subjectPixels += row[x] >= 128;
    }
    if (subjectPixels == 0)
        return composite(input, alpha);

    // 5) Feather da borda e composicao final.
    QImage mat = bin;
    if (options.edgeFeather > 0)
        boxBlurGray(mat, std::min(3, options.edgeFeather));

    return composite(input, mat);
}

QImage BackgroundRemover::applyStickerOutline(const QImage& input, int width, const QColor& color)
{
    if (input.isNull() || width <= 0) return input.copy();
    const int r = std::clamp(width, 1, 60);

    const QImage rgba = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int w = rgba.width();
    const int h = rgba.height();

    std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y) {
        const uchar* row = rgba.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (row[x * 4 + 3] > 15) {
                mask[static_cast<size_t>(y) * w + x] = 255;
            }
        }
    }

    std::vector<uint8_t> dilated = mask;
    std::vector<std::pair<int, int>> offsets;
    const int r2 = r * r;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                offsets.emplace_back(dx, dy);
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (mask[static_cast<size_t>(y) * w + x] == 255) {
                for (const auto& [dx, dy] : offsets) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        dilated[static_cast<size_t>(ny) * w + nx] = 255;
                    }
                }
            }
        }
    }

    QImage result(w, h, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);

    const uchar red = static_cast<uchar>(color.red());
    const uchar green = static_cast<uchar>(color.green());
    const uchar blue = static_cast<uchar>(color.blue());
    const uchar alpha = static_cast<uchar>(color.alpha());

    for (int y = 0; y < h; ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < w; ++x) {
            if (dilated[static_cast<size_t>(y) * w + x] > 0) {
                row[x * 4]     = red;
                row[x * 4 + 1] = green;
                row[x * 4 + 2] = blue;
                row[x * 4 + 3] = alpha;
            }
        }
    }

    QPainter p(&result);
    p.drawImage(0, 0, rgba);
    p.end();

    return result;
}

namespace {
static QImage blurRgbaImage(const QImage& input, int radius)
{
    if (input.isNull() || radius <= 0) return input.copy();
    const int r = std::clamp(radius, 1, 80);
    QImage src = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int width = src.width();
    const int height = src.height();
    if (width == 0 || height == 0) return src;

    QImage intermediate(width, height, QImage::Format_RGBA8888);
    QImage dst(width, height, QImage::Format_RGBA8888);

    const int div = 2 * r + 1;
    for (int y = 0; y < height; ++y) {
        const uchar* inRow = src.constScanLine(y);
        uchar* outRow = intermediate.scanLine(y);

        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        for (int i = -r; i <= r; ++i) {
            const int px = std::clamp(i, 0, width - 1);
            sumR += inRow[px * 4];
            sumG += inRow[px * 4 + 1];
            sumB += inRow[px * 4 + 2];
            sumA += inRow[px * 4 + 3];
        }

        for (int x = 0; x < width; ++x) {
            outRow[x * 4]     = static_cast<uchar>(sumR / div);
            outRow[x * 4 + 1] = static_cast<uchar>(sumG / div);
            outRow[x * 4 + 2] = static_cast<uchar>(sumB / div);
            outRow[x * 4 + 3] = static_cast<uchar>(sumA / div);

            const int nextX = std::clamp(x + r + 1, 0, width - 1);
            const int prevX = std::clamp(x - r, 0, width - 1);

            sumR += inRow[nextX * 4]     - inRow[prevX * 4];
            sumG += inRow[nextX * 4 + 1] - inRow[prevX * 4 + 1];
            sumB += inRow[nextX * 4 + 2] - inRow[prevX * 4 + 2];
            sumA += inRow[nextX * 4 + 3] - inRow[prevX * 4 + 3];
        }
    }

    for (int x = 0; x < width; ++x) {
        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        for (int i = -r; i <= r; ++i) {
            const int py = std::clamp(i, 0, height - 1);
            const uchar* row = intermediate.constScanLine(py);
            sumR += row[x * 4];
            sumG += row[x * 4 + 1];
            sumB += row[x * 4 + 2];
            sumA += row[x * 4 + 3];
        }

        for (int y = 0; y < height; ++y) {
            uchar* outRow = dst.scanLine(y);
            outRow[x * 4]     = static_cast<uchar>(sumR / div);
            outRow[x * 4 + 1] = static_cast<uchar>(sumG / div);
            outRow[x * 4 + 2] = static_cast<uchar>(sumB / div);
            outRow[x * 4 + 3] = static_cast<uchar>(sumA / div);

            const int nextY = std::clamp(y + r + 1, 0, height - 1);
            const int prevY = std::clamp(y - r, 0, height - 1);

            const uchar* nextRow = intermediate.constScanLine(nextY);
            const uchar* prevRow = intermediate.constScanLine(prevY);

            sumR += nextRow[x * 4]     - prevRow[x * 4];
            sumG += nextRow[x * 4 + 1] - prevRow[x * 4 + 1];
            sumB += nextRow[x * 4 + 2] - prevRow[x * 4 + 2];
            sumA += nextRow[x * 4 + 3] - prevRow[x * 4 + 3];
        }
    }

    return dst;
}
} // namespace

QImage BackgroundRemover::applyDropShadow(const QImage& input, int offsetX,
                                          int offsetY, int blurRadius,
                                          float opacity)
{
    if (input.isNull()) return input.copy();
    const QImage rgba = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int w = rgba.width();
    const int h = rgba.height();
    const float op = std::clamp(opacity, 0.0f, 1.0f);

    QImage shadow(w, h, QImage::Format_RGBA8888);
    shadow.fill(Qt::transparent);

    for (int y = 0; y < h; ++y) {
        const uchar* srcRow = rgba.constScanLine(y);
        uchar* shadowRow = shadow.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const uchar srcA = srcRow[x * 4 + 3];
            if (srcA > 0) {
                shadowRow[x * 4]     = 0;
                shadowRow[x * 4 + 1] = 0;
                shadowRow[x * 4 + 2] = 0;
                shadowRow[x * 4 + 3] = static_cast<uchar>(srcA * op);
            }
        }
    }

    if (blurRadius > 0) {
        shadow = blurRgbaImage(shadow, blurRadius);
    }

    QImage result(w, h, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);

    QPainter p(&result);
    p.drawImage(offsetX, offsetY, shadow);
    p.drawImage(0, 0, rgba);
    p.end();

    return result;
}

QColor BackgroundRemover::detectCornerColor(const QImage& img)
{
    if (img.isNull()) return Qt::magenta;
    const QImage src = img.format() == QImage::Format_RGBA8888
        ? img
        : img.convertToFormat(QImage::Format_RGBA8888);
    const int w = src.width();
    const int h = src.height();

    const QList<QPoint> samplePoints = {
        QPoint(0, 0),
        QPoint(w - 1, 0),
        QPoint(0, h - 1),
        QPoint(w - 1, h - 1),
        QPoint(std::min(1, w - 1), std::min(1, h - 1)),
        QPoint(std::max(0, w - 2), std::min(1, h - 1)),
        QPoint(std::min(1, w - 1), std::max(0, h - 2)),
        QPoint(std::max(0, w - 2), std::max(0, h - 2))
    };

    std::map<QRgb, int> freq;
    for (const QPoint& pt : samplePoints) {
        const QRgb c = src.pixel(pt);
        freq[c]++;
    }

    QRgb bestRgb = src.pixel(0, 0);
    int bestCount = 0;
    for (const auto& [rgb, count] : freq) {
        if (count > bestCount) {
            bestCount = count;
            bestRgb = rgb;
        }
    }
    return QColor(bestRgb);
}

QImage BackgroundRemover::removeByColor(const QImage& input,
                                       const QColor& targetColor,
                                       int tolerance,
                                       bool contiguous,
                                       bool preservePixelArt)
{
    if (input.isNull() || !targetColor.isValid()) return input.copy();

    QImage result = input.format() == QImage::Format_RGBA8888
        ? input.copy()
        : input.convertToFormat(QImage::Format_RGBA8888);
    const int w = result.width();
    const int h = result.height();

    const int tr = targetColor.red();
    const int tg = targetColor.green();
    const int tb = targetColor.blue();

    const float maxDist = (std::clamp(tolerance, 0, 100) / 100.0f) * 441.67295593f;
    const float maxDistSq = maxDist * maxDist;

    auto matchesColor = [&](int r, int g, int b) -> bool {
        const float dr = static_cast<float>(r - tr);
        const float dg = static_cast<float>(g - tg);
        const float db = static_cast<float>(b - tb);
        return (dr * dr + dg * dg + db * db) <= maxDistSq;
    };

    auto calcAlpha = [&](int r, int g, int b, int currentAlpha) -> int {
        if (preservePixelArt) {
            return matchesColor(r, g, b) ? 0 : currentAlpha;
        }
        const float dr = static_cast<float>(r - tr);
        const float dg = static_cast<float>(g - tg);
        const float db = static_cast<float>(b - tb);
        const float dist = std::sqrt(dr * dr + dg * dg + db * db);
        if (dist <= maxDist * 0.7f) return 0;
        if (dist >= maxDist) return currentAlpha;
        const float t = (dist - maxDist * 0.7f) / (maxDist * 0.3f);
        return static_cast<int>(currentAlpha * t);
    };

    if (!contiguous) {
        for (int y = 0; y < h; ++y) {
            uchar* row = result.scanLine(y);
            for (int x = 0; x < w; ++x) {
                const int r = row[x * 4 + 0];
                const int g = row[x * 4 + 1];
                const int b = row[x * 4 + 2];
                const int a = row[x * 4 + 3];
                row[x * 4 + 3] = static_cast<uchar>(calcAlpha(r, g, b, a));
            }
        }
    } else {
        std::vector<uint8_t> visited(static_cast<size_t>(w * h), 0);
        std::vector<uint32_t> stack;
        stack.reserve(static_cast<size_t>(w) * 4);

        auto tryPush = [&](int x, int y) {
            if (x < 0 || x >= w || y < 0 || y >= h) return;
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w)
                               + static_cast<size_t>(x);
            if (visited[idx]) return;
            const uchar* pixel = result.constScanLine(y) + x * 4;
            if (matchesColor(pixel[0], pixel[1], pixel[2])) {
                visited[idx] = 1;
                stack.push_back(static_cast<uint32_t>(idx));
            }
        };

        for (int x = 0; x < w; ++x) {
            tryPush(x, 0);
            tryPush(x, h - 1);
        }
        for (int y = 0; y < h; ++y) {
            tryPush(0, y);
            tryPush(w - 1, y);
        }

        while (!stack.empty()) {
            const uint32_t idx = stack.back();
            stack.pop_back();

            const int curX = static_cast<int>(idx % static_cast<uint32_t>(w));
            const int curY = static_cast<int>(idx / static_cast<uint32_t>(w));

            uchar* p = result.scanLine(curY) + curX * 4;
            p[3] = static_cast<uchar>(calcAlpha(p[0], p[1], p[2], p[3]));

            tryPush(curX + 1, curY);
            tryPush(curX - 1, curY);
            tryPush(curX, curY + 1);
            tryPush(curX, curY - 1);
        }
    }

    return result;
}

QString BackgroundRemover::findModelPath(const QString& preferredName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString homeDir = QDir::homePath();

    const QStringList candidateDirs = {
        homeDir + QStringLiteral("/.local/share/bg-remover/models"),
        homeDir + QStringLiteral("/.local/share/aletheia/models"),
        QStringLiteral("/home/theking1x/projetos/bg-remover"),
        appDir + QStringLiteral("/../share/creatorcanvas/models"),
        appDir + QStringLiteral("/share/creatorcanvas/models"),
        appDir + QStringLiteral("/models"),
        // Development build: bin/ -> linux-debug/ -> build/ -> project root.
        appDir + QStringLiteral("/../../../resources/models"),
        appDir,
        appDataDir + QStringLiteral("/models"),
        appDataDir
    };

    if (!preferredName.isEmpty()) {
        for (const QString& dir : candidateDirs) {
            const QString full = dir + QStringLiteral("/") + preferredName;
            if (QFileInfo::exists(full))
                return full;
        }
    }

    const QStringList defaultNames = {
        QStringLiteral("isnet-general-use.onnx"),
        QStringLiteral("u2net.onnx"),
        QStringLiteral("u2netp.onnx"),
        QStringLiteral("isnet-anime.onnx")
    };

    for (const QString& name : defaultNames) {
        for (const QString& dir : candidateDirs) {
            const QString full = dir + QStringLiteral("/") + name;
            if (QFileInfo::exists(full))
                return full;
        }
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
