# Guia de Compilacion

## Requisitos del sistema

- **Sistema operativo**: Linux (Ubuntu/Debian, Fedora, Arch)
- **Compilador**: g++ con soporte C++17
- **ftxui**: libreria de interfaz TUI (instalada desde fuente)
- **chafa** (opcional): para visualizar caratula de album en terminal

## Instalar dependencias

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install g++ make cmake git

# ftxui desde fuente
cd /tmp
git clone --depth 1 https://github.com/ArthurSonzogni/FTXUI.git
cd FTXUI && mkdir build && cd build
cmake .. -DFTXUI_BUILD_EXAMPLES=OFF -DFTXUI_BUILD_TESTS=OFF
make -j$(nproc)
sudo make install

# chafa (opcional)
sudo apt install chafa
```

### Fedora

```bash
sudo dnf install gcc-c++ make cmake git

# ftxui desde fuente (mismo proceso que arriba)
```

### Arch Linux

```bash
sudo pacman -S base-devel cmake git

# ftxui desde fuente (mismo proceso que arriba)
```

## Compilar

```bash
make            # Compilar el binario
make run        # Compilar y ejecutar
make clean      # Eliminar archivos objeto
make distclean  # Eliminar obj/ y bin/
```

## Estructura de archivos generada

```
obj/                    # Archivos objeto y dependencias
├── id3tag.o
├── main.o
├── mp3file.o
├── tui_app.o
├── *.d                 # Dependencias automaticas
bin/
└── MetadataMP3         # Binario ejecutable
```

## Makefile - Variables

| Variable | Valor default | Descripcion |
|----------|---------------|-------------|
| `CXX` | `g++` | Compilador |
| `CXXFLAGS` | `-std=c++17 -fPIC -Wall -Wextra -O2 -Iinclude` | Flags de compilacion |
| `LDFLAGS` | `-lftxui-component -lftxui-dom -lftxui-screen` | Librerias enlazadas |

## Solucion de problemas

### "ftxui/component/component.hpp: No such file"

ftxui no esta instalado o no se encuentra en el path de includes. Verificar:
```bash
ls /usr/local/include/ftxui/component/component.hpp
# Si no existe, reinstalar ftxui desde fuente
```

### "cannot find -lftxui-component"

Las librerias de ftxui no estan en el path de enlace. Verificar:
```bash
ls /usr/local/lib/libftxui-*.a
# Si no existen, reinstalar ftxui desde fuente
```

### Errores de headers movidos

Los headers estan en `include/`, no en `src/`. El Makefile usa `-Iinclude` para resolverlos. Si se agregan nuevos headers, colocarlos en `include/`.
