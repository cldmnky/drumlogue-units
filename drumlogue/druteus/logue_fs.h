/*
 * File: logue_fs.h
 *
 * logue SDK 2.x file system utils
 *
 * 2025-2026 (c) Oleg Burdaev
 * mailto: dukesrg@gmail.com
 */
#pragma once
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct fs_dir {
  int count;
  struct dirent **dirlist;
  const char *path;
  struct {
    const char *prefix;
    const char *suffix;
  } filter;
  char *suffix_uc;

  static const fs_dir *&self() {
    static thread_local const fs_dir *ptr = nullptr;
    return ptr;
  }

  static int flt(const struct dirent *entry) {
    const fs_dir *s = self();
    const size_t name_len = strlen(entry->d_name);
    const size_t suffix_len = s->filter.suffix ? strlen(s->filter.suffix) : 0;
    const size_t suffix_uc_len = s->suffix_uc ? strlen(s->suffix_uc) : 0;
    return (entry->d_type == DT_REG || entry->d_type == DT_LNK)
      && (s->filter.prefix == nullptr
          || strncmp(entry->d_name, s->filter.prefix, strlen(s->filter.prefix)) == 0)
      && (s->filter.suffix == nullptr
          || (name_len >= suffix_len
              && strcmp(entry->d_name + name_len - suffix_len, s->filter.suffix) == 0)
          || (name_len >= suffix_uc_len
              && strcmp(entry->d_name + name_len - suffix_uc_len, s->suffix_uc) == 0));
  }

  void cleanup() {
    if (dirlist != nullptr) {
      for (int i = 0; i < count; ++i)
        free(dirlist[i]);
      free(dirlist);
      dirlist = nullptr;
    }
    count = 0;
    free(suffix_uc);
    suffix_uc = nullptr;
  }

  char *get(int index) {
    return dirlist[index]->d_name;
  }

  void remove(int index) {
    for (; index < count - 1; index++)
      dirlist[index] = dirlist[index + 1];
    count--;
  }

  void refresh() {
    cleanup();
    if (filter.suffix != nullptr) {
      suffix_uc = strdup(filter.suffix);
      for (char *p = suffix_uc; *p != 0; p++)
        *p = toupper(*p);
    }
    self() = this;
    count = scandir(path, &dirlist, flt, alphasort);
    self() = nullptr;
  }

  void refresh(const char *suffix) {
    filter.suffix = suffix;
    refresh();
  }

  void refresh(const char *prefix, const char *suffix) {
    filter.prefix = prefix;
    filter.suffix = suffix;
    refresh();
  }

  void init() {
    count = 0;
    dirlist = nullptr;
    suffix_uc = nullptr;
    refresh();
  }

  fs_dir(const char *pth, const char *pfx, const char *sfx)
      : count(0), dirlist(nullptr), path(pth),
        filter({.prefix = pfx, .suffix = sfx}), suffix_uc(nullptr) {
    init();
  }

  fs_dir(const char *pth, const char *sfx)
      : count(0), dirlist(nullptr), path(pth),
        filter({.prefix = nullptr, .suffix = sfx}), suffix_uc(nullptr) {
    init();
  }

  fs_dir(const char *pth)
      : count(0), dirlist(nullptr), path(pth),
        filter({.prefix = nullptr, .suffix = nullptr}), suffix_uc(nullptr) {
    init();
  }

  fs_dir()
      : count(0), dirlist(nullptr), path(nullptr),
        filter({.prefix = nullptr, .suffix = nullptr}), suffix_uc(nullptr) {}

  ~fs_dir() {
    cleanup();
  }
};
