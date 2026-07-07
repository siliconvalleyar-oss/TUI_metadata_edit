# Arquitectura del Proyecto

## Vision general

MetadataMP3 TUI es una aplicacion de terminal para leer y modificar metadatos ID3 en archivos MP3. Utiliza ftxui para la interfaz de usuario interactiva en terminal, con un layout de paneles divididos (tabla de archivos a la izquierda, editor de detalle a la derecha).

## Capas

```
┌─────────────────────────────────────────┐
│              main.cpp                   │  Entrada: parsea args, crea TuiApp
├─────────────────────────────────────────┤
│           TuiApp (Interfaz TUI)         │  UI: ftxui components, layout, eventos
│  ┌──────────────┬───────────────────┐   │
│  │  File Table  │  Detail Panel     │   │
│  │  (navegacion)│  (edicion campos) │   │
│  └──────────────┴───────────────────┘   │
├─────────────────────────────────────────┤
│           MP3File (Dominio)             │  Modelo de negocio: metadata de un archivo
├─────────────────────────────────────────┤
│           ID3Tag (Persistencia)         │  Lectura/escritura de tags ID3v1/v2
└─────────────────────────────────────────┘
```

## Clases

### TuiApp (`include/tui_app.h`, `src/tui_app.cpp`)

Controlador principal de la interfaz TUI. Responsabilidades:
- Construccion de la interfaz con ftxui (toolbar, tabla, panel detalle, status bar)
- Manejo de eventos de teclado (navegacion, edicion, atajos)
- Carga de archivos y carpetas via entrada del usuario
- Coordinacion entre tabla y panel de detalle
- Modales de confirmacion (guardar, descartar)
- Album art via chafa (subprocess)

### MP3File (`include/mp3file.h`, `src/mp3file.cpp`)

Modelo de dominio. Responsabilidades:
- Contener los metadatos de un archivo MP3
- Coordina lectura/escritura via `ID3Tag`
- Gestion de estado (modificado, renombrado)
- Renombrado de archivo en disco
- Backup/restore de valores originales

### ID3Tag (`include/id3tag.h`, `src/id3tag.cpp`)

Capa de persistencia. Responsabilidades:
- Lectura de ID3v1 (128 bytes al final del archivo)
- Lectura de ID3v2 (cabecera al inicio, frames parseados)
- Escritura de ID3v1 e ID3v2
- Parseo de frames: TPE1, TALB, TIT2, TDRC, TCON, TRCK, TCOM, COMM, APIC
- Decodificacion de texto: ISO-8859-1, UTF-16 (LE/BE con BOM), UTF-8
- Sync-safe integer conversion para ID3v2.4+

## Flujo de datos

### Carga de archivo
```
TuiApp::loadFile()
  → new MP3File(path)
    → MP3File::load()
      → std::filesystem (file metadata)
      → ID3Tag::load(path)
        → std::ifstream (binary read)
        → readID3v1()     // lee 128 bytes al final
        → readID3v2()     // lee cabecera + frames al inicio
          → parseID3v2Frames()
            → decodeText() para cada frame
      ← rellena campos de MP3File
  → push_back to m_files vector
```

### Guardado de cambios
```
TuiApp::onApply()
  → MP3File::save()
    → ID3Tag::save(path)
      → std::ifstream + std::ofstream (read/write)
      → buildID3v2Tag()  // reconstruye tag v2 completo
      → write ID3v1 tag  // escribe/actualiza tag v1
    → std::filesystem::rename() si hay renombrado
  → updateStatusBar()
```

## Dependencias externas

- **ftxui v7.0+**: libreria de interfaz TUI para C++17
  - ftxui-component: componentes interactivos (botones, input, menus)
  - ftxui-dom: elementos de layout (text, border, hbox, vbox, table)
  - ftxui-screen: renderizado en pantalla
- **std::filesystem**: operaciones de archivos (C++17)
- **chafa** (opcional): renderizado de imagenes en terminal

## Eliminado (v1.x -> v2.0)

- **Qt5Core/Gui/Widgets**: reemplazado por ftxui + std::filesystem
- **MOC**: ya no necesario (sin Q_OBJECT macros)
- **QAbstractTableModel**: reemplazado por vector directo de MP3File*
- **MainWindow**: reemplazado por TuiApp
- **MP3TableModel**: eliminado, la tabla se renderiza directamente
