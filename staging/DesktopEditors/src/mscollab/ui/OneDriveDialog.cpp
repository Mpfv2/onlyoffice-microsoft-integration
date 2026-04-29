#include "OneDriveDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <curl/curl.h>
#include <fstream>

static size_t curlWriteFile(void* ptr, size_t size, size_t nmemb, std::ofstream* f) {
    f->write(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

// Sentinel stored in item data to indicate a not-yet-loaded folder
static const QString PLACEHOLDER_TEXT = "__placeholder__";

OneDriveDialog::OneDriveDialog(std::function<std::string()> tokenProvider,
                               QWidget* parent)
    : QDialog(parent), m_graph(std::move(tokenProvider))
{
    setWindowTitle("Open from OneDrive");
    resize(520, 480);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"Name"});
    m_tree->header()->setSectionResizeMode(QHeaderView::Stretch);
    m_tree->setAnimated(true);

    m_status = new QLabel("Loading OneDrive...", this);
    m_status->setWordWrap(true);

    m_open = new QPushButton("Open", this);
    m_open->setEnabled(false);
    auto* cancel = new QPushButton("Cancel", this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_open);
    btnRow->addWidget(cancel);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tree);
    layout->addWidget(m_status);
    layout->addLayout(btnRow);

    connect(m_tree, &QTreeWidget::itemExpanded,
            this,   &OneDriveDialog::onItemExpanded);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this,   &OneDriveDialog::onItemDoubleClicked);
    connect(m_open, &QPushButton::clicked, this, &OneDriveDialog::onOpenClicked);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        auto sel = m_tree->selectedItems();
        if (sel.isEmpty()) { m_open->setEnabled(false); return; }
        auto* item = sel.first();
        bool isFolder = item->data(0, Qt::UserRole + 1).toBool();
        m_open->setEnabled(!isFolder);
    });

    // Load root
    populateFolder(nullptr, "");
    m_status->setText("Select a .docx file to open.");
}

void OneDriveDialog::populateFolder(QTreeWidgetItem* parent,
                                    const std::string& folderPath) {
    auto items = m_graph.listFolder(folderPath);
    if (items.empty()) {
        m_status->setText("Could not load folder — check your connection and login.");
        return;
    }
    for (const auto& di : items) {
        auto* node = parent
            ? new QTreeWidgetItem(parent,   QStringList{QString::fromStdString(di.name)})
            : new QTreeWidgetItem(m_tree,   QStringList{QString::fromStdString(di.name)});

        node->setData(0, Qt::UserRole,     QString::fromStdString(di.webUrl));
        node->setData(0, Qt::UserRole + 1, di.isFolder);
        node->setData(0, Qt::UserRole + 2, QString::fromStdString(di.downloadUrl));
        node->setData(0, Qt::UserRole + 3, QString::fromStdString(
            folderPath.empty() ? di.name : folderPath + "/" + di.name));

        if (di.isFolder) {
            // Add a placeholder child so the expand arrow shows
            auto* ph = new QTreeWidgetItem(node, QStringList{PLACEHOLDER_TEXT});
            (void)ph;
        } else if (!QString::fromStdString(di.name).endsWith(".docx",
                        Qt::CaseInsensitive)) {
            // Dim non-.docx files
            node->setDisabled(true);
        }
    }
}

void OneDriveDialog::onItemExpanded(QTreeWidgetItem* item) {
    // Replace placeholder with real children on first expand
    if (item->childCount() == 1 &&
        item->child(0)->text(0) == PLACEHOLDER_TEXT) {
        delete item->takeChild(0);
        std::string path = item->data(0, Qt::UserRole + 3).toString().toStdString();
        populateFolder(item, path);
    }
}

void OneDriveDialog::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/) {
    bool isFolder = item->data(0, Qt::UserRole + 1).toBool();
    if (!isFolder) onOpenClicked();
}

void OneDriveDialog::onOpenClicked() {
    auto sel = m_tree->selectedItems();
    if (sel.isEmpty()) return;
    auto* item = sel.first();
    bool isFolder = item->data(0, Qt::UserRole + 1).toBool();
    if (isFolder) return;

    QString downloadUrl = item->data(0, Qt::UserRole + 2).toString();
    QString webUrl      = item->data(0, Qt::UserRole).toString();
    QString name        = item->text(0);

    if (downloadUrl.isEmpty()) {
        QMessageBox::warning(this, "Error", "No download URL for this file.");
        return;
    }

    m_status->setText("Downloading " + name + "...");
    repaint();

    if (!downloadToTemp(downloadUrl.toStdString(), name.toStdString(),
                        m_selectedLocalPath)) {
        QMessageBox::warning(this, "Error", "Failed to download file.");
        m_status->setText("Download failed.");
        return;
    }
    m_selectedWebUrl = webUrl;
    accept();
}

bool OneDriveDialog::downloadToTemp(const std::string& downloadUrl,
                                    const std::string& fileName,
                                    QString& outPath) {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/mscollab/";
    QDir().mkpath(tempDir);
    outPath = tempDir + QString::fromStdString(fileName);

    std::ofstream file(outPath.toStdString(), std::ios::binary);
    if (!file.is_open()) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, downloadUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    file.close();
    return res == CURLE_OK;
}

QString OneDriveDialog::selectedLocalPath() const { return m_selectedLocalPath; }
QString OneDriveDialog::selectedWebUrl()    const { return m_selectedWebUrl; }
