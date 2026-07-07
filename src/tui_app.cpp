#include "tui_app.h"
#include "id3tag.h"
#include "mp3file.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

// --- Helper: format file size ---
static std::string formatSize(int64_t sz) {
    if (sz < 1024) return std::to_string(sz) + " B";
    if (sz < 1024 * 1024) return std::to_string(sz / 1024) + " KB";
    return std::to_string(sz / (1024 * 1024)) + " MB";
}

// --- Helper: truncate string ---
static std::string trunc(const std::string &s, int maxLen) {
    if ((int)s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 1) + "~";
}

// --- Helper: get album art via chafa ---
static std::string getChafaArt(const std::vector<uint8_t> &data, int width, int height) {
    if (data.empty()) return "";

    char tmpPath[] = "/tmp/mp3tui_art_XXXXXX";
    int fd = mkstemp(tmpPath);
    if (fd < 0) return "";
    close(fd);

    std::string tmpStr(tmpPath);
    {
        std::ofstream tmpFile(tmpStr, std::ios::binary);
        tmpFile.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::string cmd = "chafa --size=" + std::to_string(width) + "x" +
                      std::to_string(height) + " --format=symbols \"" + tmpStr + "\" 2>/dev/null";

    std::string result;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe))
            result += buffer;
        pclose(pipe);
    }

    fs::remove(tmpStr);
    return result;
}

// ======================================================================
// TuiApp
// ======================================================================

TuiApp::TuiApp() {
    m_tableHeight = 20;
}

TuiApp::~TuiApp() {}

void TuiApp::loadFile(const std::string &path) {
    auto file = std::make_unique<MP3File>(path);
    if (file->load()) {
        m_files.push_back(std::move(file));
        if (m_files.size() == 1)
            m_selectedRow = 0;
    }
}

void TuiApp::loadFolder(const std::string &dir) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;
    for (const auto &entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mp3")
            loadFile(entry.path().string());
    }
}

void TuiApp::browseDirectory(const std::string &dir) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;

    m_currentDir = fs::absolute(dir).string();
    m_entries.clear();
    m_files.clear();
    m_selectedRow = -1;
    m_scrollOffset = 0;

    // Add parent directory entry (unless at root)
    fs::path parent = fs::path(m_currentDir).parent_path();
    if (parent != m_currentDir) {
        m_entries.push_back({"..", true});
    }

    // Scan directory
    try {
        for (const auto &entry : fs::directory_iterator(m_currentDir)) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) {
                // Skip hidden directories
                if (!name.empty() && name[0] == '.') continue;
                m_entries.push_back({name, true});
            } else if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mp3") {
                    loadFile(entry.path().string());
                    m_entries.push_back({name, false});
                }
            }
        }
    } catch (const fs::filesystem_error &) {}

    // Sort: dirs first, then files, alphabetically
    std::stable_sort(m_entries.begin(), m_entries.end(),
        [](const DirEntry &a, const DirEntry &b) {
            if (a.isDir != b.isDir) return a.isDir;
            // ".." always first
            if (a.name == "..") return true;
            if (b.name == "..") return false;
            return a.name < b.name;
        });

    if (!m_entries.empty())
        m_selectedRow = 0;

    updateStatusBar();
}

MP3File* TuiApp::selectedFile() const {
    if (m_selectedRow < 0 || m_selectedRow >= (int)m_entries.size())
        return nullptr;
    const DirEntry &e = m_entries[m_selectedRow];
    if (e.isDir) return nullptr;

    // Find matching file by name
    for (auto &f : m_files) {
        if (f->fileName() == e.name)
            return f.get();
    }
    return nullptr;
}

void TuiApp::enterEntry() {
    if (m_selectedRow < 0 || m_selectedRow >= (int)m_entries.size())
        return;

    const DirEntry &e = m_entries[m_selectedRow];
    if (e.isDir) {
        if (e.name == "..") {
            goUpDirectory();
        } else {
            fs::path newPath = fs::path(m_currentDir) / e.name;
            browseDirectory(newPath.string());
        }
    } else {
        // Select file for editing
        syncEditFields();
        m_editing = false;
    }
}

void TuiApp::goUpDirectory() {
    fs::path parent = fs::path(m_currentDir).parent_path();
    if (parent != m_currentDir) {
        std::string prevDir = fs::path(m_currentDir).filename().string();
        browseDirectory(parent.string());
        // Select the directory we came from
        for (int i = 0; i < (int)m_entries.size(); i++) {
            if (m_entries[i].name == prevDir) {
                m_selectedRow = i;
                break;
            }
        }
    }
}

void TuiApp::syncEditFields() {
    auto *file = selectedFile();
    if (!file) {
        m_editTitle.clear();
        m_editArtist.clear();
        m_editAlbum.clear();
        m_editYear.clear();
        m_editTrack.clear();
        m_editGenre.clear();
        m_editComposer.clear();
        m_editComment.clear();
        m_editFileName.clear();
        return;
    }
    m_editTitle = file->title();
    m_editArtist = file->artist();
    m_editAlbum = file->album();
    m_editYear = file->year();
    m_editTrack = file->track();
    m_editGenre = file->genre();
    m_editComposer = file->composer();
    m_editComment = file->comment();
    m_editFileName = file->fileName();
}

std::vector<MP3File*> TuiApp::selectedFiles() const {
    std::vector<MP3File*> sel;
    auto *f = selectedFile();
    if (f) sel.push_back(f);
    return sel;
}

void TuiApp::updateStatusBar() {
    int totalEntries = m_entries.size();
    int modified = 0;
    for (auto &f : m_files)
        if (f->isModified()) modified++;

    std::ostringstream oss;
    oss << m_currentDir;
    oss << " | " << totalEntries << " items";
    if (modified > 0)
        oss << " | Modified: " << modified;
    if (m_selectedRow >= 0)
        oss << " | [" << (m_selectedRow + 1) << "/" << totalEntries << "]";
    m_statusMessage = oss.str();
}

// ======================================================================
// Toolbar
// ======================================================================

Component TuiApp::buildToolbar() {
    auto addFilesBtn = Button("[a] Add Files", [this] { onAddFiles(); });
    auto addFolderBtn = Button("[f] Add Folder", [this] { onAddFolder(); });
    auto clearBtn = Button("[x] Clear", [this] { onClearAll(); });
    auto selectAllBtn = Button("[s] Select All", [this] { onToggleSelectAll(); });

    return Container::Horizontal({
        addFilesBtn,
        addFolderBtn,
        clearBtn,
        selectAllBtn,
    }) | Renderer([](Element inner) {
        return hbox({
            text(" MP3 Metadata Editor ") | bold | color(Color::Cyan),
            separator(),
            inner,
        });
    });
}

void TuiApp::onAddFiles() {
    m_showAddFiles = true;
    m_inputPath.clear();
}

void TuiApp::onAddFolder() {
    m_showAddFolder = true;
    m_inputPath.clear();
}

void TuiApp::onClearAll() {
    m_files.clear();
    m_entries.clear();
    m_selectedRow = -1;
    m_scrollOffset = 0;
    m_currentDir.clear();
    m_editTitle.clear();
    m_editArtist.clear();
    m_editAlbum.clear();
    m_editYear.clear();
    m_editTrack.clear();
    m_editGenre.clear();
    m_editComposer.clear();
    m_editComment.clear();
    m_editFileName.clear();
    updateStatusBar();
}

void TuiApp::onToggleSelectAll() {
    if (m_entries.empty()) return;
    m_selectAll = !m_selectAll;
    if (m_selectAll)
        m_selectedRow = 0;
    else
        m_selectedRow = -1;
    updateStatusBar();
}

void TuiApp::onRemoveArt() {
    auto *file = selectedFile();
    if (!file) return;
    file->removeAlbumArt();
    updateStatusBar();
}

void TuiApp::onDiscardChanges() {
    int modified = 0;
    for (auto &f : m_files)
        if (f->isModified()) modified++;
    if (modified == 0) {
        m_toolbarMessage = "No modified files to discard.";
        return;
    }
    m_showConfirmDiscard = true;
}

void TuiApp::onApply() {
    int modified = 0;
    for (auto &f : m_files)
        if (f->isModified()) modified++;
    if (modified == 0) {
        m_toolbarMessage = "No modified files to save.";
        return;
    }
    m_showConfirmApply = true;
}

// ======================================================================
// File Table
// ======================================================================

Component TuiApp::buildTable() {
    return Renderer([this] {
        return renderFileTable() | reflect(m_tableBox);
    }) | CatchEvent([this](Event event) {
        int total = (int)m_entries.size();

        if (event == Event::ArrowUp && m_selectedRow > 0) {
            m_selectedRow--;
            if (m_selectedRow < m_scrollOffset)
                m_scrollOffset = m_selectedRow;
            syncEditFields();
            updateStatusBar();
            return true;
        }
        if (event == Event::ArrowDown && m_selectedRow < total - 1) {
            m_selectedRow++;
            if (m_selectedRow >= m_scrollOffset + m_tableHeight)
                m_scrollOffset = m_selectedRow - m_tableHeight + 1;
            syncEditFields();
            updateStatusBar();
            return true;
        }
        if (event == Event::PageUp) {
            m_selectedRow = std::max(0, m_selectedRow - m_tableHeight);
            if (m_selectedRow < m_scrollOffset)
                m_scrollOffset = m_selectedRow;
            syncEditFields();
            updateStatusBar();
            return true;
        }
        if (event == Event::PageDown) {
            m_selectedRow = std::min(total - 1, m_selectedRow + m_tableHeight);
            if (m_selectedRow >= m_scrollOffset + m_tableHeight)
                m_scrollOffset = m_selectedRow - m_tableHeight + 1;
            syncEditFields();
            updateStatusBar();
            return true;
        }
        if (event == Event::Home) {
            m_selectedRow = 0;
            m_scrollOffset = 0;
            syncEditFields();
            updateStatusBar();
            return true;
        }
        if (event == Event::End) {
            if (!m_entries.empty()) {
                m_selectedRow = total - 1;
                if (m_selectedRow >= m_scrollOffset + m_tableHeight)
                    m_scrollOffset = m_selectedRow - m_tableHeight + 1;
            }
            syncEditFields();
            updateStatusBar();
            return true;
        }
        if (event == Event::Return && m_selectedRow >= 0) {
            enterEntry();
            updateStatusBar();
            return true;
        }

        // Mouse click - use reflect box for relative coordinates
        if (event.is_mouse() && event.mouse().button == Mouse::Left &&
            event.mouse().motion == Mouse::Pressed) {
            int relY = event.mouse().y - m_tableBox.y_min - 2; // 2 rows header
            int clickedRow = m_scrollOffset + relY;
            if (clickedRow >= 0 && clickedRow < total) {
                m_selectedRow = clickedRow;
                syncEditFields();
                updateStatusBar();
                return true;
            }
        }

        // Mouse wheel
        if (event.is_mouse() && event.mouse().button == Mouse::WheelUp) {
            if (m_selectedRow > 0) {
                m_selectedRow--;
                if (m_selectedRow < m_scrollOffset)
                    m_scrollOffset = m_selectedRow;
                syncEditFields();
                updateStatusBar();
            }
            return true;
        }
        if (event.is_mouse() && event.mouse().button == Mouse::WheelDown) {
            if (m_selectedRow < total - 1) {
                m_selectedRow++;
                if (m_selectedRow >= m_scrollOffset + m_tableHeight)
                    m_scrollOffset = m_selectedRow - m_tableHeight + 1;
                syncEditFields();
                updateStatusBar();
            }
            return true;
        }
        return false;
    }) | border | size(WIDTH, EQUAL, 60);
}

Element TuiApp::renderFileTable() {
    Elements rows;

    // Header
    rows.push_back(hbox({
        text(trunc("Name", 25)) | size(WIDTH, EQUAL, 25) | bold,
        text(trunc("Artist", 12)) | size(WIDTH, EQUAL, 12) | bold,
        text(trunc("Title", 12)) | size(WIDTH, EQUAL, 12) | bold,
        text(trunc("Year", 5)) | size(WIDTH, EQUAL, 5) | bold,
    }) | bgcolor(Color::GrayDark));

    rows.push_back(separatorHSelector(0, 1, Color::GrayDark, Color::White));

    int total = (int)m_entries.size();

    // Clamp scroll offset
    if (m_scrollOffset > 0 && m_scrollOffset > total - m_tableHeight)
        m_scrollOffset = std::max(0, total - m_tableHeight);
    if (m_scrollOffset < 0) m_scrollOffset = 0;

    int visibleStart = m_scrollOffset;
    int visibleEnd = std::min(total, visibleStart + m_tableHeight);

    for (int i = visibleStart; i < visibleEnd; i++) {
        const DirEntry &entry = m_entries[i];
        bool selected = (i == m_selectedRow);

        std::string name, artist, title, year;
        Color rowColor = Color::Default;

        if (entry.isDir) {
            name = "  " + trunc(entry.name, 22) + "/";
            rowColor = Color::Yellow;
        } else {
            // Find matching file
            MP3File *file = nullptr;
            for (auto &f : m_files) {
                if (f->fileName() == entry.name) {
                    file = f.get();
                    break;
                }
            }
            if (file) {
                name = trunc(file->fileName(), 25);
                artist = trunc(file->artist(), 12);
                title = trunc(file->title(), 12);
                year = trunc(file->year(), 5);
                if (file->isModified())
                    rowColor = Color::Blue;
            } else {
                name = trunc(entry.name, 25);
            }
        }

        auto row = hbox({
            text(name) | size(WIDTH, EQUAL, 25),
            text(artist) | size(WIDTH, EQUAL, 12),
            text(title) | size(WIDTH, EQUAL, 12),
            text(year) | size(WIDTH, EQUAL, 5),
        });

        if (rowColor != Color::Default)
            row = row | color(rowColor);
        if (selected)
            row = row | inverted;

        rows.push_back(row);
    }

    // Scroll indicators
    if (m_scrollOffset > 0 || visibleEnd < total) {
        std::string scrollInfo = "  (" + std::to_string(m_scrollOffset + 1) +
                                 "-" + std::to_string(visibleEnd) +
                                 " of " + std::to_string(total) + ")";
        rows.push_back(text(scrollInfo) | dim);
    }

    if (m_entries.empty()) {
        rows.push_back(text("  Empty directory") | dim);
        rows.push_back(text("  Press [f] to add folder") | dim);
    }

    return vbox(std::move(rows));
}

// ======================================================================
// Detail Panel
// ======================================================================

Component TuiApp::buildDetailPanel() {
    return Renderer([this] {
        return renderDetailPanel() | reflect(m_detailBox);
    }) | CatchEvent([this](Event event) {
        // Mouse click on detail panel fields
        if (event.is_mouse() && event.mouse().button == Mouse::Left &&
            event.mouse().motion == Mouse::Pressed && selectedFile()) {
            int relY = event.mouse().y - m_detailBox.y_min;
            // Fields start at row 5 (header + file info + separator + 3 header rows)
            // Each field is 1 row: Title(5), Artist(6), Album(7), Year(8),
            // Track(9), Genre(10), Composer(11), Comment(12)
            int fieldStart = 5;
            int fieldIdx = relY - fieldStart;
            if (fieldIdx >= 0 && fieldIdx < 8) {
                m_editFieldIndex = fieldIdx;
                m_editing = true;
                syncEditFields();
                return true;
            }
            return true; // consume click even outside fields
        }

        if (!m_editing) {
            if (event == Event::Return || event == Event::Character('e')) {
                if (selectedFile()) {
                    m_editing = true;
                    m_editFieldIndex = 0;
                }
                return true;
            }
            if (event == Event::ArrowUp && m_editFieldIndex > 0) {
                m_editFieldIndex--;
                return true;
            }
            if (event == Event::ArrowDown && m_editFieldIndex < 7) {
                m_editFieldIndex++;
                return true;
            }
            if (event == Event::Character('r')) {
                onRemoveArt();
                return true;
            }
            return false;
        }

        // Edit mode
        if (event == Event::Escape) {
            m_editing = false;
            return true;
        }
        if (event == Event::Tab || event == Event::ArrowDown) {
            m_editFieldIndex = (m_editFieldIndex + 1) % 8;
            return true;
        }
        if (event == Event::TabReverse || event == Event::ArrowUp) {
            m_editFieldIndex = (m_editFieldIndex + 7) % 8;
            return true;
        }

        // Apply edit to selected file(s)
        auto applyField = [this](int fieldIdx, const std::string &value) {
            for (auto *file : selectedFiles()) {
                switch (fieldIdx) {
                    case 0: file->setTitle(value); break;
                    case 1: file->setArtist(value); break;
                    case 2: file->setAlbum(value); break;
                    case 3: file->setYear(value); break;
                    case 4: file->setTrack(value); break;
                    case 5: file->setGenre(value); break;
                    case 6: file->setComposer(value); break;
                    case 7: file->setComment(value); break;
                }
            }
        };

        if (event == Event::Return) {
            std::string *fields[] = {
                &m_editTitle, &m_editArtist, &m_editAlbum, &m_editYear,
                &m_editTrack, &m_editGenre, &m_editComposer, &m_editComment
            };
            applyField(m_editFieldIndex, *fields[m_editFieldIndex]);

            if (m_editFieldIndex == 0 && !m_editFileName.empty()) {
                for (auto *file : selectedFiles())
                    file->setNewFileName(m_editFileName);
            }

            updateStatusBar();
            return true;
        }

        // Text input handling
        std::string *fields[] = {
            &m_editTitle, &m_editArtist, &m_editAlbum, &m_editYear,
            &m_editTrack, &m_editGenre, &m_editComposer, &m_editComment
        };

        if (event == Event::Backspace) {
            auto &field = *fields[m_editFieldIndex];
            if (!field.empty()) {
                field.pop_back();
                applyField(m_editFieldIndex, field);
            }
            return true;
        }

        if (event.is_character()) {
            auto &field = *fields[m_editFieldIndex];
            field += event.character();
            applyField(m_editFieldIndex, field);
            return true;
        }

        return false;
    }) | border | size(WIDTH, EQUAL, 50);
}

Element TuiApp::renderDetailPanel() {
    Elements rows;

    rows.push_back(text(" File Details ") | bold | color(Color::Cyan));
    rows.push_back(separator());

    auto *file = selectedFile();
    if (!file) {
        auto *entry = (m_selectedRow >= 0 && m_selectedRow < (int)m_entries.size())
                      ? &m_entries[m_selectedRow] : nullptr;
        if (entry && entry->isDir) {
            rows.push_back(text("  [DIR] " + entry->name) | color(Color::Yellow));
            rows.push_back(text("  Press Enter to open") | dim);
        } else {
            rows.push_back(text("  No file selected") | dim);
            rows.push_back(text("  Navigate with Up/Down arrows") | dim);
            rows.push_back(text("  Press Enter to open dir/select file") | dim);
        }
        rows.push_back(separator());
        rows.push_back(text(m_toolbarMessage) | color(Color::Yellow));
        return vbox(std::move(rows));
    }

    rows.push_back(text("  " + file->filePath()) | dim);
    rows.push_back(text("  Size: " + formatSize(file->fileSize()) + "  |  Modified: " + file->lastModified()) | dim);
    rows.push_back(separator());

    auto makeField = [this](const std::string &label, const std::string &value, int fieldIdx) {
        bool active = m_editing && m_editFieldIndex == fieldIdx;
        auto labelEl = text(label) | size(WIDTH, EQUAL, 12);
        auto valueEl = text(value.empty() ? "(empty)" : value);
        if (active)
            valueEl = valueEl | inverted | color(Color::Cyan);
        return hbox({labelEl, valueEl});
    };

    rows.push_back(makeField("  Title:    ", m_editTitle, 0));
    rows.push_back(makeField("  Artist:   ", m_editArtist, 1));
    rows.push_back(makeField("  Album:    ", m_editAlbum, 2));
    rows.push_back(makeField("  Year:     ", m_editYear, 3));
    rows.push_back(makeField("  Track:    ", m_editTrack, 4));
    rows.push_back(makeField("  Genre:    ", m_editGenre, 5));
    rows.push_back(makeField("  Composer: ", m_editComposer, 6));
    rows.push_back(makeField("  Comment:  ", m_editComment, 7));

    rows.push_back(separator());

    // Album art
    rows.push_back(text("  Album Art") | bold);
    if (file->hasAlbumArt() && !file->removeArtFlag()) {
        std::string art = getChafaArt(file->albumArt(), 40, 15);
        if (!art.empty()) {
            rows.push_back(text(art));
        } else {
            rows.push_back(text("  [Image present - install chafa to view]") | dim);
        }
        rows.push_back(text("  [r] Remove art") | dim);
    } else if (file->removeArtFlag()) {
        rows.push_back(text("  (removed on apply)") | color(Color::Red));
    } else {
        rows.push_back(text("  No image") | dim);
    }

    rows.push_back(separator());
    rows.push_back(text(m_toolbarMessage) | color(Color::Yellow));

    return vbox(std::move(rows));
}

// ======================================================================
// Status Bar
// ======================================================================

Component TuiApp::buildStatusBar() {
    return Renderer([this] {
        return renderStatusBar();
    });
}

Element TuiApp::renderStatusBar() {
    return hbox({
        text(" " + m_statusMessage + " ") | color(Color::White) | bgcolor(Color::Blue),
        filler(),
        text(" [q]Quit [Enter]Open [e]Edit [Ctrl+S]Save ") |
            color(Color::White) | bgcolor(Color::GrayDark),
    });
}

// ======================================================================
// Main Run Loop
// ======================================================================

void TuiApp::run() {
    // If no directory set, use current directory
    if (m_currentDir.empty()) {
        browseDirectory(fs::current_path().string());
    }

    updateStatusBar();

    auto toolbar = buildToolbar();
    auto table = buildTable();
    auto detail = buildDetailPanel();
    auto status = buildStatusBar();

    // Left panel: toolbar + table
    auto leftPanel = Container::Vertical({
        toolbar,
        table,
    });

    // Right panel: detail
    auto rightPanel = detail;

    // Main layout with split
    int splitSize = 55;
    auto mainContainer = ResizableSplitLeft(
        leftPanel,
        rightPanel,
        &splitSize
    );

    // Full layout
    auto fullLayout = Container::Vertical({
        mainContainer,
        status,
    });

    auto app = App::Fullscreen();

    // Wrap with global event handling
    auto wrapped = fullLayout | CatchEvent([this, &app](Event event) {
        // Modal dialogs consume ALL events
        if (m_showAddFiles || m_showAddFolder ||
            m_showConfirmDiscard || m_showConfirmApply) {

            if (m_showAddFiles) {
                if (event == Event::Escape) {
                    m_showAddFiles = false;
                    return true;
                }
                if (event == Event::Backspace && !m_inputPath.empty()) {
                    m_inputPath.pop_back();
                    return true;
                }
                if (event == Event::Return) {
                    std::istringstream iss(m_inputPath);
                    std::string token;
                    while (iss >> token)
                        loadFile(token);
                    m_showAddFiles = false;
                    m_inputPath.clear();
                    updateStatusBar();
                    return true;
                }
                if (event.is_character()) {
                    m_inputPath += event.character();
                    return true;
                }
                return true;
            }

            if (m_showAddFolder) {
                if (event == Event::Escape) {
                    m_showAddFolder = false;
                    return true;
                }
                if (event == Event::Backspace && !m_inputPath.empty()) {
                    m_inputPath.pop_back();
                    return true;
                }
                if (event == Event::Return) {
                    browseDirectory(m_inputPath);
                    m_showAddFolder = false;
                    m_inputPath.clear();
                    updateStatusBar();
                    return true;
                }
                if (event.is_character()) {
                    m_inputPath += event.character();
                    return true;
                }
                return true;
            }

            if (m_showConfirmDiscard) {
                if (event == Event::Character('y') || event == Event::Return) {
                    for (auto &f : m_files)
                        if (f->isModified()) f->discardChanges();
                    m_showConfirmDiscard = false;
                    m_toolbarMessage = "Changes discarded.";
                    syncEditFields();
                    updateStatusBar();
                    return true;
                }
                if (event == Event::Character('n') || event == Event::Escape) {
                    m_showConfirmDiscard = false;
                    return true;
                }
                return true;
            }

            if (m_showConfirmApply) {
                if (event == Event::Character('y') || event == Event::Return) {
                    int success = 0, failed = 0;
                    for (auto &f : m_files) {
                        if (f->isModified()) {
                            if (f->save()) success++;
                            else failed++;
                        }
                    }
                    m_showConfirmApply = false;
                    std::ostringstream oss;
                    oss << "Saved: " << success << " ok, " << failed << " failed.";
                    m_toolbarMessage = oss.str();
                    syncEditFields();
                    updateStatusBar();
                    return true;
                }
                if (event == Event::Character('n') || event == Event::Escape) {
                    m_showConfirmApply = false;
                    return true;
                }
                return true;
            }
        }

        // Global shortcuts - NOT when editing text
        if (!m_editing) {
            if (event == Event::CtrlC || event == Event::Character('q')) {
                app.Exit();
                return true;
            }
            if (event == Event::CtrlS) {
                onApply();
                return true;
            }
            if (event == Event::Character('e')) {
                if (selectedFile()) {
                    m_editing = true;
                    m_editFieldIndex = 0;
                }
                return true;
            }
            if (event == Event::Character('d')) {
                onDiscardChanges();
                return true;
            }
            if (event == Event::Character('r')) {
                onRemoveArt();
                return true;
            }
        }

        return false;
    });

    // Custom renderer for modals
    auto modalRenderer = Renderer(wrapped, [this, wrapped] {
        auto content = wrapped->Render();

        if (m_showAddFiles) {
            auto modal = vbox({
                text(" Add MP3 Files ") | bold | color(Color::Cyan),
                separator(),
                text(" Enter file paths (space-separated):"),
                text(" > " + m_inputPath + "_") | color(Color::Yellow),
                separator(),
                text(" [Enter] Load  [Esc] Cancel") | dim,
            }) | border | size(WIDTH, LESS_THAN, 60);

            return dbox({
                content,
                modal | center,
            });
        }

        if (m_showAddFolder) {
            auto modal = vbox({
                text(" Add Folder ") | bold | color(Color::Cyan),
                separator(),
                text(" Enter folder path:"),
                text(" > " + m_inputPath + "_") | color(Color::Yellow),
                separator(),
                text(" [Enter] Load  [Esc] Cancel") | dim,
            }) | border | size(WIDTH, LESS_THAN, 60);

            return dbox({
                content,
                modal | center,
            });
        }

        if (m_showConfirmDiscard) {
            int modified = 0;
            for (auto &f : m_files)
                if (f->isModified()) modified++;

            auto modal = vbox({
                text(" Discard Changes ") | bold | color(Color::Red),
                separator(),
                text(" Revert changes to " + std::to_string(modified) + " file(s)?"),
                separator(),
                text(" [y] Yes  [n] No") | dim,
            }) | border | size(WIDTH, LESS_THAN, 50);

            return dbox({
                content,
                modal | center,
            });
        }

        if (m_showConfirmApply) {
            int modified = 0;
            for (auto &f : m_files)
                if (f->isModified()) modified++;

            auto modal = vbox({
                text(" Save Changes ") | bold | color(Color::Green),
                separator(),
                text(" Save changes to " + std::to_string(modified) + " file(s)?"),
                separator(),
                text(" [y] Yes  [n] No") | dim,
            }) | border | size(WIDTH, LESS_THAN, 50);

            return dbox({
                content,
                modal | center,
            });
        }

        return content;
    });

    app.Loop(modalRenderer);
}
