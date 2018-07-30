#ifndef HZ_SERVER_ERROR_H
#define HZ_SERVER_ERROR_H

#include <iostream>

#define hz_error(erro_type, erros,file) \
    do {\
           std::cerr<<__FILE__<<": "<<__LINE__<<" "<<erro_type<<": "<<erros<<std::endl;\
    } while(0)


#endif //HZ_SERVER_ERROR_H
