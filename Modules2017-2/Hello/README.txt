Hello: modulo trivial con rutinas init_module y cleanup_module

Lo siguiente se debe realizar parados en
el directorio en donde se encuentra este README.txt

+ Compilacion (puede ser en modo usuario):
% make
...
% ls
... mimodulo.ko ...

+ Instalacion (en modo root)

# insmod mimodulo.ko
# dmesg
...
[ ... ] Hello world
#

+ Desinstalar el modulo

# rmmod mimodulo.ko
# dmesg
...
[ ... ] Goodbye world
#
