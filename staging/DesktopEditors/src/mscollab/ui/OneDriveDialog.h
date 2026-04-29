#pragma once
#include "../graph/GraphApiClient.h"
#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QString>
#include <string>
#include <functional>

// Modal dialog for browsing and selecting a file from the user's OneDrive.
// Call exec() — returns QDialog::Accepted if the user selected a file.
// After acceptance, selectedLocalPath() returns the temp path of the downloaded file.
class OneDriveDialog : public QDialog {
    Q_OBJECT

public:
    explicit OneDriveDialog(std::function<std::string()> tokenProvider,
                            QWidget* parent = nullptr);

    // Valid after exec() returns QDialog::Accepted.
    QString selectedLocalPath() const;
    QString selectedWebUrl() const;

private slots:
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onOpenClicked();

private:
    GraphApiClient            m_graph;
    QTreeWidget*              m_tree   = nullptr;
    QPushButton*              m_open   = nullptr;
    QLabel*                   m_status = nullptr;
    QString                   m_selectedLocalPath;
    QString                   m_selectedWebUrl;

    void populateFolder(QTreeWidgetItem* parent, const std::string& folderPath);
    bool downloadToTemp(const std::string& downloadUrl,
                        const std::string& fileName,
                        QString& outPath);
};
