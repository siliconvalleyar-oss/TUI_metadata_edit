# API Reference

## MP3File

Clase de dominio que representa un archivo MP3 con sus metadatos.

### Constructor

```cpp
MP3File();
explicit MP3File(const std::string &filePath);
```

### Metodos principales

| Metodo | Retorno | Descripcion |
|--------|---------|-------------|
| `load()` | `bool` | Lee metadatos del disco via ID3Tag |
| `save()` | `bool` | Escribe metadatos al disco |

### Getters (inline)

| Metodo | Retorno | Campo ID3 |
|--------|---------|-----------|
| `filePath()` | `std::string` | Ruta completa |
| `fileName()` | `std::string` | Nombre del archivo |
| `fileSize()` | `int64_t` | Tamano en bytes |
| `lastModified()` | `std::string` | Fecha de modificacion (YYYY-MM-DD HH:MM) |
| `title()` | `std::string` | TIT2 |
| `artist()` | `std::string` | TPE1 |
| `album()` | `std::string` | TALB |
| `year()` | `std::string` | TDRC/TYER |
| `genre()` | `std::string` | TCON |
| `track()` | `std::string` | TRCK |
| `comment()` | `std::string` | COMM |
| `composer()` | `std::string` | TCOM |
| `albumArt()` | `const std::vector<uint8_t>&` | APIC (imagen raw) |
| `hasAlbumArt()` | `bool` | `!albumArt().empty()` |
| `isModified()` | `bool` | Flag de modificacion |
| `newFileName()` | `std::string` | Nombre para renombrado |

### Setters

Cada setter marca `m_modified = true`:

```cpp
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
void setNewFileName(const std::string &v);  // tambien marca m_renameFile
void setModified(bool v);
```

---

## TuiApp

Controlador principal de la interfaz TUI.

### Constructor

```cpp
TuiApp();
~TuiApp();
```

### Metodos principales

| Metodo | Retorno | Descripcion |
|--------|---------|---|
| `loadFile(const std::string&)` | `void` | Carga un archivo MP3 individual |
| `loadFolder(const std::string&)` | `void` | Carga recursivamente todos los .mp3 de una carpeta |
| `run()` | `void` | Inicia el loop principal de la interfaz TUI |

---

## ID3Tag

Capa de lectura/escritura de tags ID3 en archivos MP3.

### Metodos principales

| Metodo | Retorno | Descripcion |
|--------|---------|---|
| `load(const std::string&)` | `bool` | Lee ID3v1 y ID3v2 del archivo |
| `save(const std::string&)` | `bool` | Escribe ID3v1 e ID3v2 al archivo |

### Getters/Setters

Mismos campos que MP3File (artist, album, title, year, genre, track, comment, composer, albumArt).

### Frames ID3v2 soportados

| Frame | Campo |
|-------|-------|
| TPE1 | Artista |
| TALB | Album |
| TIT2 | Titulo |
| TDRC / TYER | Anio |
| TCON | Genero |
| TRCK | Pista |
| TCOM | Compositor |
| COMM | Comentario |
| APIC | Caratula de album |

### Codificaciones soportadas

- `0x00`: ISO-8859-1
- `0x01`: UTF-16 con BOM
- `0x02`: UTF-16BE
- `0x03`: UTF-8

### Metodos internos

```cpp
bool readID3v1(std::ifstream &file, int64_t fileSize);
bool readID3v2(std::ifstream &file, int64_t fileSize);
void parseID3v2Frames(...);
std::string decodeText(...);
std::vector<uint8_t> encodeText(...);
std::vector<uint8_t> buildID3v2Tag();
std::vector<uint8_t> buildFrame(const char*, const std::string&, uint8_t);
std::vector<uint8_t> buildAPICFrame();
static bool isValidImageData(const std::vector<uint8_t> &data);
```
