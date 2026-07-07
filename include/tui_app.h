#ifndef TUI_APP_H
#define TUI_APP_H

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <memory>
#include <string>
#include <vector>
#include "mp3file.h"

struct DirEntry {
    std::string name;
    bool isDir;
};

class TuiApp {
public:
    TuiApp();
    ~TuiApp();

    void loadFile(const std::string &path);
    void loadFolder(const std::string &dir);
    void browseDirectory(const std::string &dir);
    void run();

private:
    ftxui::Component buildToolbar();
    ftxui::Component buildTable();
    ftxui::Component buildDetailPanel();
    ftxui::Component buildStatusBar();

    ftxui::Element renderFileTable();
    ftxui::Element renderDetailPanel();
    ftxui::Element renderStatusBar();

    void onAddFiles();
    void onAddFolder();
    void onClearAll();
    void onApply();
    void onDiscardChanges();
    void onRemoveArt();
    void onToggleSelectAll();
    void updateStatusBar();
    std::vector<MP3File*> selectedFiles() const;

    // File browser
    void enterEntry();
    void goUpDirectory();
    void syncEditFields();
    MP3File* selectedFile() const;

    std::vector<std::unique_ptr<MP3File>> m_files;
    int m_selectedRow = -1;
    int m_scrollOffset = 0;
    int m_tableHeight = 20;
    bool m_selectAll = false;
    bool m_editing = false;
    int m_editFieldIndex = 0;
    std::string m_statusMessage;

    std::string m_editTitle;
    std::string m_editArtist;
    std::string m_editAlbum;
    std::string m_editYear;
    std::string m_editTrack;
    std::string m_editGenre;
    std::string m_editComposer;
    std::string m_editComment;
    std::string m_editFileName;

    std::string m_toolbarMessage;
    bool m_showAddFiles = false;
    bool m_showAddFolder = false;
    bool m_showConfirmDiscard = false;
    bool m_showConfirmApply = false;
    bool m_showAbout = false;
    std::string m_inputPath;

    // File browser state
    std::string m_currentDir;
    std::vector<DirEntry> m_entries;

    ftxui::Box m_tableBox;
    ftxui::Box m_detailBox;

    ftxui::Component m_component;
    ftxui::Component m_toolbarComponent;
    ftxui::Component m_tableComponent;
    ftxui::Component m_detailComponent;
    ftxui::Component m_statusComponent;
};

#endif
