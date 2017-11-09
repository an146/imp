/* imp -- heat-off daemon
 * Made by Anton Novikov, Russia in dark swamps of desperate love. 2017
 */

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

static int
system_desu(std::string command)
{
  std::cerr << "> " << command << std::endl;
  return std::system(command.c_str());
}

static std::string
read_desu(std::string filename)
{
  std::ifstream i(filename);
  std::string ret;
  i >> ret;
  return ret;
}

static void /*__noreturn*/
exec_desu()
{
  system_desu("systemctl poweroff");
}

struct hwmon : public std::vector<std::string>
{
  hwmon(const std::string &);
  hwmon(const hwmon &) = delete;
  hwmon(hwmon &&) = default;
  void focus() const;
  std::string path;
  std::string device_name;
};

struct hwmon_class : public std::vector<hwmon>
{
  hwmon_class();
  hwmon_class(const hwmon_class &) = delete;
};

hwmon::hwmon(const std::string &_path)
  : path(_path)
{
  focus();
  char device_buf[64];
  ssize_t device_size = readlink("device", device_buf, sizeof(device_buf));
  if (device_size > 0) {
    std::string device(device_buf, device_size);
    std::string::const_iterator last_component;
    switch (std::string::size_type slash = device.rfind('/')) {
    case std::string::npos:
      last_component = device.begin();
      break;
    default:
      last_component = device.begin() + slash + 1;
    }
    device_name = std::string(last_component, device.cend());
  }

  DIR *dir = opendir(".");
  for (dirent *entry = readdir(dir); entry; entry = readdir(dir)) {
    if (strchr(entry->d_name, '_') == NULL)
      continue;
    emplace_back(entry->d_name);
  }
}

void hwmon::focus() const
{
  chdir(path.c_str());
}

hwmon_class::hwmon_class()
{
  const std::string basepath = "/sys/class/hwmon/";
  DIR *dir = opendir(basepath.c_str());
  for (dirent *entry = readdir(dir); entry; entry = readdir(dir)) {
    if (*entry->d_name == '.')
      continue;
    emplace_back(basepath + entry->d_name);
  }
}

int
main(int argc, char **argv)
{
  hwmon_class sch;
  const std::string config = "/etc/imprc";
  if (argc < 2) {
    for (auto &i : sch) {
      i.focus();
      system_desu("pwd && ls -l");
      //std::cout << ".device_name: " << i.device_name << std::endl;
      std::cout << "name: " << read_desu("name") << std::endl;
      for (auto &j : i)
        std::cout << j << "\t" << read_desu(j) << std::endl;
    }
  } else if (argc == 2 && std::string("init") == argv[1]) {
    if (system_desu("[ -f " + config + " ]") == 0)
      throw std::logic_error("config exists");
    std::ofstream o(config);
    for (auto &i : sch) {
      i.focus();
      o << ":" << read_desu("name") << std::endl;
      for (auto &j : i) {
        std::string word = read_desu(j);
        bool nondigit = false;
        for (char c : word)
          if (!std::isdigit(c) && c != '-')
            nondigit = true;
        if (nondigit)
          o << '#';
        o << j << "\t" << word << std::endl;
      }
    }
  } else if (argc == 2 && std::string("check") == argv[1]) {
    std::map<std::string, std::map<std::string, std::function<bool (int)>>> triggers;
    // read config
    std::ifstream i(config);
    std::string name;
    std::string token, value, _pred;
    std::function<bool (int, int)> pred;
    while (i >> token) {
      if (token[0] == ':')
        name = std::string(token.begin() + 1, token.end());
      else if (token[0] == '#')
        i >> _pred;
      else if (name.empty())
        throw std::out_of_range("instance name not specified");
      else {
        value = token;
        i >> _pred;
        switch (_pred[0]) {
        case '<':
          pred = std::less<int>();
          break;
        case '>':
          pred = std::greater<int>();
          break;
        case '=':
          pred = std::equal_to<int>();
          break;
        }
        triggers[name].insert(std::make_pair(
          value,
          std::bind(pred, std::placeholders::_1, std::atoi(_pred.c_str() + 1))
        ));
      }
    }
    for (auto &i : sch) {
      i.focus();
      name = read_desu("name");
      if (!triggers.count(name))
        continue;
      for (auto &j : i) {
        if (!triggers[name].count(j))
          continue;
        if (triggers[name][j](std::atoi(read_desu(j).c_str())))
          exec_desu();
      }
    }
  }
}
