#ifndef ID3TAG_H
#define ID3TAG_H

#include <string>
#include <vector>
#include <cstdint>

struct ID3v1Tag {
    bool valid = false;
    std::string title;
    std::string artist;
    std::string album;
    std::string year;
    std::string comment;
    int genre = 255;
};

struct APICFrame {
    std::vector<uint8_t> data;
    std::string mimeType;
    std::string description;
    int pictureType = 0;
};

class ID3Tag {
public:
    ID3Tag();

    bool load(const std::string &filePath);
    bool save(const std::string &filePath);

    std::string artist() const { return m_artist; }
    std::string album() const { return m_album; }
    std::string title() const { return m_title; }
    std::string year() const { return m_year; }
    std::string genre() const { return m_genre; }
    std::string track() const { return m_track; }
    std::string comment() const { return m_comment; }
    std::string composer() const { return m_composer; }
    const std::vector<uint8_t>& albumArt() const { return m_albumArt; }

    void setArtist(const std::string &v) { m_artist = v; m_dirty = true; }
    void setAlbum(const std::string &v) { m_album = v; m_dirty = true; }
    void setTitle(const std::string &v) { m_title = v; m_dirty = true; }
    void setYear(const std::string &v) { m_year = v; m_dirty = true; }
    void setGenre(const std::string &v) { m_genre = v; m_dirty = true; }
    void setTrack(const std::string &v) { m_track = v; m_dirty = true; }
    void setComment(const std::string &v) { m_comment = v; m_dirty = true; }
    void setComposer(const std::string &v) { m_composer = v; m_dirty = true; }
    void setAlbumArt(const std::vector<uint8_t> &v) { m_albumArt = v; m_dirty = true; }
    void removeAlbumArt() { m_albumArt.clear(); m_removeArt = true; m_dirty = true; }
    bool hasAlbumArt() const { return !m_albumArt.empty(); }
    bool removeArtFlag() const { return m_removeArt; }
    bool isDirty() const { return m_dirty; }

private:
    struct ID3v2Header {
        char id[3];
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint8_t flags;
        uint32_t size;
    };

    struct FrameHeader {
        char id[4];
        uint32_t size;
        uint16_t flags;
    };

    bool readID3v1(std::ifstream &file, int64_t fileSize);
    bool readID3v2(std::ifstream &file, int64_t fileSize);
    void parseID3v2Frames(const std::vector<uint8_t> &data, uint32_t size, bool isSyncSafe, uint8_t flags);
    std::string decodeText(const std::vector<uint8_t> &data, int &offset, uint8_t encoding, int length = -1);
    std::vector<uint8_t> encodeText(const std::string &text, uint8_t encoding);

    std::vector<uint8_t> buildID3v2Tag();
    std::vector<uint8_t> buildFrame(const char *frameId, const std::string &text, uint8_t encoding = 3);
    std::vector<uint8_t> buildAPICFrame();

    static uint32_t syncSafeToInt(const uint8_t *data);
    static void intToSyncSafe(uint32_t value, uint8_t *data);
    static uint32_t frameSize(const uint8_t *data, bool isSyncSafe);
    static bool isValidImageData(const std::vector<uint8_t> &data);

    std::string m_filePath;
    std::string m_artist;
    std::string m_album;
    std::string m_title;
    std::string m_year;
    std::string m_genre;
    std::string m_track;
    std::string m_comment;
    std::string m_composer;
    std::vector<uint8_t> m_albumArt;
    bool m_dirty = false;
    bool m_removeArt = false;
    bool m_hasID3v1 = false;
    bool m_hasID3v2 = false;
    std::vector<uint8_t> m_rawID3v2;
};

#endif
