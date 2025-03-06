#include "logging.hpp"

#include <iostream>

namespace Log {
    static Level _level = Level::Error;

    void init(Level lvl)
    {
        _level = lvl;
    };

    void write(Level lvl, std::ostringstream &msg)
    {
        if (lvl >= _level) {
            std::cout << msg.str() << std::endl;
            fflush(stdout);
        }
    }

};