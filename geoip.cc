
#include <iostream>
#include <vector>
#include <string>

#include "maxmind.hpp"
#include "CLI/CLI.hpp"

#define VERSION "v0.2.1"
#define AUTHOR  "xbol"

void lookup(std::string ip, std::string file, std::vector<std::vector<std::string>> fields, bool dump = false) {
  maxmind db{std::filesystem::path {file}};

  if (!db) {
    std::cerr << "ERROR: " << db.status_msg() << std::endl;
    return;
  }

  LookupResult result{db.lookup(ip)};

  if (!result) {
    std::cout << "ip " << ip << " not found" << std::endl;
    return;
  }

  if (dump) {
    db.print();
    return;
  }

  std::cout << ip << "/" << result.netmask;
  for (auto& frags : fields) {
    Entry entry{result.value(frags)};
    if (!entry) continue;
    if (entry.str()) {
      std::cout << " " << entry.str().value();
    } else if (entry.num()) {
      std::cout << " " << entry.num().value();
    } else if (entry.numf()) {
      std::cout << " " << entry.numf().value();
    }
  }
  std::cout << std::endl;


}

void lookupName(std::string host, std::string file, std::vector<std::vector<std::string>> fields, bool dump = false) {
  maxmind db{std::filesystem::path {file}};

  if (!db) {
    std::cerr << "ERROR: " << db.status_msg() << std::endl;
    return;
  }

  std::vector<LookupResult> results{db.lookupName(host)};

  if (results.size() == 0) {
    std::cout << "host " << host << " not found" << std::endl;
    return;
  }

  for (auto& result : results) {

    if (!result) {
      std::cout << "host " << host << " not found" << std::endl;
      continue;
    }

    if (dump) {
      db.print();
      continue;
    }

    std::cout << host << " " << result.canonical_ip << "/" << result.netmask;
    for (auto& frags : fields) {
      Entry entry{result.value(frags)};
      if (!entry) continue;
      if (entry.str()) {
        std::cout << " " << entry.str().value();
      } else if (entry.num()) {
        std::cout << " " << entry.num().value();
      } else if (entry.numf()) {
        std::cout << " " << entry.numf().value();
      }
    }
    std::cout << std::endl;

  }
}


// Three default and free maxmind db
std::string ASN{"/usr/share/GeoIP/GeoLite2-ASN.mmdb"};
std::string CITY{"/usr/share/GeoIP/GeoLite2-City.mmdb"};
std::string COUNTRY{"/usr/share/GeoIP/GeoLite2-Country.mmdb"};

using fragments = std::vector<std::vector<std::string>>;

int main(int argc, char** argv) {

// Variables
  std::vector<std::string> ARGentries;
  std::vector<std::string> ARGfields{"continent", "country"};
  std::string ARGlang{"en"};
  std::optional<std::string> ARGfile{std::nullopt};
  bool FLAGasn{false};
  bool FLAGcity{false};
  bool FLAGcountry{false};
  bool FLAGdump{false};
  bool FLAGinfo{false};

  CLI::App app{"A tool to inspect GeoIP, " AUTHOR " " VERSION};
  app.add_option("entries", ARGentries, "domain name or ip address (4/6) to lookup")->required();
  auto OPTformat = app.add_option("-F,--format", ARGfields, "result entry formats");  // TODO
  app.add_option("--lang", ARGlang, "language to display");
  app.add_flag("-s,--asn", FLAGasn, "show ASN number, aka. Autonomous System Numbers");
  app.add_option("-f", ARGfile, "use the specific mmdb file")->needs(OPTformat);  // TODO
  auto OPTcity = app.add_flag("-c,--city", FLAGcity, "show city information (more verbose, country information is included)");
  auto OPTcountry = app.add_flag("-C,--country", FLAGcountry, "show country information");
  app.add_flag("--dump,-d", FLAGdump, "dump queried result content, json format");
  app.add_flag("-i,--info", FLAGinfo, "query addrinfo for a host, support both ipv4/6 and domain name but there is a network request");
  CLI11_PARSE(app, argc, argv);

// SETTINGS, TODO
  OPTcity->excludes(OPTcountry);
  /* OPTcountry->excludes(OPTcity); */

// PROGRAMS

  if (!(FLAGcity || FLAGcountry || FLAGdump || FLAGasn))
    FLAGcountry = true;

  for (auto& entry : ARGentries) {
    if (FLAGinfo) {

      if (FLAGasn) {
        lookupName(entry, ASN,
          fragments {
            {"autonomous_system_number"}
          , {"autonomous_system_organization"}
          }, FLAGdump
        );
      }
      if (FLAGcity) {
        lookupName(entry, CITY,
          fragments {
            {"continent",  "names", ARGlang}
          , {"country",  "names", ARGlang}
          , {"location", "time_zone"}
          , {"location", "latitude"}
          , {"location", "longitude"}
          }, FLAGdump
        );
      } else if (FLAGcountry) {
        lookupName(entry, COUNTRY,
          fragments {
            {"continent",  "names", ARGlang}
          , {"country",  "names", ARGlang}
          }, FLAGdump
        );
      }


    } else {

      if (FLAGasn) {
        lookup(entry, ASN,
          fragments {
            {"autonomous_system_number"}
          , {"autonomous_system_organization"}
          }, FLAGdump
        );
      }
      if (FLAGcity) {
        lookup(entry, CITY,
          fragments {
            {"continent",  "names", ARGlang}
          , {"country",  "names", ARGlang}
          , {"location", "time_zone"}
          , {"location", "latitude"}
          , {"location", "longitude"}
          }, FLAGdump
        );
      } else if (FLAGcountry) {
        lookup(entry, COUNTRY,
          fragments {
            {"continent",  "names", ARGlang}
          , {"country",  "names", ARGlang}
          }, FLAGdump
        );
      }

    }

  }


  return 0;
}
