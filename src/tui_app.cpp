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
    }) | border | size(WIDTH, EQUAL, 60);
}

Element TuiApp::renderFileTable() {
    Elements rows;

    // Toolbar (visual only)
    rows.push_back(hbox({
        text(" MP3 Metadata Editor ") | bold | color(Color::Cyan),
        separator(),
        text(" [a]Add [f]Folder [x]Clear [s]SelectAll ") | dim,
    }));

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
        text(" [q]Quit [Enter]Open/Edit [e]Edit [a]Add [f]Folder [Ctrl+S]Save [d]Discard ") |
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

    // Build components - toolbar is VISUAL ONLY, not in focus chain
    auto table = buildTable();
    auto detail = buildDetailPanel();
    auto status = buildStatusBar();

    // Left panel: just the table (toolbar is rendered inside table)
    auto leftPanel = table;

    // Right panel: detail
    auto rightPanel = detail;

    // Main layout with split
    int splitSize = 55;
    auto mainContainer = ResizableSplitLeft(
        leftPanel,
        rightPanel,
        &splitSize
    );

    auto app = App::Fullscreen();

    // Single event handler for ALL keyboard/mouse events
    // This is the ONLY place that processes user input
    auto wrapped = mainContainer | CatchEvent([this, &app](Event event) {
        // ---- Modal dialogs consume ALL events ----
        if (m_showAddFiles || m_showAddFolder ||
            m_showConfirmDiscard || m_showConfirmApply) {

            if (m_showAddFiles) {
                if (event == Event::Escape) { m_showAddFiles = false; return true; }
                if (event == Event::Backspace && !m_inputPath.empty()) {
                    m_inputPath.pop_back(); return true;
                }
                if (event == Event::Return) {
                    std::istringstream iss(m_inputPath);
                    std::string token;
                    while (iss >> token) loadFile(token);
                    m_showAddFiles = false; m_inputPath.clear();
                    updateStatusBar(); return true;
                }
                if (event.is_character()) { m_inputPath += event.character(); return true; }
                return true;
            }

            if (m_showAddFolder) {
                if (event == Event::Escape) { m_showAddFolder = false; return true; }
                if (event == Event::Backspace && !m_inputPath.empty()) {
                    m_inputPath.pop_back(); return true;
                }
                if (event == Event::Return) {
                    browseDirectory(m_inputPath);
                    m_showAddFolder = false; m_inputPath.clear();
                    updateStatusBar(); return true;
                }
                if (event.is_character()) { m_inputPath += event.character(); return true; }
                return true;
            }

            if (m_showConfirmDiscard) {
                if (event == Event::Character('y') || event == Event::Return) {
                    for (auto &f : m_files) if (f->isModified()) f->discardChanges();
                    m_showConfirmDiscard = false;
                    m_toolbarMessage = "Changes discarded.";
                    syncEditFields(); updateStatusBar(); return true;
                }
                if (event == Event::Character('n') || event == Event::Escape) {
                    m_showConfirmDiscard = false; return true;
                }
                return true;
            }

            if (m_showConfirmApply) {
                if (event == Event::Character('y') || event == Event::Return) {
                    int success = 0, failed = 0;
                    for (auto &f : m_files) {
                        if (f->isModified()) { if (f->save()) success++; else failed++; }
                    }
                    m_showConfirmApply = false;
                    std::ostringstream oss;
                    oss << "Saved: " << success << " ok, " << failed << " failed.";
                    m_toolbarMessage = oss.str();
                    syncEditFields(); updateStatusBar(); return true;
                }
                if (event == Event::Character('n') || event == Event::Escape) {
                    m_showConfirmApply = false; return true;
                }
                return true;
            }
        }

        // ---- Mouse events ----
        if (event.is_mouse()) {
            auto &m = event.mouse();

            // Click on table: select row, open dirs immediately
            if (m.button == Mouse::Left && m.motion == Mouse::Pressed) {
                int relY = m.y - m_tableBox.y_min - 3; // 3 rows: toolbar + header + separator
                int clickedRow = m_scrollOffset + relY;
                if (clickedRow >= 0 && clickedRow < (int)m_entries.size()) {
                    m_selectedRow = clickedRow;
                    syncEditFields();
                    // If clicked on a directory, navigate into it
                    if (m_entries[m_selectedRow].isDir) {
                        enterEntry();
                    }
                    updateStatusBar(); return true;
                }
                // Click on detail panel: enter edit on field
                if (m.x >= m_detailBox.x_min && m.x <= m_detailBox.x_max &&
                    m.y >= m_detailBox.y_min && m.y <= m_detailBox.y_max) {
                    if (selectedFile()) {
                        int relDY = m.y - m_detailBox.y_min;
                        int fieldIdx = relDY - 5; // header rows
                        if (fieldIdx >= 0 && fieldIdx < 8) {
                            m_editFieldIndex = fieldIdx;
                            m_editing = true;
                        }
                    }
                    return true;
                }
                return true;
            }

            // Mouse wheel
            if (m.button == Mouse::WheelUp) {
                if (m_selectedRow > 0) {
                    m_selectedRow--;
                    if (m_selectedRow < m_scrollOffset) m_scrollOffset = m_selectedRow;
                    syncEditFields(); updateStatusBar();
                }
                return true;
            }
            if (m.button == Mouse::WheelDown) {
                if (m_selectedRow < (int)m_entries.size() - 1) {
                    m_selectedRow++;
                    if (m_selectedRow >= m_scrollOffset + m_tableHeight)
                        m_scrollOffset = m_selectedRow - m_tableHeight + 1;
                    syncEditFields(); updateStatusBar();
                }
                return true;
            }
            return true; // consume all other mouse events
        }

        // ---- Global shortcuts: ALWAYS active, even during edit mode ----
        if (event == Event::CtrlC || event == Event::Character('q')) {
            app.Exit(); return true;
        }
        if (event == Event::CtrlS) {
            onApply(); return true;
        }
        if (event == Event::Character('d') && !m_editing) {
            onDiscardChanges(); return true;
        }

        // ---- Edit mode: text input ----
        if (m_editing) {
            if (event == Event::Escape) { m_editing = false; return true; }
            if (event == Event::Tab || event == Event::ArrowDown) {
                m_editFieldIndex = (m_editFieldIndex + 1) % 8; return true;
            }
            if (event == Event::TabReverse || event == Event::ArrowUp) {
                m_editFieldIndex = (m_editFieldIndex + 7) % 8; return true;
            }
            if (event == Event::Return) {
                // Apply current field and move to next
                std::string *fields[] = {
                    &m_editTitle, &m_editArtist, &m_editAlbum, &m_editYear,
                    &m_editTrack, &m_editGenre, &m_editComposer, &m_editComment
                };
                for (auto *file : selectedFiles()) {
                    switch (m_editFieldIndex) {
                        case 0: file->setTitle(*fields[0]); break;
                        case 1: file->setArtist(*fields[1]); break;
                        case 2: file->setAlbum(*fields[2]); break;
                        case 3: file->setYear(*fields[3]); break;
                        case 4: file->setTrack(*fields[4]); break;
                        case 5: file->setGenre(*fields[5]); break;
                        case 6: file->setComposer(*fields[6]); break;
                        case 7: file->setComment(*fields[7]); break;
                    }
                }
                m_editFieldIndex = (m_editFieldIndex + 1) % 8;
                return true;
            }
            if (event == Event::Backspace) {
                std::string *fields[] = {
                    &m_editTitle, &m_editArtist, &m_editAlbum, &m_editYear,
                    &m_editTrack, &m_editGenre, &m_editComposer, &m_editComment
                };
                auto &field = *fields[m_editFieldIndex];
                if (!field.empty()) {
                    field.pop_back();
                    for (auto *file : selectedFiles()) {
                        switch (m_editFieldIndex) {
                            case 0: file->setTitle(field); break;
                            case 1: file->setArtist(field); break;
                            case 2: file->setAlbum(field); break;
                            case 3: file->setYear(field); break;
                            case 4: file->setTrack(field); break;
                            case 5: file->setGenre(field); break;
                            case 6: file->setComposer(field); break;
                            case 7: file->setComment(field); break;
                        }
                    }
                }
                return true;
            }
            if (event.is_character()) {
                std::string *fields[] = {
                    &m_editTitle, &m_editArtist, &m_editAlbum, &m_editYear,
                    &m_editTrack, &m_editGenre, &m_editComposer, &m_editComment
                };
                auto &field = *fields[m_editFieldIndex];
                field += event.character();
                for (auto *file : selectedFiles()) {
                    switch (m_editFieldIndex) {
                        case 0: file->setTitle(field); break;
                        case 1: file->setArtist(field); break;
                        case 2: file->setAlbum(field); break;
                        case 3: file->setYear(field); break;
                        case 4: file->setTrack(field); break;
                        case 5: file->setGenre(field); break;
                        case 6: file->setComposer(field); break;
                        case 7: file->setComment(field); break;
                    }
                }
                return true;
            }
            return true; // consume all in edit mode
        }

        // ---- Navigation mode ----
        int total = (int)m_entries.size();

        if (event == Event::ArrowUp && m_selectedRow > 0) {
            m_selectedRow--;
            if (m_selectedRow < m_scrollOffset) m_scrollOffset = m_selectedRow;
            syncEditFields(); updateStatusBar(); return true;
        }
        if (event == Event::ArrowDown && m_selectedRow < total - 1) {
            m_selectedRow++;
            if (m_selectedRow >= m_scrollOffset + m_tableHeight)
                m_scrollOffset = m_selectedRow - m_tableHeight + 1;
            syncEditFields(); updateStatusBar(); return true;
        }
        if (event == Event::PageUp) {
            m_selectedRow = std::max(0, m_selectedRow - m_tableHeight);
            if (m_selectedRow < m_scrollOffset) m_scrollOffset = m_selectedRow;
            syncEditFields(); updateStatusBar(); return true;
        }
        if (event == Event::PageDown) {
            m_selectedRow = std::min(total - 1, m_selectedRow + m_tableHeight);
            if (m_selectedRow >= m_scrollOffset + m_tableHeight)
                m_scrollOffset = m_selectedRow - m_tableHeight + 1;
            syncEditFields(); updateStatusBar(); return true;
        }
        if (event == Event::Home) {
            m_selectedRow = 0; m_scrollOffset = 0;
            syncEditFields(); updateStatusBar(); return true;
        }
        if (event == Event::End) {
            if (total > 0) {
                m_selectedRow = total - 1;
                if (m_selectedRow >= m_scrollOffset + m_tableHeight)
                    m_scrollOffset = m_selectedRow - m_tableHeight + 1;
            }
            syncEditFields(); updateStatusBar(); return true;
        }

        // Enter: open dir or enter edit mode
        if (event == Event::Return && m_selectedRow >= 0) {
            const DirEntry &e = m_entries[m_selectedRow];
            if (e.isDir) {
                enterEntry(); // navigate into directory
            } else if (selectedFile()) {
                m_editing = true;
                m_editFieldIndex = 0;
            }
            updateStatusBar(); return true;
        }

        // Toolbar shortcuts (only when NOT editing)
        if (!m_editing) {
            if (event == Event::Character('a')) { onAddFiles(); return true; }
            if (event == Event::Character('f')) { onAddFolder(); return true; }
            if (event == Event::Character('x')) { onClearAll(); return true; }
            if (event == Event::Character('s')) { onToggleSelectAll(); return true; }
            if (event == Event::Character('r')) { onRemoveArt(); return true; }
            if (event == Event::Character('e') && selectedFile()) {
                m_editing = true; m_editFieldIndex = 0; return true;
            }
        }

        return false;
    });

    // Render: toolbar is visual-only, rendered inside the table area
    auto fullRenderer = Renderer(wrapped, [this, wrapped] {
        auto content = wrapped->Render();

        // Overlay modals
        if (m_showAddFiles) {
            auto modal = vbox({
                text(" Add MP3 Files ") | bold | color(Color::Cyan),
                separator(),
                text(" Enter file paths (space-separated):"),
                text(" > " + m_inputPath + "_") | color(Color::Yellow),
                separator(),
                text(" [Enter] Load  [Esc] Cancel") | dim,
            }) | border | size(WIDTH, LESS_THAN, 60);
            return dbox({content, modal | center});
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
            return dbox({content, modal | center});
        }
        if (m_showConfirmDiscard) {
            int modified = 0;
            for (auto &f : m_files) if (f->isModified()) modified++;
            auto modal = vbox({
                text(" Discard Changes ") | bold | color(Color::Red),
                separator(),
                text(" Revert changes to " + std::to_string(modified) + " file(s)?"),
                separator(),
                text(" [y] Yes  [n] No") | dim,
            }) | border | size(WIDTH, LESS_THAN, 50);
            return dbox({content, modal | center});
        }
        if (m_showConfirmApply) {
            int modified = 0;
            for (auto &f : m_files) if (f->isModified()) modified++;
            auto modal = vbox({
                text(" Save Changes ") | bold | color(Color::Green),
                separator(),
                text(" Save changes to " + std::to_string(modified) + " file(s)?"),
                separator(),
                text(" [y] Yes  [n] No") | dim,
            }) | border | size(WIDTH, LESS_THAN, 50);
            return dbox({content, modal | center});
        }
        return content;
    });

    app.Loop(fullRenderer);
}
