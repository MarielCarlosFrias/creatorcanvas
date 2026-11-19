#pragma once

#include <QCoreApplication>
#include <QImage>
#include <QMetaObject>
#include <QRunnable>
#include <QThreadPool>

#include <functional>

namespace cc {

/// Executa trabalho de imagem fora da thread da interface e entrega o
/// resultado de volta à fila Qt. O callback deve validar o seu próprio
/// contexto (por exemplo, usando QPointer e um generation token).
class AsyncImageTask final : public QRunnable
{
public:
    using Work = std::function<QImage()>;
    using Completion = std::function<void(QImage)>;

    static void start(Work work, Completion completion)
    {
        auto* task = new AsyncImageTask(std::move(work), std::move(completion));
        task->setAutoDelete(true);
        QThreadPool::globalInstance()->start(task);
    }

    void run() override
    {
        QImage result = m_work();
        QCoreApplication* app = QCoreApplication::instance();
        if (!app)
            return;

        QMetaObject::invokeMethod(app,
            [completion = std::move(m_completion), result = std::move(result)]() mutable {
                completion(std::move(result));
            }, Qt::QueuedConnection);
    }

private:
    AsyncImageTask(Work work, Completion completion)
        : m_work(std::move(work)), m_completion(std::move(completion))
    {
    }

    Work m_work;
    Completion m_completion;
};

} // namespace cc
