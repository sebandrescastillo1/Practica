
# compiler options -- C99 with warnings
OPT_GCC = -Wall
# compiler options and libraries for Linux, Mac OS X or Solaris
#ifeq ($(OSTYPE),linux-gnu)  # default linux-gnu // Agustin
  LIB = -lpthread -lrt 
#endif
ifeq ($(OSTYPE),darwin)	# Mac OS X
  LIB = 
endif
ifeq ($(OSTYPE),solaris)
  LIB = -lpthread -lrt 
endif

#classes

all: ClientePlayout ServerRelay ClienteWebcam webcamStream

ClientePlayout: ClientePlayout.c
	gcc $(OPT_GCC) ClientePlayout.c -o ClientePlayout.o $(LIB)

ServerRelay: ServerRelay.c
	gcc $(OPT_GCC) ServerRelay.c -o ServerRelay.o $(LIB)

ClienteWebcam: ClienteWebcam.c
	gcc $(OPT_GCC) ClienteWebcam.c -o ClienteWebcam.o $(LIB)


webcamStream: webcamStream.sh
	chmod 755 webcamStream.sh

clean:
	rm -f *.o 
