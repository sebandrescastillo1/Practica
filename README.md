# Proyecto Práctica 2022-2023
- Sebastian Castillo		201821051-K



## Archivos
- README.md: Archivo con explicación de repositorio.

- ClienteWebcam.c: Cliente con webcam, que produce y emite video a servidor redireccionador.

- ServerRelay.c: Servidor relay que selecciona y redirecciona video a Cliente playout.

- ClientePlayout.c: Cliente en red del playout, que recibe video de Servidor Redireccionador y reenvía para reproducir en TV.

- webcamStream.sh: Script bash con comando ffmpeg para la transmision.

- makefile: Facilita al usuario tareas tales como compilar programas, dar permisos a .sh o eliminar archivos residuales.

## Preparacion

Utilizando Makefile para compilar:

- ServerRelay.c -> $make ServerRelay

- ClienteWebcam.c -> $make ClienteWebcam

- ClientePlayout.c -> $make ClientePlayout


Utilizando Makefile para dar permisos:

- webcamStream.sh -> $make webcamStream


Utilizando Makefile para hacer todas las tareas necesarias:
- Prepara todo -> $make all


Utilizando Makefile para limpiar .o
- Comando -> $make clean

## Ejecucion


ClientePlayout.c: 
- ./ClientePlayout.o < domain >
```sh
Ej:$ ./ClientePlayout.o aragorn.elo.utfsm.cl
```

ClienteWebcam.c: 
- ./ClienteWebcam.o < domain > < port > < device >
```sh
Ej:$ ./ClienteWebcam.o aragorn.elo.utfsm.cl 47203 0
```
Server relay siempre recibe en puerto 47203 a las webcams.

ServerRelay.c: 
- ./ServerRelay.o
```sh
Ej:$ ./ClienteWebcam.o 
```










