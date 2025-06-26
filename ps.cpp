#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <string_view>
#include <iostream>
#include <sstream>

// helper to read entire file into string
static bool readFile(const std::string &path, std::string &out) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    char buf[4096];
    ssize_t r;
    out.clear();
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        out.append(buf, r);
    }
    close(fd);
    return r >= 0;
}

// helper to read symlink
static bool readLink(const std::string &path, std::string &out) {
    char buf[4096];
    ssize_t len = readlink(path.c_str(), buf, sizeof(buf)-1);
    if (len < 0) return false;
    buf[len] = '\0';
    out.assign(buf);
    return true;
}

static std::string jsonEscape(const std::string &s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

int main() {
    DIR *proc = opendir("/proc");
    if (!proc) return 1;
    std::vector<std::string> entries;
    struct dirent *ent;
    while ((ent = readdir(proc))) {
        if (ent->d_type != DT_DIR && ent->d_type != DT_LNK && ent->d_type != DT_UNKNOWN)
            continue;
        const char *name = ent->d_name;
        if (!*name) continue;
        char *endp;
        long pid = strtol(name, &endp, 10);
        if (*endp != '\0') continue; // not numeric
        entries.emplace_back(name);
    }
    closedir(proc);

    std::cout << "[";
    bool first = true;
    for (const std::string &pidStr : entries) {
        std::string base = std::string("/proc/") + pidStr + "/";
        std::string exe, cwd, maps, stat, cmdline;
        if (!readLink(base + "exe", exe)) continue;
        if (!readLink(base + "cwd", cwd)) continue;
        if (!readFile(base + "maps", maps)) continue;
        if (!readFile(base + "stat", stat)) continue;
        if (!readFile(base + "cmdline", cmdline)) continue;

        std::istringstream mapsStream(maps);
        std::string firstLine;
        if (!std::getline(mapsStream, firstLine)) continue;
        std::string addrStr = firstLine.substr(0, firstLine.find('-'));
        unsigned long baseAddr = strtoul(addrStr.c_str(), nullptr, 16);

        std::istringstream statStream(stat);
        long pid; std::string comm; char state;
        statStream >> pid >> comm >> state;
        if (!statStream) continue;

        std::vector<std::string> args;
        std::string current;
        for (char c : cmdline) {
            if (c == '\0') {
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) args.push_back(current);

        if (!first) std::cout << ",";
        first = false;
        std::cout << "{\"pid\": " << pid
                  << ", \"exe\": \"" << jsonEscape(exe) << "\""
                  << ", \"cwd\": \"" << jsonEscape(cwd) << "\""
                  << ", \"base_address\": " << baseAddr
                  << ", \"state\": \"" << state << "\""
                  << ", \"cmdline\": [";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << "\"" << jsonEscape(args[i]) << "\"";
        }
        std::cout << "]}";
    }
    std::cout << "]";
    return 0;
}

