#ifndef FS_ORGANIZER_TESTS_SUPPORT_UNANNOUNCED_BOXES_H
#define FS_ORGANIZER_TESTS_SUPPORT_UNANNOUNCED_BOXES_H

#include <deque>
#include <functional>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtTest/QtTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWidget>

class UnannouncedBoxes final : public QObject
{
public:
    explicit UnannouncedBoxes(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&watch_, &QTimer::timeout, this, &UnannouncedBoxes::LookForABox);
        watch_.start(kEvery);
    }

    void Announce(std::function<void(QWidget&)> answer)
    {
        announced_.push_back(std::move(answer));
    }

    [[nodiscard]] std::size_t StillExpected() const
    {
        return announced_.size();
    }

private:
    static constexpr int kEvery = 5;

    static QString Describe(const QWidget& box)
    {
        if (const auto* message = qobject_cast<const QMessageBox*>(&box))
        {
            return QStringLiteral("\"%1\": %2 %3")
                .arg(message->windowTitle(), message->text(), message->informativeText())
                .trimmed();
        }

        return QStringLiteral("%1 \"%2\"").arg(QString::fromLatin1(box.metaObject()->className()), box.windowTitle());
    }

    static void Dismiss(QWidget& box)
    {
        if (auto* dialog = qobject_cast<QDialog*>(&box))
        {
            dialog->reject();

            return;
        }

        box.close();
    }

    void LookForABox()
    {
        QWidget* box = QApplication::activeModalWidget();

        if (box == nullptr)
        {
            return;
        }

        if (!announced_.empty())
        {
            const std::function<void(QWidget&)> answer = std::move(announced_.front());

            announced_.pop_front();
            answer(*box);

            if (box->isVisible())
            {
                Dismiss(*box);
            }

            return;
        }

        const QString said = Describe(*box);

        Dismiss(*box);
        QFAIL(qPrintable(QStringLiteral("a modal box nobody announced came up: ") + said));
    }

    QTimer watch_{};
    std::deque<std::function<void(QWidget&)>> announced_{};
};

#endif // FS_ORGANIZER_TESTS_SUPPORT_UNANNOUNCED_BOXES_H
