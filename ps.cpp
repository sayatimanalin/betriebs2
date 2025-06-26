#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <dirent.h>
#include <cstdlib>
#include <sys/stat.h>
#include <sstream>
#include <string_view>


using namespace std;

string clean(const string &s) {
    string test;
    for (unsigned char c : s) {
        switch (c) {
            case '\\': test+= "\\\\"; break;
            case '\"': test+= "\\\""; break;
            case '\n': test+= "\\n";  break;
            case '\r': test+= "\\r";  break;
            case '\t': test+= "\\t";  break;
            default:
                if (c < 0x20) {
                    char tmp[7];
                    snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                    test+= tmp;
                } else test+= c;
        }
    }
    return test;
}

int main() {
    DIR *proc = opendir("/proc");
    if (!proc) return 1;

    vector<string> pids;
    dirent *d;
    while ((d = readdir(proc)) != nullptr) {

        if (d->d_type != DT_DIR && d->d_type != DT_LNK && d->d_type != DT_UNKNOWN)
            continue;
        char *endp;
        strtol(d->d_name, &endp, 10);
        if (*endp == '\0') pids.emplace_back(d->d_name);
    }
    closedir(proc);

    cout << "[";
    bool firstOut = true;

    for (const string &pidStr : pids) {
        string base = "/proc/" + pidStr + "/";

        char lbuf[4096];
        ssize_t llen = readlink((base + "exe").c_str(), lbuf, sizeof(lbuf) - 1);
        if (llen < 0) continue;
        lbuf[llen] = '\0';
        string exe(lbuf);

        llen = readlink((base + "cwd").c_str(), lbuf, sizeof(lbuf) - 1);
        if (llen < 0) continue;
        lbuf[llen] = '\0';
        string cwd(lbuf);

        string maps;
        {
            int fd = open((base + "maps").c_str(), O_RDONLY);
            if (fd < 0) continue;
            char buf[4096];
            ssize_t r;
            while ((r = read(fd, buf, sizeof(buf))) > 0) maps.append(buf, r);
            close(fd);
            if (r < 0) continue;
        }

        string stat;
        {
            int fd = open((base + "stat").c_str(), O_RDONLY);
            if (fd < 0) continue;
            char buf[4096];
            ssize_t r;
            while ((r = read(fd, buf, sizeof(buf))) > 0) stat.append(buf, r);
            close(fd);
            if (r < 0) continue;
        }

        string cmdln;
        {
            int fd = open((base + "cmdline").c_str(), O_RDONLY);
            if (fd < 0) continue;
            char buf[4096];
            ssize_t r;
            while ((r = read(fd, buf, sizeof(buf))) > 0) cmdln.append(buf, r);
            close(fd);
            if (r < 0) continue;
        }

        istringstream mapsIn(maps);
        string firstLine;
        getline(mapsIn, firstLine);
        size_t dash = firstLine.find('-');
        unsigned long baseAddr = dash == string::npos ? 0
                : strtoul(firstLine.substr(0, dash).c_str(), nullptr, 16);

        istringstream statIn(stat);
        long pid; string comm; char state;
        statIn >> pid >> comm >> state;
        if (!statIn) continue;

        vector<string> args;
        string cur;
        for (char c : cmdln) {
            if (c == '\0') { if (!cur.empty()) { args.push_back(cur); cur.clear(); } }
            else cur += c;
        }
        if (!cur.empty()) args.push_back(cur);

        if (!firstOut) cout << ",";
        firstOut = false;


        cout << "{"
             << "\"pid\":" << pid << ","
             << "\"exe\":\"" << clean(exe) << "\","
             << "\"cwd\":\"" << clean(cwd) << "\","
             << "\"base_address\":" << baseAddr << ","
             << "\"state\":\"" << state << "\","
             << "\"cmdline\":[";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) cout << ",";
            cout << "\"" << clean(args[i]) << "\"";
        }
        cout << "]}";
    }

    cout << "]";
    return 0;
}
