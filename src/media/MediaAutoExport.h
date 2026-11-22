#pragma once

#include "media/PlaylistExporter.h"

#include <QDate>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

#include <functional>
#include <vector>

namespace kmedia {

class DatabaseManager;

/**
 * Exports every day with media into `<folder>/YYYY-MM-DD/`: after each sync and each edit, on a
 * worker thread, with numbered names that keep the order of the meeting page. A day that changes
 * again while it exports is exported once more, and folders of past weeks are removed.
 */
class MediaAutoExport : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY settingsChanged FINAL)
    Q_PROPERTY(
        bool convertToMp4 READ convertToMp4 WRITE setConvertToMp4 NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool ffmpegAvailable READ isFfmpegAvailable CONSTANT FINAL)

public:
    using MeetingKindProvider = std::function<QString(const QDate&)>;
    using CoWeekProvider = std::function<bool(const QDate&)>;

    static constexpr int kParallelDays = 2;

    explicit MediaAutoExport(QObject* parent = nullptr);
    ~MediaAutoExport() override;

    Q_DISABLE_COPY_MOVE(MediaAutoExport)

    void setDatabase(DatabaseManager* database);
    void setScheduleProviders(MeetingKindProvider meetingKind, CoWeekProvider coWeek);
    void setConversionCacheDir(const QString& cacheDir);
    void setSongLabel(const QString& songLabel);
    void setToday(const QDate& today);

    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool enabled);
    [[nodiscard]] QString folder() const;
    void setFolder(const QString& folder);
    [[nodiscard]] bool convertToMp4() const;
    void setConvertToMp4(bool convert);
    [[nodiscard]] bool isFfmpegAvailable() const;
    [[nodiscard]] bool isActive() const;

    void exportDay(const QString& dateIso);
    void exportDays(const std::vector<QDate>& days);
    void waitForIdle();

    [[nodiscard]] std::vector<ExportSection> sectionsForDay(const QDate& date) const;

signals:
    void settingsChanged();
    void dayExported(const QString& dateIso, int copied, int conversionFailures);

private:
    void start(const QString& dateIso);
    void finish(const QString& dateIso, const DayExportResult& result);
    [[nodiscard]] QDate today() const;

    DatabaseManager* m_database{nullptr};
    MeetingKindProvider m_meetingKind;
    CoWeekProvider m_coWeek;
    QString m_cacheDir;
    QString m_songLabel{QStringLiteral("Song")};
    QDate m_today;
    bool m_enabled{false};
    QString m_folder;
    bool m_convertToMp4{false};
    bool m_ffmpegAvailable{false};
    QThreadPool m_pool;
    QSet<QString> m_running;
    QSet<QString> m_dirty;
};

} // namespace kmedia
