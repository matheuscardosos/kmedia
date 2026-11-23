#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace kmedia {

class CatalogClient;

/**
 * The yeartext shown on the presentation screen while nothing is presented, inspired by Meeting
 * Media Manager: fetched once a year in the media language, or the fallback languages, and kept
 * by the catalog cache for offline use. Sign languages show none.
 */
class YeartextProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged FINAL)

public:
    explicit YeartextProvider(CatalogClient* client, QObject* parent = nullptr);

    [[nodiscard]] QString text() const;
    // The languages to try in order; English is tried last when it is not in the list.
    void setLanguages(const QStringList& languages, bool signLanguage, int year);

signals:
    void textChanged();

private:
    void tryLanguage(int index);
    void setText(const QString& text);

    CatalogClient* m_client{nullptr};
    QStringList m_languages;
    int m_year{0};
    QString m_text;
    quint64 m_request{0};
};

} // namespace kmedia
