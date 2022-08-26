#pragma once

#ifndef MAXMIND_H
#define MAXMIND_H

#include <string>
#include <filesystem>
#include <optional>
#include <vector>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "maxminddb.h"

struct Handle_t {
  Handle_t() : handle{nullptr}, status{false} {}
  Handle_t(Handle_t&& origin) : handle{std::move(origin.handle)} {
    origin.moved = true;
  }
  Handle_t(std::unique_ptr<MMDB_s> h) : handle{std::move(h)} {}
  Handle_t(std::unique_ptr<MMDB_s> h, bool s) : handle{std::move(h)}, status{s} {}
  Handle_t& operator=(Handle_t&& origin) {
    origin.moved = true;
    handle = std::move(origin.handle);
    return *this;
  }
  ~Handle_t() {
    if (status && !moved) {
      MMDB_close(handle.get());
      status = false;
    }
  }
  MMDB_s* ptr() { return handle.get(); }
  operator bool() { return status; }

  private:
  std::unique_ptr<MMDB_s> handle;
  bool status{true};
  bool moved{false};
};

struct Entry : MMDB_entry_data_s {
  using Base = MMDB_entry_data_s;

  enum struct EntryType : uint32_t {
    TSTRING = MMDB_DATA_TYPE_UTF8_STRING,
    TDOUBLE = MMDB_DATA_TYPE_DOUBLE,
    TBYTES = MMDB_DATA_TYPE_BYTES,
    TUINT16 = MMDB_DATA_TYPE_UINT16,
    TUINT32 = MMDB_DATA_TYPE_UINT32,
    TMAP = MMDB_DATA_TYPE_MAP,
    TINT32 = MMDB_DATA_TYPE_INT32,
    TUINT64 = MMDB_DATA_TYPE_UINT64,
    TUINT128 = MMDB_DATA_TYPE_UINT128,
    TARRAY = MMDB_DATA_TYPE_ARRAY,
    TBOOLEAN = MMDB_DATA_TYPE_BOOLEAN,
    TFLOAT = MMDB_DATA_TYPE_FLOAT,
  } ;

  Entry() {}
  Entry(Base v) : Base{v} {}

  Base* underlying() {
    return static_cast<Base*>(this);
  }
  EntryType get_type() {
    return EntryType {Base::type};
  }
  std::optional<std::string> str() {
    if (get_type() == EntryType::TSTRING) {
      return std::string {Base::utf8_string, Base::data_size};
    }
    return std::nullopt;
  }
  std::optional<long long> num() {
    switch (get_type()) {
      case EntryType::TUINT16:
        return Base::uint16;
      case EntryType::TUINT32:
        return Base::uint32;
      case EntryType::TUINT64:
        return Base::uint64;
      case EntryType::TINT32:
        return Base::int32;
      default:
        return std::nullopt;
    }
  }
  std::optional<double> numf() {
    if (get_type() == EntryType::TDOUBLE) {
      return Base::double_value;
    }
    return std::nullopt;
  }
  operator bool() { return Base::has_data; }
};

struct LookupResult : MMDB_lookup_result_s {
  using Base = MMDB_lookup_result_s;
  LookupResult() {}
  LookupResult(Base v) : Base {v} {}
  LookupResult(Base v, std::string ip) : Base {v}, canonical_ip{ip} {}
  LookupResult& operator = (Base v) {
    *static_cast<Base*>(this) = v;
    return *this;
  }
  Base* underlying() {
    return static_cast<Base*>(this);
  }
  operator bool() { return Base::found_entry; }
  inline unsigned int mask() { return Base::netmask; }
  Entry value(std::vector<std::string> tokens) {
    Entry entry;
    std::vector<const char*> paths;

    for (auto& s : tokens) { paths.push_back(s.c_str()); }
    paths.push_back(nullptr);

    status = MMDB_aget_value(&this->entry, entry.underlying(), paths.data());
    return entry;
  }
  int status;
  int gai_error;
  int mmdb_error;
  int port{0};
  std::string canonical_ip{""};
};

struct EntryList {
  EntryList() : data{nullptr} {}
  EntryList(LookupResult& result) {
    status = MMDB_get_entry_data_list(&result.entry, &data);
  }
  EntryList(EntryList&& ls) : data{ls.data}, status{ls.status} {
    ls.moved = true;
  }
  EntryList(EntryList const&) = delete;
  EntryList& operator=(EntryList&& ls) {
    data = ls.data;
    status = ls.status;
    ls.moved = true;
    return *this;
  }
  ~EntryList() {
    if (!moved && data != nullptr)
      MMDB_free_entry_data_list(data);
  }
  void dump() {
    if (data != nullptr && status == MMDB_SUCCESS) {
      MMDB_dump_entry_data_list(stdout, data, 2);
    }
  }

  MMDB_entry_data_list_s* data;
  int status;
  private:
  bool moved{false};
};

// ADDRINFO TODO
struct AddrInfo {

  enum struct AddrClass {
    V4, V6
  };

  AddrInfo() {}
  AddrInfo(AddrInfo&& i) : info{i.info}, status{i.status} {
    i.moved = true;
  }
  AddrInfo(std::string host) {
    addrinfo hints {
      .ai_flags = (AI_V4MAPPED | AI_ADDRCONFIG)
    , .ai_family = AF_UNSPEC
    , .ai_socktype = SOCK_RAW
    };
    status = getaddrinfo(host.c_str(), nullptr, &hints, &info);
  }
  ~AddrInfo() {
    if (!moved && info != nullptr) {
      freeaddrinfo(info);
    }
  }

  // TODO: handle error here
  static std::string inet_str(addrinfo* i) {
    char buf[64];
    switch (i->ai_addr->sa_family) {
      case AF_INET:
        inet_ntop(AF_INET, &reinterpret_cast<struct sockaddr_in*>(i->ai_addr)->sin_addr, buf, 64);
        break;
      case AF_INET6:
        inet_ntop(AF_INET6, &reinterpret_cast<struct sockaddr_in6*>(i->ai_addr)->sin6_addr, buf, 64);
        break;
      default:
        return "UnknownIP";
    }
    return buf;
  }

  static int inet_port(addrinfo* i) {
    switch (i->ai_addr->sa_family) {
      case AF_INET:
        return ntohl(reinterpret_cast<struct sockaddr_in*>(i->ai_addr)->sin_port);
        break;
      case AF_INET6:
        return ntohl(reinterpret_cast<struct sockaddr_in6*>(i->ai_addr)->sin6_port);
        break;
      default:
        return 0;
    }
  }

  operator bool() {
    return status == 0;
  }

  addrinfo* info{nullptr};
  int status;
  private:
  bool moved{false};
};
// END ADDRINFO

class maxmind {
  public:
    maxmind(std::filesystem::path);
    maxmind(maxmind const&) = delete;
    std::string status_msg();
    int status_code();
    operator bool();
    LookupResult lookup(std::string);
    std::vector<LookupResult> lookupName(std::string);
    void print();

  private:
    int status; // to store any status code, stateful
    Handle_t handle;
    EntryList links;
};

#endif
