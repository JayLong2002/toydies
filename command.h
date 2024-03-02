#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <string>

const size_t k_max_args = 1024;

enum CMD{
    GET,
    SET,
    DEL,
    SEARCH,
    UNKNOWN
};

static int32_t parse_req(
    const uint8_t *data, size_t len, std::vector<std::string> &out)
{
    if (len < 4) {
        return -1;
    }
    uint32_t n = 0;
    memcpy(&n, &data[0], 4);
    if (n > k_max_args) {
        return -1;
    }

    size_t pos = 4;
    while (n--) {
        if (pos + 4 > len) {
            return -1;
        }
        uint32_t sz = 0;
        memcpy(&sz, &data[pos], 4);
        if (pos + 4 + sz > len) {
            return -1;
        }
        out.push_back(std::string((char *)&data[pos + 4], sz));
        pos += 4 + sz;
    }

    if (pos != len) {
        return -1; 
    }
    return 0;
}

static bool cmd_is(const std::string &word, const char *cmd) {
    return 0 == strcasecmp(word.c_str(), cmd);
}

static CMD parser_cmd(const std::vector<std::string> &cmd)
{
    if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
        return GET;
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        return SET;
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        return DEL;
    } else if(cmd.size() == 3 && cmd_is(cmd[0],"search")){
        return SEARCH;
    } else{
        return UNKNOWN;
    }
}










