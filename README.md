# DK3D — Proyecto Final Laboratorio de Gráficas por Computadora
> Recreación 3D del clásico **Donkey Kong** (1981) utilizando OpenGL 3.3 Core Profile, desarrollada como proyecto final para la materia de Laboratorio de Gráficas por Computadora — UNAM.

---

## Descripción

Juego 3D inspirado en el arcade original de Donkey Kong. El jugador controla a Mario y debe subir por las vigas y escaleras esquivando los barriles que lanza Donkey Kong para rescatar a Pauline.

**Características implementadas:**
- Render con OpenGL 3.3 Core Profile
- Sistema de iluminación con spotlight (Mario) y point lights por piso (tecla `I`)
- Textura procedural de celosía metálica en las vigas (generada en código, sin imágenes externas)
- Animación por keyframes del objeto bonus (.obj cargado desde archivo)
- 3 modos de cámara: ortográfica, seguimiento y primera persona (tecla `C`)
- HUD con TIME y SCORE renderizados con fuente pixel-art propia
- Lógica completa de juego: colisiones, barriles, escaleras, victoria y muerte

---

## Requisitos

- **Sistema operativo:** Windows 10/11 (64 bits)
- **Compilador:** GCC vía [MSYS2](https://www.msys2.org/) con entorno **UCRT64**
- **Dependencias:**
  - `glfw3`
  - `glew`
  - `cglm`
  - `opengl32` (incluida en Windows)

---

## Instalación de dependencias

1. Descarga e instala **MSYS2** desde https://www.msys2.org/

2. Abre la terminal **MSYS2 UCRT64** e instala las dependencias:

```bash
pacman -S mingw-w64-ucrt-x86_64-glfw
pacman -S mingw-w64-ucrt-x86_64-glew
pacman -S mingw-w64-ucrt-x86_64-cglm
```

---

## Compilación

Abre la terminal **MSYS2 UCRT64**, navega a la carpeta del proyecto y ejecuta:

```bash
gcc main.c -o DK3D.exe -lglfw3 -lglew32 -lopengl32 -lgdi32 -lcglm -lm
```

---

## Ejecución

```bash
./DK3D.exe
```

> **Nota:** el archivo `modelo.obj` debe estar en la misma carpeta que `DK3D.exe`. Este archivo contiene el modelo 3D del objeto bonus (estrella/moneda) que aparece en los pisos.

---

## Controles

| Tecla | Acción |
|---|---|
| `←` `→` | Mover a Mario izquierda / derecha |
| `↑` | Subir escalera |
| `↓` | Bajar escalera |
| `Espacio` | Saltar |
| `C` | Cambiar cámara (ortográfica → seguimiento → primera persona) |
| `I` | Activar / desactivar point lights por piso |
| `L` | Cambiar skin de Mario (naranja → verde) |
| `W` | Teletransportar a Mario arriba (debug) |
| `Enter` | Pausar / reanudar |
| `R` | Reiniciar partida |
| `Q` | Salir |

---

## Estructura del proyecto

```
Proyecto-Final-Lab-Grafica/
├── main.c          # Código fuente principal
├── modelo.obj      # Modelo 3D del objeto bonus
└── README.md       # Este archivo
```

---

## Créditos

Desarrollado por **Jp-NavarroN**  
Materia: Laboratorio de Gráficas por Computadora  
Facultad de Ingeniería — UNAM
