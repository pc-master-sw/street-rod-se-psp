# Compilar Street Rod SE para PSP

En Linux o WSL, instalá PSPSDK y agregá sus binarios al `PATH`.
Desde la raíz del repositorio, ejecutá `make psp`. El binario queda
en `.build/psp/EBOOT.PBP`. Las fuentes de `engine/` incluyen los
cambios de portabilidad aplicados a la revisión indicada en el
[README](../README.md).

El ZIP publicado incluye las mismas fuentes y una receta de compilación
en `source/BUILD.txt`. El EBOOT de v0.1.0 tiene SHA-256 `693990d9b0ecea911318a2526abf00c0f63b1699c32a47fdb8e2e98962a1eea6`.
