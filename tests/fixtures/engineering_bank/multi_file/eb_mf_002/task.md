util.c defines helper but util.h does not declare it. Add a prototype to util.h so consumers can use it. Parameter names may match the definition (int x) or be omitted (int).
