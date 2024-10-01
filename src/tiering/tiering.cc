#include "tiering.h"
#include "../global.h"


#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <regex>

#include "yaml-cpp/yaml.h"
#include "../structs.h"



std::string file_extension = ".sst";

std::string getFileExtension() {
  return file_extension;
}


IdTypeTable thread_map;
IntDeviceTable file_device_map;

// receive compaction id and new level, create new compaction details and insert
// into map
void registerStartCompaction(pthread_t compaction_id, int new_level) {
  IdTypeTable::accessor a;
  thread_map.insert(a, compaction_id);
  a->second = static_cast<op_type>(new_level);
}

void registerThread(pthread_t thread_id, op_type type) {
  IdTypeTable::accessor a;
  thread_map.insert(a, thread_id);
  a->second = type;
}

// get thread type or THREAD_OTHER if not found
op_type getThreadType(pthread_t thread_id) {
  IdTypeTable::accessor a;
  bool find = thread_map.find(a, thread_id);
  if (find) {
    return a->second;
  } else {
    return THREAD_OTHER;
  }
}


void linkFileToDevice(int sst_number, std::shared_ptr<Device> device) {
  
  IntDeviceTable::accessor a;
  file_device_map.insert(a, sst_number);
  a->second = device;

}

void removeFileDevice(int sst_number) {
  file_device_map.erase(sst_number);
}

std::shared_ptr<Device> getDevice(int sst_number) {
  IntDeviceTable::accessor a;
  bool find = file_device_map.find(a, sst_number);
  if (find) {
    return a->second;
  } else {
    return nullptr;
  }
}

void printFileDeviceMap() {
    for( IntDeviceTable::iterator i=file_device_map.begin(); i!=file_device_map.end(); ++i ) {
      std::cout << "file: " << i->first << " | device: " << i->second->name << std::endl;
    }
}

// write file device map to a file
void dumpFileDeviceMapToFile(std::string filename) {
  // std::cout << "Dumping file device map to file: " << filename << std::endl;
  std::ofstream myfile;
  myfile.open(filename,std::ofstream::trunc);
  for( IntDeviceTable::iterator i=file_device_map.begin(); i!=file_device_map.end(); ++i ) {
    myfile << "file: " << i->first << " | device: " << i->second->name << " | level: " << get_sst_level(i->first) << std::endl;
    std::cout << "file: " << i->first << " | device: " << i->second->name << " | level: " << get_sst_level(i->first) << std::endl;
  }
  myfile.close();
}




void loadFileDeviceMapFromFile(std::string filename) {

  //open file if exists
  std::ifstream myfile;
  myfile.open(filename);
  if (!myfile.is_open()) {
    std::cout << "File " << filename << " does not exist" << std::endl;
    return;
  } else {
      std::string line;
      while (std::getline(myfile, line)) {
        std::regex r("file: (\\d+) \\| device: (\\d+) \\| level: (\\d+)");
        std::smatch m;
        std::regex_search(line, m, r);
        if (m.size() > 0) {
          int file = std::stoi(m[1]);

          add_sst_level(file, std::stoi(m[3]));

          // device_type device = static_cast<device_type>(std::stoi(m[2]));

          std::shared_ptr<Device> device = name_to_device_map[std::stoi(m[2])];

          linkFileToDevice(file, device);
        }
      }
  }

  
}


bool isThreadACompaction(op_type thread) {
  return thread >=1 && thread <= 10;
} 

int getLevelFromOpType(op_type thread) {
  return thread;
}

bool isSST(std::string filename) {
  //check if file extension is .sst
  return filename.find(getFileExtension()) != std::string::npos;
}

int isSSTorWal(std::string filename) {
  //take the last 4 characters of the filename. If they are .sst return 1, if they are .wal 
  // return 2, else return 0
  std::string extension = filename.substr(filename.length() - 4);
  if (extension == getFileExtension()) {
    return 1;
  } else if (extension == ".log") {
    return 2;
  } else {
    return 0;
  }
}


std::string redirection_map_file_;
std::shared_ptr<Device> wal_device = nullptr;
std::vector<std::string> db_paths;
std::vector<std::vector<int>> placement_policies;
std::vector<std::string> device_engines;


// receive full path and return the file name without path and the path in the
// parameters
void GetFileName(const std::string& fname, std::string* path,
                  std::string* name) {
  size_t slash = fname.find_last_of("/");
  if (slash == std::string::npos) {
    *path = "";
    *name = fname;
  } else {
    *path = fname.substr(0, slash + 1);
    *name = fname.substr(slash + 1);
  }
}


// return extension of a file name
std::string getExtension(const std::string& fname) {
  size_t dot = fname.find_last_of(".");
  if (dot == std::string::npos) {
    return "";
  } else {
    return fname.substr(dot + 1);
  }
}

std::string actualPathByDevice(std::string fname, std::string name, std::shared_ptr<Device> dev) {

  if (dev) {
    return dev->path + "/" + name;
  } else {
    return fname;
  }
}



void deleteFileFromMap(std::string fname) {
  std::string path_, name_;
  GetFileName(fname, &path_, &name_);
  std::string extension = getExtension(name_);

  std::string actualPath;

  if (extension == file_extension.substr(1)) {
    //remove .sst from name_ and get number from name_
    int sst_number = std::stoi(name_.substr(0, name_.size() - 4));
    removeFileDevice(sst_number);
  }
}




// device: 0 -> SSD, 1 -> PMEM
std::string fileActualPath(std::string fname, std::shared_ptr<Device>& device, int& level) {
  std::string path_, name_;
  GetFileName(fname, &path_, &name_);
  std::string extension = getExtension(name_);

  std::string actualPath;


  if (extension == file_extension.substr(1)) {

    //remove .sst from name_ and get number from name_
    int sst_number = std::stoi(name_.substr(0, name_.size() - 4));

    std::shared_ptr<Device> device_ = getDevice(sst_number);

    level = -1;

    // if file is already redirected
    if (device_ != nullptr) {
      
      device = device_;
      level = get_sst_level(sst_number);

      return actualPathByDevice(fname, name_, device_);
    } else {
      // std::cout << "file not redirected yet, fname: " << sst_number << std::endl;
      op_type ttype = getThreadType(pthread_self());
      if (ttype == THREAD_FLUSH) {
        int new_level = 0;

        level = new_level;

        device = getDeviceFromLevel(level);
        // std::cout << "fname1: " << fname << " | level: " << level << " | device: " << device->name << std::endl;
        

        actualPath = actualPathByDevice(fname, name_, device);
        linkFileToDevice(sst_number, device);
        return actualPath;


      } else if (isThreadACompaction(ttype)) {
        int new_level = getLevelFromOpType(ttype);

        level = new_level;

        device = getDeviceFromLevel(level);
        // std::cout << "fname2: " << fname << " | level: " << level << " | device: " << device->name << std::endl;


        actualPath = actualPathByDevice(fname, name_, device);

        // std::cout << "Compaction - " << actualPath << std::endl;

        linkFileToDevice(sst_number, device);

        return actualPath;
      } else {  // not flush or compaction, but sst and not in map. Should
                // never happen..
        //print thread id
        // std::cout << "thread id: " << pthread_self() << std::endl;
        level = 0;


        device = getDeviceFromLevel(0);
        // std::cout << "fname3: " << fname << " | level: " << level << " | device: " << device->name << std::endl;

        actualPath = actualPathByDevice(fname, name_, device);
        // std::cout << "fname: " << fname << "| actualPath: " << actualPath << std::endl;
        linkFileToDevice(sst_number, device);
        return actualPath;
      }
    }
  } 
  else if (extension == "log") {
      if (wal_device) {
        device = wal_device;
        return device->path + "/" + name_;
      } else {
        device = getDeviceFromLevel(0);
        return device->path + "/" + name_;
      }

  } //any other type of file

  device = defaultDev;
  // device = getDeviceFromLevel(2);
  return fname;
}




struct Tier {
  bool wal;
  int name;
  std::string path;
  std::string engine;
  Policy policy;
  long cache_size;
  long disk_size;
};

void printPolicy(Policy p) {
  std::cout << "type: " << p.type << std::endl;
  std::cout << "levels: ";
  for (int i : p.levels) {
    std::cout << i << " ";
  }
  std::cout << std::endl;
  std::cout << "threshold: " << p.threshold << std::endl;
}

void printTier(Tier t) {
  std::cout << "------------------" << std::endl;
  std::cout << "name: " << t.name << std::endl;
  std::cout << "path: " << t.path << std::endl;
  std::cout << "wal: " << t.wal << std::endl;
  std::cout << "engine: " << t.engine << std::endl;
  printPolicy(t.policy);
  std::cout << "cache_size: " << t.cache_size << std::endl;
  std::cout << "disk_size: " << t.disk_size << std::endl;
}



void nodeToPolicy(YAML::Node node, Policy& p) {
  p.type = node["type"].as<std::string>();
  try {
    p.levels = node["levels"].as<std::vector<int>>();
  }
  catch (...) {  } 

  try {
    p.threshold = node["threshold"].as<long>();
  }
  catch (...) {  } 
}

void nodeToTier(YAML::Node node, int name, Tier& t) {
  t.name = name;

  t.path = node["path"].as<std::string>();
  t.engine = node["engine"].as<std::string>();

  // if (node["wal"])
  try {
    t.wal = node["wal"].as<bool>();
  }
  catch (...) {
    t.wal = false;
  } 
  
  t.cache_size = node["cache_size"].as<long>();
  t.disk_size = node["disk_size"].as<long>();

  nodeToPolicy(node["policy"], t.policy);
  std::cout << "finished" << std::endl;

}

void read_config_file() {
  std::cout << std::endl;


  std::string this_file = __FILE__;
  //get parent folder of this_file
  std::string parent_folder = this_file.substr(0, this_file.find_last_of("/\\"));
  std::string parent_parent_folder = parent_folder.substr(0, parent_folder.find_last_of("/\\"));
  std::string parent_parent_parent_folder = parent_parent_folder.substr(0, parent_parent_folder.find_last_of("/\\"));
  std::string config_file = parent_parent_parent_folder + "/yaml-config/config.yaml";



  std::cout << "config_file: " << config_file << std::endl;

  YAML::Node yaml_file = YAML::LoadFile(config_file);

  std::vector<Tier> tiers_temp;


  for(auto yaml : yaml_file) {


    std::cout << "yaml: " << yaml.first.as<std::string>() << std::endl;

    std::string key = yaml.first.as<std::string>();
    std::smatch match;
    if ( std::regex_match(key, match, std::regex("tier(\\d+)")) ) {
      std::cout << "match: " << match[1] << std::endl;
      Tier t;
      nodeToTier(yaml.second, stoi(std::string(match[1])), t);
      tiers_temp.push_back(t);
    } else {
      if (yaml.first.as<std::string>() == "configs") {
        redirection_map_file_ = yaml.second["redirection_map_file"].as<std::string>();
        file_extension = yaml.second["file_extension"].as<std::string>();
        profiler_log_file = yaml.second["profiler_log"].as<std::string>();
        activate_cache = yaml.second["activate_cache"].as<bool>();

        // activate_cache = false;
      }
    }

  }
  // sort tiers by name
  std::sort(tiers_temp.begin(), tiers_temp.end(), [](const Tier& lhs, const Tier& rhs) {
    return lhs.name < rhs.name;
  });

  for (Tier t : tiers_temp) {
    printTier(t);
  }

  // iterate over tiers with index i
  for (int j = 0; j < tiers_temp.size(); j++) {
    tiers.push_back(std::make_shared<Device>(tiers_temp[j].name, tiers_temp[j].path, tiers_temp[j].engine, tiers_temp[j].cache_size, tiers_temp[j].disk_size, tiers_temp[j].policy));
    if  (tiers_temp[j].wal) {
      wal_device = tiers[j];
    }
    if (j > 0) {
      tiers[j]->set_prev(tiers[j - 1]);
      tiers[j - 1]->set_next(tiers[j]);
    }
    name_to_device_map[tiers_temp[j].name] = tiers[j];
  }

    // caches.push_back(std::make_shared<DeviceCache>(tiers[0], tiers[1], 0));
    // tiers[1]->cache = std::make_shared<DeviceCache>(tiers[0], tiers[1]);

  // if (activate_cache) {
  //   for (int j = 1; j < tiers.size(); j++) {
  //     tiers[j]->cache = std::make_shared<DeviceCache>(tiers[j-1], tiers[j]);
  //     caches.push_back(tiers[j]->cache);
  //   }
  // }
  


  for (int j = 0; j < tiers.size(); j++) {
    for (int level : tiers_temp[j].policy.levels) {
      level_to_device_map[level] = tiers[j];
    }
  }

  for (int j = 0; j < tiers.size(); j++) {
    if (tiers[j]->policy.type == "capacity") {
      capacity_devices.push_back(tiers[j]);
      tiers[j]->lru = std::make_shared<LRU>(tiers[j]->disk_size, tiers[j]->policy.threshold);
    }
  }

// capacity_devices.push_back(tiers[1]);
// tiers[1]->lru = std::make_shared<LRU>();


  // capacity_devices[0]->total_size = 200000;
  // capacity_devices[1]->total_size = 12884901888;

  // capacity_devices[0]->lru = std::make_shared<LRU>();


  // capacity_devices[0]->lru->thisCapacity = capacity_devices[0];
  // capacity_devices[0]->lru->nextCapacity = capacity_devices[1];
  // capacity_devices[0]->lru_working = true;



  for (int i = 0; i < capacity_devices.size(); i++) {
    capacity_devices[i]->lru->thisCapacity = capacity_devices[i];
    if (i < capacity_devices.size() - 1) {
      capacity_devices[i]->lru->nextCapacity = capacity_devices[i + 1];
    }
    if (i < capacity_devices.size() - 1) {
      capacity_devices[i]->lru_working = true;
    }
  }
  

  std::cout << "level_to_device_map: " << std::endl;

  //print level_to_device_map (level -> device) (for the device simpl print the env)
  for (auto const& x : level_to_device_map) {
    std::cout << "last-level: " << x.first << " " << x.second->env << std::endl;
  }
  

  std::cout <<  std::endl;


  std::cout << "Loading file device map from file: " << redirection_map_file_ << std::endl;
  loadFileDeviceMapFromFile(redirection_map_file_);
  std::cout << "File device map: " << std::endl;
  printFileDeviceMap();
  std::cout << "-------------------------------------------------------" << std::endl;
}



void dumpRedirectionMap() {
  dumpFileDeviceMapToFile(redirection_map_file_);

}


void dump_second_config()  {
  // std::cout << "Dumping file device map to file: " << filename << std::endl;
  std::ofstream myfile;
  myfile.open("/home/gsd/ruben/kvs-nvme/kvstore/configs.txt",std::ofstream::trunc);

  //write level_to_device_map to file
  for (auto const& x : level_to_device_map) {
    myfile << "level: " << x.first << " | device: " << x.second->name << std::endl;
  }
  //write "---"
  myfile << "---" << std::endl;
  //write current_capacity_device_index
  myfile << "current_capacity_device_index: " << current_capacity_device_index << std::endl;


  //print the lrus
  for (int i = 0; i < capacity_devices.size(); i++) {
    myfile << "----" << std::endl;
    myfile << capacity_devices[i]->lru->to_string() << std::endl;
  }

  myfile.close();
}

void read_second_config() {
  std::string filename = "/home/gsd/ruben/kvs-nvme/kvstore/configs.txt";
  //until line is not "---" apply the regex "level: (\\d+) | device: (\\d+)" and add to level_to_device_map
  //after "---" apply the regex "current_capacity_device_index: (\\d+)" and set current_capacity_device_index
  std::ifstream myfile;
  myfile.open(filename);
  if (!myfile.is_open()) {
    std::cout << "File " << filename << " does not exist" << std::endl;
    return;
  } else {
      std::string line;
      while (std::getline(myfile, line)) {
        std::regex r("level: (\\d+) \\| device: (\\d+)");
        std::smatch m;
        std::regex_search(line, m, r);
        if (m.size() > 0) {
          level_to_device_map[std::stoi(m[1])] = name_to_device_map[std::stoi(m[2])];

          if (name_to_device_map[std::stoi(m[2])]->lru) {
            name_to_device_map[std::stoi(m[2])]->lru->last_level = std::stoi(m[1]);
          }
          std::cout << "from configs2 - level: " << m[1] << " | device: " << m[2] << std::endl;
          
        } else if (line == "---") {
          std::getline(myfile, line);
          std::regex r("current_capacity_device_index: (\\d+)");
          std::smatch m;
          std::regex_search(line, m, r);
          if (m.size() > 0) {
            current_capacity_device_index = std::stoi(m[1]);
          }
        }
      }
  }
  myfile.close();

  //print level_to_device_map
  for (auto const& x : level_to_device_map) {
    std::cout << "level: " << x.first << " | device: " << x.second->name << std::endl;
  }
  std::cout << "current_capacity_device_index: " << current_capacity_device_index << std::endl;


  //for each capacity device get the biggest level axxorfing to level_to_device_map and set it as it's lru's last level
  for (int i = 0; i < capacity_devices.size(); i++) {
    int max_level = -1;
    for (auto const& x : level_to_device_map) {
      if (x.second->name == capacity_devices[i]->name) {
        if (x.first > max_level) {
          max_level = x.first;
        }
      }
    }
    // capacity_devices[i]->lru->last_level = max_level;
  }

}


void read_second_config_lrus() {
  std::cout << "Reading second config lrus" << std::endl;
  std::string filename = "/home/gsd/ruben/kvs-nvme/kvstore/configs.txt";

  //read file and every time you find a line "----"
  //do: get the 10 next lines (each one represent a level) and apply the regex "(\\d+),"
  //, get all the matches and print them
  // dont use auxiliar functions
  std::ifstream myfile;
  myfile.open(filename);

  if (!myfile.is_open()) {
    std::cout << "File " << filename << " does not exist" << std::endl;
    return;
  } else {
    std::string line;
    int current_capacity_device_index = 0;
    while (std::getline(myfile, line)) {
      if (line == "----") {
        for (int i = 0; i < 10; i++) {
          std::getline(myfile, line);
          std::regex myregex = std::regex("(\\d+),");
          //get all matches
          std::sregex_iterator it(line.begin(), line.end(), myregex);
          //print all matches
          while (it != std::sregex_iterator()) {
            std::smatch match = *it;
            //print matches group only
            std::cout << match[1].str() << std::endl;            
            it++;

            //add the match to the lru
            capacity_devices[current_capacity_device_index]->lru->add(std::stoi(match[1].str()));
            
          }
        }
        // apply regex "current size: (\d+)" and print the match
        std::getline(myfile, line);
        std::regex myregex = std::regex("current size: (\\d+)");
        std::smatch match;
        std::regex_search(line, match, myregex);
        if (match.size() > 0) {
          std::cout << "current size: " << match[1] << std::endl;
        }
        current_capacity_device_index++;
      }
    }
  }

  //print all lrus
  for (int i = 0; i < capacity_devices.size(); i++) {
    std::cout << "lru: " << i << std::endl;
    std::cout << capacity_devices[i]->lru->to_string() << std::endl;
  }
  
}