#include "maxmind.hpp"

// Include c implementation header
#include "maxminddb.h"

#include <filesystem>
namespace fs = std::filesystem;

#include <utility>
#include <iostream>
#include <vector>

// C files
#include <cstdlib>

LookupResult maxmind::lookup(std::string ip) {
  /* int gai_error, mmdb_error; */
  LookupResult result;
  result = MMDB_lookup_string(handle.ptr(), ip.c_str(), &result.gai_error, &result.mmdb_error);
  /* if (gai_error != 0) { */
  /*   std::cerr << "Error on geting info for <" << ip << ">" << std::endl; */
  /*   return false; */
  /* } else if (mmdb_error != MMDB_SUCCESS) { */
  /*   std::cerr << "Error from libmaxmind DB" << std::endl; */
  /*   return false; */
  /* } else if (!result.found_entry) { */
  /*   std::cerr << "Result not found for <" << ip << ">" << std::endl; */
  /*   return false; */
  /* } */
  links = EntryList {result};

  return result;
}

std::vector<LookupResult> maxmind::lookupName(std::string host) {
  std::vector<LookupResult> results;
  AddrInfo info{host};
  if (!info) return results;
  auto ptr = info.info;
  while (ptr != nullptr) {
    LookupResult result;
    result = MMDB_lookup_sockaddr(handle.ptr(), ptr->ai_addr, &result.mmdb_error);
    result.canonical_ip = AddrInfo::inet_str(ptr);
    result.port = AddrInfo::inet_port(ptr);
    results.push_back(result);
    ptr = ptr->ai_next;
  }
  return results;
}

void maxmind::print() {
  if (links.status == MMDB_SUCCESS) {
    links.dump();
  }
}

maxmind::maxmind(fs::path f) {
  MMDB_s shandle;
  status = MMDB_open(f.c_str(), MMDB_MODE_MMAP, &shandle);
  if (status == MMDB_SUCCESS) {
    maxmind::handle = Handle_t {std::unique_ptr<MMDB_s>(new MMDB_s(shandle))};
  } else {
    maxmind::handle = Handle_t {std::unique_ptr<MMDB_s>(new MMDB_s()), false};
  }
}

auto maxmind::status_code() -> int {
  return status;
}

maxmind::operator bool () {
  return status == MMDB_SUCCESS;
}

auto maxmind::status_msg() -> std::string {
  switch (status) {
    default:
    case MMDB_SUCCESS:
      return "everything ok";
    case MMDB_FILE_OPEN_ERROR:
      return "error on opening MaxMind DB file";
    case MMDB_IO_ERROR:
      return "error on IO operation, please check errno";
    case MMDB_CORRUPT_SEARCH_TREE_ERROR:
      return "error of impossible result";
    case MMDB_INVALID_METADATA_ERROR:
      return "error of invalid metadata";
    case MMDB_UNKNOWN_DATABASE_FORMAT_ERROR:
      return "wrong database format, version is not 2";
    case MMDB_OUT_OF_MEMORY_ERROR:
      return "out of memory";
    case MMDB_INVALID_DATA_ERROR:
      return "data section contains invalid data";
    case MMDB_INVALID_LOOKUP_PATH_ERROR:
      return "invalid loopup path";
    case MMDB_LOOKUP_PATH_DOES_NOT_MATCH_DATA_ERROR:
      return "lookup path doesn't match data base";
  }
}
