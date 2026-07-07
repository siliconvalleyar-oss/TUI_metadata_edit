# MetadataMP3 TUI

Editor de metadatos ID3 para archivos MP3 con interfaz de terminal (TUI), construido con C++17 y ftxui.

Permite visualizar, editar y guardar metadatos (artista, album, titulo, anio, genero, pista, comentario, compositor) y caratula de album de multiples archivos MP3 de forma interactiva desde la terminal.

## Caracteristicas

- Interfaz TUI interactiva con paneles divididos (tabla + detalle)
- Carga individual de archivos o carpetas completas (recursivo)
- Vista de tabla con columnas: nombre, artista, album, titulo, anio, pista
- Panel de detalle con campos editables para todos los metadatos
- Visualizacion de caratula de album via chafa (ASCII art en terminal)
- Eliminacion de caratula de album
- Renombrado de archivos desde el editor
- Navegacion con teclado: flechas, Enter, Tab, Page Up/Down
- Soporte para ID3v1, ID3v1.1 e ID3v2.3
- Codificacion de texto: ISO-8859-1, UTF-16, UTF-8
- Guardado de imagenes APIC (JPEG/PNG)
- Carga de archivos via argumentos de linea de comandos

## Requisitos

- Linux (tested on Ubuntu/Debian)
- g++ con soporte C++17
- ftxui v7.0+ (libreria de interfaz TUI para C++)
- chafa (opcional, para visualizar caratula de album)

```bash
# Debian/Ubuntu - instalar dependencias del sistema
sudo apt install g++ cmake git

# Instalar ftxui desde fuente
cd /tmp
git clone --depth 1 https://github.com/ArthurSonzogni/FTXUI.git
cd FTXUI && mkdir build && cd build
cmake .. -DFTXUI_BUILD_EXAMPLES=OFF -DFTXUI_BUILD_TESTS=OFF
make -j$(nproc)
sudo make install

# Instalar chafa (opcional, para caratula de album)
sudo apt install chafa
```

## Compilacion

```bash
make          # Compilar
make run      # Compilar y ejecutar
make clean    # Limpiar objetos
make distclean # Limpiar todo (obj + bin)
```

### Estructura de salida

```
bin/MetadataMP3    # Binario compilado
obj/               # Archivos objeto y dependencias
```

## Estructura del proyecto

```
.
├── bin/                # Binarios compilados
├── docs/               # Documentacion
│   ├── ARCHITECTURE.md
│   ├── API.md
│   ├── BUILD.md
│   └── CHANGELOG.md
├── include/            # Headers (.h)
│   ├── id3tag.h
│   ├── mp3file.h
│   └── tui_app.h
├── obj/                # Objetos compilados
├── src/                # Fuentes (.cpp)
│   ├── id3tag.cpp
│   ├── main.cpp
│   ├── mp3file.cpp
│   └── tui_app.cpp
├── Makefile
├── README.md
└── VERSION
```

## Uso

```bash
# Ejecutar vacio
./bin/MetadataMP3

# Cargar archivos directamente
./bin/MetadataMP3 archivo1.mp3 archivo2.mp3

# Cargar una carpeta completa
./bin/MetadataMP3 /ruta/a/carpeta/mp3/
```

### Atajos de teclado

| Tecla | Accion |
|-------|--------|
| Flechas Arriba/Abajo | Navegar en la tabla de archivos |
| Enter | Seleccionar archivo / entrar en modo edicion |
| Tab / Shift+Tab | Cambiar entre campos de edicion |
| Escape | Salir del modo edicion / cancelar |
| `a` | Agregar archivos (solicita rutas) |
| `f` | Agregar carpeta (solicita ruta) |
| `s` | Seleccionar/deseleccionar todos |
| `x` | Limpiar lista de archivos |
| `d` | Descartar cambios |
| `r` | Eliminar caratula de album |
| `Ctrl+S` | Aplicar/Guardar cambios |
| `q` / `Ctrl+C` | Salir |

## Licencia

Ver archivo LICENSE (si existe).
