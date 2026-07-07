#ifndef MP3FILE_H
#define MP3FILE_H

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <ctime>

class ID3Tag;

class MP3File {
public:
    MP3File();
    explicit MP3File(const std::string &filePath);
    ~MP3File();

    bool load();
    bool save();

    std::string filePath() const { return m_filePath; }
    std::string fileName() const { return m_fileName; }
    int64_t fileSize() const { return m_fileSize; }
    std::string lastModified() const;

    std::string artist() const { return m_artist; }
    std::string album() const { return m_album; }
    std::string title() const { return m_title; }
    std::string year() const { return m_year; }
    std::string genre() const { return m_genre; }
    std::string track() const { return m_track; }
    std::string comment() const { return m_comment; }
    std::string composer() const { return m_composer; }
    const std::vector<uint8_t>& albumArt() const { return m_albumArt; }
    bool hasAlbumArt() const { return !m_albumArt.empty(); }

    void setArtist(const std::string &v);
    void setAlbum(const std::string &v);
    void setTitle(const std::string &v);
    void setYear(const std::string &v);
    void setGenre(const std::string &v);
    void setTrack(const std::string &v);
    void setComment(const std::string &v);
    void setComposer(const std::string &v);
    void setAlbumArt(const std::vector<uint8_t> &v);
    void removeAlbumArt();
    bool removeArtFlag() const { return m_removeArt; }

    bool isModified() const { return m_modified; }
    void setModified(bool v) { m_modified = v; }

    void discardChanges();

    std::string newFileName() const { return m_newFileName; }
    void setNewFileName(const std::string &v) {
        m_newFileName = v;
        m_renameFile = !v.empty();
    }

private:
    std::string m_filePath;
    std::string m_fileName;
    int64_t m_fileSize = 0;
    std::string m_lastModified;

    std::string m_artist;
    std::string m_album;
    std::string m_title;
    std::string m_year;
    std::string m_genre;
    std::string m_track;
    std::string m_comment;
    std::string m_composer;
    std::vector<uint8_t> m_albumArt;

    bool m_modified = false;
    bool m_removeArt = false;
    bool m_renameFile = false;
    std::string m_newFileName;

    bool m_hasBackup = false;
    std::string m_origArtist;
    std::string m_origAlbum;
    std::string m_origTitle;
    std::string m_origYear;
    std::string m_origGenre;
    std::string m_origTrack;
    std::string m_origComment;
    std::string m_origComposer;
    std::vector<uint8_t> m_origAlbumArt;
};

#endif
