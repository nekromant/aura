#ifndef MY_DEBUG_H
#define MY_DEBUG_H

#define START_CHAR 'S'
//const uint8_t global_lvl = 4;
#define DEBUG
#ifdef DEBUG
    #define DBG(lvl) if (lvl < global_lvl)
#else
    #define DBG(lvl) if (0)
#endif

#endif
