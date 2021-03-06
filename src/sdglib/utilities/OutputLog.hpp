//
// Created by Luis Yanes (EI) on 08/02/2018.
//

#ifndef BSG_OUTPUTLOG_H
#define BSG_OUTPUTLOG_H

#include <iostream>
#include <ctime>
namespace sdglib {
    enum LogLevels{INFO, WARN, DEBUG};
    extern LogLevels OutputLogLevel;
    std::ostream &OutputLog(LogLevels level = LogLevels::INFO, bool include_date = true);
    std::ostream &OutputLog(bool include_date);
};
#endif //BSG_OUTPUTLOG_H
