# Changelog

## [2.0.0] - 2026-07-06

### Changed

- **Migracion completa de Qt5 a ftxui**: interfaz TUI interactiva en terminal
- Eliminada dependencia de Qt5Core, Qt5Gui, Qt5Widgets y MOC
- Reemplazado QAbstractTableModel por vector directo de MP3File*
- Reemplazado QFile/QFileInfo por std::ifstream/std::filesystem
- Reemplazado QString por std::string
- Reemplazado QByteArray por std::vector<uint8_t>
- Reemplazado QTextCodec por decodificador UTF-16 manual
- Reemplazado QImage por validacion de magic bytes para APIC

### Added

- Interfaz TUI con paneles divididos (tabla + detalle)
- Navegacion completa con teclado (flechas, Tab, Enter, Page Up/Down)
- Atajos de teclado: [a] Add Files, [f] Add Folder, [d] Discard, [Ctrl+S] Save, [q] Quit
- Modal de confirmacion para guardar y descartar cambios
- Album art via chafa (ASCII art en terminal)
- Carga de archivos via argumentos de linea de comandos
- Status bar con informe de archivos, seleccionados y modificados

### Removed

- Qt5 GUI (MainWindow, MP3TableModel)
- QFileDialog (reemplazado por input de texto en modal)
- QMessageBox (reemplazado por modales TUI)
- QProgressDialog (progreso via status bar)
- MOC (Qt Meta Object Compiler)
- Edicion in-place en celdas de la tabla

## [1.0.0] - 2026-07-02

### Added

- Editor grafico de metadatos ID3 para archivos MP3
- Carga de archivos individuales y carpetas (recursivo)
- Vista de tabla con seleccion multiple y ordenamiento
- Panel de detalle con edicion de todos los campos ID3
- Soporte ID3v1, ID3v1.1 e ID3v2.3
- Decodificacion de texto ISO-8859-1, UTF-16, UTF-8
- Visualizacion de caratula de album (APIC frame)
- Eliminacion de caratula de album
- Renombrado de archivos desde el editor
- Edicion in-place en celdas de la tabla
- Checkbox de seleccion por archivo con "Select All"
- Indicador visual de archivos modificados (texto azul)
- Barra de estado con conteo de archivos, seleccionados y modificados
- Tooltips con ruta completa y tamano en bytes
- Makefile con estructura organizada (src/, include/, obj/, bin/)
