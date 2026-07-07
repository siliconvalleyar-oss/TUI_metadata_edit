#include "mp3file.h"
#include "id3tag.h"
#include <algorithm>
#include <filesystem>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

MP3File::MP3File() {}

MP3File::MP3File(const std::string &filePath)
    : m_filePath(filePath) {}

MP3File::~MP3File() {}

std::string MP3File::lastModified() const {
    return m_lastModified;
}

bool MP3File::load() {
    fs::path p(m_filePath);
    if (!fs::exists(p) || !fs::is_regular_file(p))
        return false;

    m_fileName = p.filename().string();
    m_fileSize = fs::file_size(p);

    auto ftime = fs::last_write_time(p);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    auto tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm *tm_info = std::localtime(&tt);
    std::ostringstream oss;
    oss << std::put_time(tm_info, "%Y-%m-%d %H:%M");
    m_lastModified = oss.str();

    ID3Tag tag;
    if (!tag.load(m_filePath))
        return false;

    m_artist = tag.artist();
    m_album = tag.album();
    m_title = tag.title();
    m_year = tag.year();
    m_genre = tag.genre();
    m_track = tag.track();
    m_comment = tag.comment();
    m_composer = tag.composer();
    m_albumArt = tag.albumArt();

    m_modified = false;
    m_removeArt = false;
    m_renameFile = false;
    m_newFileName.clear();

    if (m_title.empty())
        m_title = p.stem().string();

    m_hasBackup = true;
    m_origArtist = m_artist;
    m_origAlbum = m_album;
    m_origTitle = m_title;
    m_origYear = m_year;
    m_origGenre = m_genre;
    m_origTrack = m_track;
    m_origComment = m_comment;
    m_origComposer = m_composer;
    m_origAlbumArt = m_albumArt;

    return true;
}

bool MP3File::save() {
    if (!m_modified)
        return true;

    ID3Tag tag;
    tag.setArtist(m_artist);
    tag.setAlbum(m_album);
    tag.setTitle(m_title);
    tag.setYear(m_year);
    tag.setGenre(m_genre);
    tag.setTrack(m_track);
    tag.setComment(m_comment);
    tag.setComposer(m_composer);

    if (m_removeArt)
        tag.removeAlbumArt();
    else if (!m_albumArt.empty())
        tag.setAlbumArt(m_albumArt);

    if (!tag.save(m_filePath))
        return false;

    if (m_renameFile && !m_newFileName.empty()) {
        fs::path p(m_filePath);
        fs::path newPath = p.parent_path() / m_newFileName;

        std::string ext = newPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".mp3")
            newPath += ".mp3";

        if (newPath != p) {
            if (fs::exists(newPath))
                fs::remove(newPath);
            fs::rename(p, newPath);
            m_filePath = newPath.string();
            m_fileName = newPath.filename().string();
        }
    }

    m_modified = false;
    m_renameFile = false;
    return true;
}

void MP3File::setArtist(const std::string &v) { m_artist = v; m_modified = true; }
void MP3File::setAlbum(const std::string &v) { m_album = v; m_modified = true; }
void MP3File::setTitle(const std::string &v) { m_title = v; m_modified = true; }
void MP3File::setYear(const std::string &v) { m_year = v; m_modified = true; }
void MP3File::setGenre(const std::string &v) { m_genre = v; m_modified = true; }
void MP3File::setTrack(const std::string &v) { m_track = v; m_modified = true; }
void MP3File::setComment(const std::string &v) { m_comment = v; m_modified = true; }
void MP3File::setComposer(const std::string &v) { m_composer = v; m_modified = true; }
void MP3File::setAlbumArt(const std::vector<uint8_t> &v) { m_albumArt = v; m_modified = true; m_removeArt = false; }
void MP3File::removeAlbumArt() { m_albumArt.clear(); m_removeArt = true; m_modified = true; }

void MP3File::discardChanges() {
    if (!m_hasBackup)
        return;
    m_artist = m_origArtist;
    m_album = m_origAlbum;
    m_title = m_origTitle;
    m_year = m_origYear;
    m_genre = m_origGenre;
    m_track = m_origTrack;
    m_comment = m_origComment;
    m_composer = m_origComposer;
    m_albumArt = m_origAlbumArt;
    m_modified = false;
    m_removeArt = false;
    m_renameFile = false;
    m_newFileName.clear();
}
