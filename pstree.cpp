#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

struct ProcInfo {
    int pid;
    int ppid;
    std::string name;
    std::vector<int> children;
};

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

static void printNode(int pid, const std::unordered_map<int, ProcInfo> &procs) {
    auto it = procs.find(pid);
    if (it == procs.end()) return;
    const ProcInfo &p = it->second;
    std::cout << "{\"pid\": " << p.pid
              << ", \"name\": \"" << jsonEscape(p.name) << "\""
              << ", \"children\": [";
    bool first = true;
    for (int child : p.children) {
        if (!first) std::cout << ",";
        first = false;
        printNode(child, procs);
    }
    std::cout << "]}";
}

int main() {
    DIR *proc = opendir("/proc");
    if (!proc) return 1;
    std::unordered_map<int, ProcInfo> procs;
    std::vector<int> order;
    struct dirent *ent;
    while ((ent = readdir(proc))) {
        if (ent->d_type != DT_DIR && ent->d_type != DT_LNK && ent->d_type != DT_UNKNOWN)
            continue;
        char *endp;
        long pid = strtol(ent->d_name, &endp, 10);
        if (*endp != '\0') continue;
        std::string statPath = std::string("/proc/") + ent->d_name + "/stat";
        std::string stat;
        if (!readFile(statPath, stat)) continue;
        int spid, ppid; char name[256], state;
        if (sscanf(stat.c_str(), "%d (%255[^)]) %c %d", &spid, name, &state, &ppid) != 4) continue;
        ProcInfo info{spid, ppid, name, {}};
        procs[spid] = info;
        order.push_back(spid);
    }
    closedir(proc);
    for (auto &kv : procs) {
        int ppid = kv.second.ppid;
        auto pit = procs.find(ppid);
        if (pit != procs.end()) {
            pit->second.children.push_back(kv.first);
        }
    }
    std::vector<int> roots;
    for (int pid : order) {
        auto it = procs.find(pid);
        if (it == procs.end()) continue;
        if (!procs.count(it->second.ppid)) roots.push_back(pid);
    }
    std::cout << "[";
    bool first = true;
    for (int pid : roots) {
        if (!first) std::cout << ",";
        first = false;
        printNode(pid, procs);
    }
    std::cout << "]";
    return 0;
}

