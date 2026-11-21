# Street Rod SE para PSP

Port nativo de Street Rod SE para Sony PSP. Usa un motor C compilado para Allegrex
(MIPS), video VGA de 320 × 200, controles de PSP y audio de altavoz de PC.
Carga las tablas y recursos de una copia DOS del juego.

1. Descargá [Street-Rod-SE-PSP.zip](https://github.com/pc-master-sw/street-rod-se-psp/releases/tag/v0.1.0).
2. Seguí el [manual de instalación](docs/instalacion.md).
3. Consultá el [manual de uso](docs/uso.md) para jugar, guardar y continuar.

## Desarrollo

La [guía de compilación](docs/compilacion.md) explica cómo generar el EBOOT
mediante PSPSDK. La [validación](docs/validacion.md) detalla las pruebas
de esta primera versión.

## Créditos

- Motor C de Krzysztof Kania (kylofon). [Street Rod SDL3, revisión de origen](https://github.com/kylofon/street-rod-sdl3/tree/0e5598e4fb45735b7c611a41b3595c91f16c3fa8) y [licencia MIT](engine/LICENSE).
- Adaptador y runtime de DOS2PSP Native bajo [GPL-2.0 o posterior](LICENSE).
- Juego original de California Dreams / Logical Design Works. Cada jugador aporta sus propios archivos DOS.
