#ifndef STRUCTS_H
#define STRUCTS_H
#endif

typedef enum { OFF, ON, AUTO  } Mode;

struct Config {
    int reference;
    Mode mode;
};