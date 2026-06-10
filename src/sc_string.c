#include "constants.h"
#include "sc_string.h"

#include <string.h>

int safe_strcpy(char* const dst, char const* const src, size_t count) {
    if ((count == 0) || (count <= strlen(src))) {
        return RET_ERROR;
    }
    strcpy(dst, src);
    return RET_OKAY;
}
