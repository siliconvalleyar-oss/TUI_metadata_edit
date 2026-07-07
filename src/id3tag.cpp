#include "id3tag.h"
#include <fstream>
#include <cstring>
#include <algorithm>

static const int ID3V1_TAG_SIZE = 128;
static const int ID3V2_HEADER_SIZE = 10;
static const int FRAME_HEADER_SIZE = 10;

ID3Tag::ID3Tag() {}

bool ID3Tag::isValidImageData(const std::vector<uint8_t> &data) {
    if (data.size() < 4) return false;
    if (data[0] == 0xFF && data[1] == 0xD8) return true;  // JPEG
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) return true;  // PNG
    return false;
}

bool ID3Tag::load(const std::string &filePath) {
    m_filePath = filePath;
    m_artist.clear();
    m_album.clear();
    m_title.clear();
    m_year.clear();
    m_genre.clear();
    m_track.clear();
    m_comment.clear();
    m_composer.clear();
    m_albumArt.clear();
    m_dirty = false;
    m_removeArt = false;
    m_hasID3v1 = false;
    m_hasID3v2 = false;
    m_rawID3v2.clear();

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;

    file.seekg(0, std::ios::end);
    int64_t fileSize = file.tellg();
    if (fileSize < 4)
        return false;

    if (fileSize >= ID3V1_TAG_SIZE)
        readID3v1(file, fileSize);

    file.seekg(0);
    readID3v2(file, fileSize);

    file.close();
    return true;
}

bool ID3Tag::save(const std::string &filePath) {
    if (!m_dirty)
        return true;

    std::ifstream inFile(filePath, std::ios::binary | std::ios::ate);
    if (!inFile.is_open())
        return false;

    int64_t fileSize = inFile.tellg();
    inFile.seekg(0);

    std::vector<uint8_t> existingContent(fileSize);
    inFile.read(reinterpret_cast<char*>(existingContent.data()), fileSize);
    inFile.close();

    int dataStart = 0;
    if (existingContent.size() >= (size_t)ID3V2_HEADER_SIZE &&
        memcmp(existingContent.data(), "ID3", 3) == 0) {
        uint32_t tagSz = syncSafeToInt(existingContent.data() + 6);
        dataStart = ID3V2_HEADER_SIZE + tagSz;
        if ((existingContent[5] & 0x10))
            dataStart += 10;
    }

    std::vector<uint8_t> audioData(existingContent.begin() + dataStart, existingContent.end());

    if (audioData.size() >= (size_t)ID3V1_TAG_SIZE) {
        if (memcmp(audioData.data() + audioData.size() - 3, "TAG", 3) == 0)
            audioData.resize(audioData.size() - ID3V1_TAG_SIZE);
    }

    std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open())
        return false;

    std::vector<uint8_t> v2tag = buildID3v2Tag();
    if (!v2tag.empty())
        outFile.write(reinterpret_cast<const char*>(v2tag.data()), v2tag.size());

    outFile.write(reinterpret_cast<const char*>(audioData.data()), audioData.size());

    std::vector<uint8_t> v1tag;
    v1tag.insert(v1tag.end(), "TAG", "TAG" + 3);

    auto padLatin1 = [](const std::string &s, int len) {
        std::vector<uint8_t> result(len, 0);
        for (int i = 0; i < len && i < (int)s.size(); i++)
            result[i] = static_cast<uint8_t>(s[i]);
        return result;
    };

    auto v1title = padLatin1(m_title, 30);
    v1tag.insert(v1tag.end(), v1title.begin(), v1title.end());
    auto v1artist = padLatin1(m_artist, 30);
    v1tag.insert(v1tag.end(), v1artist.begin(), v1artist.end());
    auto v1album = padLatin1(m_album, 30);
    v1tag.insert(v1tag.end(), v1album.begin(), v1album.end());
    auto v1year = padLatin1(m_year, 4);
    v1tag.insert(v1tag.end(), v1year.begin(), v1year.end());

    auto v1comment = padLatin1(m_comment, 28);
    v1tag.insert(v1tag.end(), v1comment.begin(), v1comment.end());
    v1tag.push_back(0);

    bool ok = false;
    int trackNum = 0;
    try { trackNum = std::stoi(m_track); ok = true; } catch (...) {}
    v1tag.push_back(static_cast<uint8_t>(ok && trackNum > 0 && trackNum < 256 ? trackNum : 0));

    bool genreOk = false;
    int genreNum = 255;
    try { genreNum = std::stoi(m_genre); genreOk = true; } catch (...) {}
    v1tag.push_back(static_cast<uint8_t>(genreOk ? genreNum : 255));

    outFile.write(reinterpret_cast<const char*>(v1tag.data()), v1tag.size());
    outFile.close();
    return true;
}

bool ID3Tag::readID3v1(std::ifstream &file, int64_t fileSize) {
    file.seekg(fileSize - ID3V1_TAG_SIZE);
    uint8_t tag[ID3V1_TAG_SIZE];
    file.read(reinterpret_cast<char*>(tag), ID3V1_TAG_SIZE);
    if (file.gcount() < ID3V1_TAG_SIZE)
        return false;
    if (memcmp(tag, "TAG", 3) != 0)
        return false;

    m_hasID3v1 = true;

    auto readStr = [&](int offset, int len) -> std::string {
        int nullPos = len;
        for (int i = 0; i < len; i++) {
            if (tag[offset + i] == 0) { nullPos = i; break; }
        }
        return std::string(reinterpret_cast<const char*>(tag + offset), nullPos);
    };

    m_title = readStr(3, 30);
    m_artist = readStr(33, 30);
    m_album = readStr(63, 30);

    std::string rawYear = readStr(93, 4);
    rawYear.erase(0, rawYear.find_first_not_of(' '));
    rawYear.erase(rawYear.find_last_not_of(' ') + 1);
    m_year = rawYear;

    if (tag[125] == 0 && tag[126] != 0) {
        m_track = std::to_string(tag[126]);
        m_comment = readStr(97, 28);
    } else {
        m_comment = readStr(97, 30);
    }

    m_genre = std::to_string(tag[127]);
    return true;
}

bool ID3Tag::readID3v2(std::ifstream &file, int64_t fileSize) {
    uint8_t header[ID3V2_HEADER_SIZE];
    file.read(reinterpret_cast<char*>(header), ID3V2_HEADER_SIZE);
    if (file.gcount() < ID3V2_HEADER_SIZE)
        return false;
    if (memcmp(header, "ID3", 3) != 0)
        return false;

    m_hasID3v2 = true;

    bool isSyncSafe = (header[3] >= 4);
    uint32_t tagSize = syncSafeToInt(header + 6);

    if (tagSize > (uint32_t)(fileSize - ID3V2_HEADER_SIZE))
        tagSize = fileSize - ID3V2_HEADER_SIZE;

    m_rawID3v2.resize(tagSize);
    file.read(reinterpret_cast<char*>(m_rawID3v2.data()), tagSize);
    parseID3v2Frames(m_rawID3v2, tagSize, isSyncSafe, header[5]);
    return true;
}

void ID3Tag::parseID3v2Frames(const std::vector<uint8_t> &data, uint32_t size, bool isSyncSafe, uint8_t flags) {
    uint32_t pos = 0;

    if (flags & 0x40) {
        if (size >= 4) {
            uint32_t extSize;
            if (isSyncSafe)
                extSize = syncSafeToInt(data.data());
            else
                extSize = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                          ((uint32_t)data[2] << 8) | (uint32_t)data[3];
            pos = extSize + 4;
            if (pos > size) pos = size;
        }
    }

    while (pos + FRAME_HEADER_SIZE <= size && data.size() >= pos + FRAME_HEADER_SIZE) {
        FrameHeader fhdr;
        memcpy(&fhdr, data.data() + pos, FRAME_HEADER_SIZE);

        if (fhdr.id[0] == 0)
            break;

        uint32_t frameSz = frameSize(reinterpret_cast<const uint8_t*>(&fhdr.size), isSyncSafe);

        pos += FRAME_HEADER_SIZE;
        if (pos + frameSz > size || frameSz == 0)
            break;

        std::vector<uint8_t> frameData(data.begin() + pos, data.begin() + pos + frameSz);

        if (memcmp(fhdr.id, "APIC", 4) == 0) {
            if (frameData.size() > 1) {
                uint8_t enc = frameData[0];
                int offset = 1;

                int mimeEnd = -1;
                for (size_t i = offset; i < frameData.size(); i++) {
                    if (frameData[i] == 0) { mimeEnd = i; break; }
                }
                if (mimeEnd < 0) break;
                std::string mimeType(frameData.begin() + offset, frameData.begin() + mimeEnd);
                offset = mimeEnd + 1;

                if (offset >= (int)frameData.size()) break;
                offset++;  // skip picture type byte

                if (enc == 0 || enc == 3) {
                    int descEnd = -1;
                    for (size_t i = offset; i < frameData.size(); i++) {
                        if (frameData[i] == 0) { descEnd = i; break; }
                    }
                    if (descEnd < 0) break;
                    offset = descEnd + 1;
                } else {
                    if (offset + 1 >= (int)frameData.size()) break;
                    if (frameData[offset] == 0xFF && frameData[offset+1] == 0xFE) {
                        offset += 2;
                    }
                    int descEnd = -1;
                    for (size_t i = offset; i + 1 < frameData.size(); i++) {
                        if (frameData[i] == 0 && frameData[i+1] == 0) { descEnd = i; break; }
                    }
                    if (descEnd < 0) break;
                    offset = descEnd + 2;
                }

                if (offset < (int)frameData.size()) {
                    std::vector<uint8_t> artData(frameData.begin() + offset, frameData.end());
                    if (isValidImageData(artData)) {
                        m_albumArt = artData;
                    }
                }
            }
        } else {
            std::string frameId(fhdr.id, 4);
            std::string value;
            if (frameData.size() > 1) {
                uint8_t enc = frameData[0];
                int offset = 1;
                value = decodeText(frameData, offset, enc);
            }

            if (frameId == "TPE1") m_artist = value;
            else if (frameId == "TALB") m_album = value;
            else if (frameId == "TIT2") m_title = value;
            else if (frameId == "TYER" || frameId == "TDRC") m_year = value;
            else if (frameId == "TCON") m_genre = value;
            else if (frameId == "TRCK") m_track = value;
            else if (frameId == "COMM") m_comment = value;
            else if (frameId == "TCOM") m_composer = value;
        }

        pos += frameSz;
    }
}

std::string ID3Tag::decodeText(const std::vector<uint8_t> &data, int &offset, uint8_t encoding, int length) {
    if (offset >= (int)data.size())
        return {};

    int available = (length < 0) ? ((int)data.size() - offset) : length;

    switch (encoding) {
    case 0: // ISO-8859-1
    {
        int end = offset + available;
        for (int i = offset; i < offset + available && i < (int)data.size(); i++) {
            if (data[i] == 0) { end = i; break; }
        }
        std::string result(data.begin() + offset, data.begin() + end);
        offset = (end < (int)data.size() - 1) ? end + 1 : data.size();
        return result;
    }
    case 1: // UTF-16 with BOM
    case 2: // UTF-16BE
    {
        int bomSize = 0;
        if (encoding == 1 && offset + 1 < (int)data.size()) {
            if (data[offset] == 0xFF && data[offset+1] == 0xFE)
                bomSize = 2;
            else if (data[offset] == 0xFE && data[offset+1] == 0xFF)
                bomSize = 2;
        }

        int searchStart = offset + bomSize;
        int end = searchStart;
        while (end + 1 < (int)data.size() && (end - searchStart) < available) {
            if (data[end] == 0 && data[end+1] == 0)
                break;
            end += 2;
        }
        if (end >= (int)data.size()) end = data.size() - 2;
        if (end < searchStart) end = searchStart;

        int len = end - searchStart;
        if (len > 0 && len % 2 == 0) {
            bool isBE = (encoding == 2) || (encoding == 1 && bomSize > 0 && data[offset] == 0xFE);

            std::string result;
            for (int i = searchStart; i < end; i += 2) {
                uint16_t codeUnit;
                if (isBE)
                    codeUnit = ((uint16_t)data[i] << 8) | data[i+1];
                else
                    codeUnit = data[i] | ((uint16_t)data[i+1] << 8);

                if (codeUnit < 0x80) {
                    result += static_cast<char>(codeUnit);
                } else if (codeUnit < 0x800) {
                    result += static_cast<char>(0xC0 | (codeUnit >> 6));
                    result += static_cast<char>(0x80 | (codeUnit & 0x3F));
                } else {
                    result += static_cast<char>(0xE0 | (codeUnit >> 12));
                    result += static_cast<char>(0x80 | ((codeUnit >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codeUnit & 0x3F));
                }
            }
            offset = end + 2;
            return result;
        }
        offset = end + 2;
        return {};
    }
    case 3: // UTF-8
    {
        int end = offset + available;
        for (int i = offset; i < offset + available && i < (int)data.size(); i++) {
            if (data[i] == 0) { end = i; break; }
        }
        std::string result(data.begin() + offset, data.begin() + end);
        offset = (end < (int)data.size() - 1) ? end + 1 : data.size();
        return result;
    }
    }
    return {};
}

std::vector<uint8_t> ID3Tag::encodeText(const std::string &text, uint8_t /*encoding*/) {
    std::vector<uint8_t> result(text.begin(), text.end());
    result.push_back(0);
    return result;
}

uint32_t ID3Tag::syncSafeToInt(const uint8_t *data) {
    return ((uint32_t)data[0] << 21) |
           ((uint32_t)data[1] << 14) |
           ((uint32_t)data[2] << 7) |
           ((uint32_t)data[3]);
}

void ID3Tag::intToSyncSafe(uint32_t value, uint8_t *data) {
    data[0] = (value >> 21) & 0x7F;
    data[1] = (value >> 14) & 0x7F;
    data[2] = (value >> 7) & 0x7F;
    data[3] = value & 0x7F;
}

uint32_t ID3Tag::frameSize(const uint8_t *data, bool isSyncSafe) {
    if (isSyncSafe)
        return syncSafeToInt(data);
    return (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 |
           (uint32_t)data[2] << 8 | (uint32_t)data[3];
}

std::vector<uint8_t> ID3Tag::buildFrame(const char *frameId, const std::string &text, uint8_t encoding) {
    if (text.empty())
        return {};

    std::vector<uint8_t> content;
    content.push_back(encoding);
    auto encoded = encodeText(text, encoding);
    content.insert(content.end(), encoded.begin(), encoded.end());

    std::vector<uint8_t> frame;
    frame.insert(frame.end(), frameId, frameId + 4);

    uint32_t sz = content.size();
    frame.push_back((sz >> 24) & 0xFF);
    frame.push_back((sz >> 16) & 0xFF);
    frame.push_back((sz >> 8) & 0xFF);
    frame.push_back(sz & 0xFF);

    frame.push_back(0);
    frame.push_back(0);
    frame.insert(frame.end(), content.begin(), content.end());
    return frame;
}

std::vector<uint8_t> ID3Tag::buildAPICFrame() {
    if (m_albumArt.empty())
        return {};

    std::vector<uint8_t> content;
    content.push_back(0);

    if (!isValidImageData(m_albumArt))
        return {};

    std::string mimeType;
    if (m_albumArt[0] == 0xFF && m_albumArt[1] == 0xD8)
        mimeType = "image/jpeg";
    else if (m_albumArt[0] == 0x89 && m_albumArt[1] == 0x50)
        mimeType = "image/png";
    else
        return {};

    content.insert(content.end(), mimeType.begin(), mimeType.end());
    content.push_back(0);
    content.push_back(3);
    content.push_back(0);
    content.insert(content.end(), m_albumArt.begin(), m_albumArt.end());

    std::vector<uint8_t> frame;
    frame.insert(frame.end(), "APIC", "APIC" + 4);
    uint32_t sz = content.size();
    frame.push_back((sz >> 24) & 0xFF);
    frame.push_back((sz >> 16) & 0xFF);
    frame.push_back((sz >> 8) & 0xFF);
    frame.push_back(sz & 0xFF);
    frame.push_back(0);
    frame.push_back(0);
    frame.insert(frame.end(), content.begin(), content.end());
    return frame;
}

std::vector<uint8_t> ID3Tag::buildID3v2Tag() {
    std::vector<uint8_t> frames;

    auto addFrame = [&](const char *id, const std::string &val) {
        auto f = buildFrame(id, val);
        if (!f.empty()) frames.insert(frames.end(), f.begin(), f.end());
    };

    addFrame("TPE1", m_artist);
    addFrame("TALB", m_album);
    addFrame("TIT2", m_title);
    addFrame("TDRC", m_year);
    addFrame("TCON", m_genre);
    addFrame("TRCK", m_track);
    addFrame("TCOM", m_composer);
    addFrame("COMM", m_comment);

    if (!m_removeArt && !m_albumArt.empty()) {
        auto f = buildAPICFrame();
        if (!f.empty()) frames.insert(frames.end(), f.begin(), f.end());
    }

    if (frames.empty())
        return {};

    uint32_t tagSize = frames.size();
    std::vector<uint8_t> header;
    header.insert(header.end(), "ID3", "ID3" + 3);
    header.push_back(3);
    header.push_back(0);
    header.push_back(0);
    header.push_back((tagSize >> 21) & 0x7F);
    header.push_back((tagSize >> 14) & 0x7F);
    header.push_back((tagSize >> 7) & 0x7F);
    header.push_back(tagSize & 0x7F);

    header.insert(header.end(), frames.begin(), frames.end());
    return header;
}
