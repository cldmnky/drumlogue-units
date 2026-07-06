/**
 * @file unit.cc
 * @brief DRUSYS — drumlogue system diagnostic unit
 *
 * Gathers kernel, CPU, memory, filesystem, and runtime information.
 * Writes a comprehensive report to /var/lib/drumlogued/userfs/Programs/DRUSYS.TXT
 * and displays key stats on the drumlogue screen.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <time.h>

#include "unit.h"

#define OUTPUT_PATH "/var/lib/drumlogued/userfs/Programs/DRUSYS.TXT"

static char g_kernel[32]  = "";
static char g_cpu[32]     = "";
static char g_mem[32]     = "";
static char g_status[48]  = "Scanning...";
static bool g_done         = false;

static FILE *g_out = nullptr;

static void s_write_ln(const char *s) {
  if (!g_out) return;
  fputs(s, g_out);
  fputc('\n', g_out);
}

static void s_write_fmt(const char *fmt, ...) {
  if (!g_out) return;
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  fputs(buf, g_out);
}

static void s_copy_file(const char *path, const char *label) {
  FILE *f = fopen(path, "r");
  if (!f) {
    s_write_fmt("%s: (not found)\n", label);
    return;
  }
  s_write_fmt("--- %s ---\n", label);
  char buf[256];
  while (fgets(buf, sizeof(buf), f))
    fputs(buf, g_out);
  s_write_ln("");
  fclose(f);
}

static void s_write_header(const char *title) {
  s_write_ln("========================================");
  s_write_fmt("  %s\n", title);
  s_write_ln("========================================");
  s_write_ln("");
}

static void s_scan_dir(const char *path, int depth, int max_depth) {
  if (depth > max_depth) return;
  DIR *d = opendir(path);
  if (!d) return;

  char indent[128];
  int n = (depth < (int)sizeof(indent) - 1) ? depth * 2 : 0;
  memset(indent, ' ', n);
  indent[n] = '\0';

  struct dirent *e;
  while ((e = readdir(d)) != nullptr) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;

    char full[512];
    snprintf(full, sizeof(full), "%s/%s", path, e->d_name);

    struct stat st;
    int ret = stat(full, &st);
    if (ret != 0) {
      s_write_fmt("%s? %s\n", indent, e->d_name);
      continue;
    }

    if      (S_ISDIR(st.st_mode)) s_write_fmt("%sd %s/\n", indent, e->d_name);
    else if (S_ISLNK(st.st_mode)) s_write_fmt("%sl %s ->\n", indent, e->d_name);
    else if (S_ISREG(st.st_mode)) s_write_fmt("%s  %s (%ld)\n", indent, e->d_name, (long)st.st_size);
    else                          s_write_fmt("%s? %s\n", indent, e->d_name);

    if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode))
      s_scan_dir(full, depth + 1, max_depth);
  }
  closedir(d);
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != unit_header.target)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;
  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;
  if (desc->output_channels != 2)
    return k_unit_err_geometry;

  g_out = fopen(OUTPUT_PATH, "w");
  if (!g_out) {
    snprintf(g_status, sizeof(g_status), "ERR: fopen fail");
    return k_unit_err_none;
  }

  s_write_ln("Drumlogue System Diagnostic");
  s_write_ln("===========================");
  {
    time_t t = time(nullptr);
    char *ts = ctime(&t);
    s_write_fmt("Generated: %s", ts ? ts : "unknown\n");
  }
  s_write_ln("");

  s_write_header("Runtime Descriptor");
  s_write_fmt("Sample rate:      %u Hz\n", desc->samplerate);
  s_write_fmt("Frames per buffer: %u (%0.2f ms)\n", desc->frames_per_buffer,
              desc->frames_per_buffer * (1000.0f / desc->samplerate));
  s_write_fmt("Input channels:   %u\n", desc->input_channels);
  s_write_fmt("Output channels:  %u\n", desc->output_channels);
  s_write_fmt("Target:           0x%04X\n", desc->target);
  s_write_fmt("SDK API:          0x%08X\n", desc->api);
  s_write_fmt("Unit name:        %s\n", unit_header.name);
  s_write_fmt("Developer ID:     0x%08X\n", unit_header.dev_id);
  s_write_fmt("Unit ID:          0x%08X\n", unit_header.unit_id);
  s_write_fmt("Version:          0x%06X (%d.%d.%d)\n", unit_header.version,
              (unit_header.version >> 16) & 0xFF,
              (unit_header.version >> 8) & 0xFF,
              unit_header.version & 0xFF);
  s_write_ln("");

  struct utsname u;
  if (uname(&u) == 0) {
    s_write_header("Kernel (uname)");
    s_write_fmt("sysname:  %s\n", u.sysname);
    s_write_fmt("nodename: %s\n", u.nodename);
    s_write_fmt("release:  %s\n", u.release);
    s_write_fmt("version:  %s\n", u.version);
    s_write_fmt("machine:  %s\n", u.machine);
    s_write_ln("");

    snprintf(g_kernel, sizeof(g_kernel), "%s %s", u.sysname, u.release);
    snprintf(g_cpu, sizeof(g_cpu), "%s", u.machine);
    snprintf(g_status, sizeof(g_status), "Kernel OK");
  }

  s_copy_file("/proc/cpuinfo", "CPU Info");

  {
    int cores = 0;
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
      char buf[128];
      while (fgets(buf, sizeof(buf), f))
        if (strncmp(buf, "processor", 9) == 0)
          cores++;
      fclose(f);
    }
    snprintf(g_cpu, sizeof(g_cpu), "%s %dcore", u.machine, cores);
  }

  s_copy_file("/proc/meminfo", "Memory Info");

  {
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
      char buf[128];
      unsigned long total_kb = 0;
      while (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, "MemTotal:", 9) == 0) {
          sscanf(buf + 9, "%lu", &total_kb);
          break;
        }
      }
      fclose(f);
      snprintf(g_mem, sizeof(g_mem), "%lu MB total",
               total_kb / 1024);
    }
  }

  s_copy_file("/proc/version", "Kernel Version");
  s_copy_file("/proc/mounts", "Mount Points");
  s_copy_file("/proc/self/maps", "Process Memory Map");
  s_copy_file("/proc/uptime", "Uptime");
  s_copy_file("/proc/loadavg", "Load Average");

  s_write_header("Filesystem Tree");
  s_write_ln("(d=directory, l=symlink, size in bytes)");
  s_write_ln("");

  s_scan_dir("/etc", 0, 2);
  s_scan_dir("/tmp", 0, 2);
  s_scan_dir("/var/lib/drumlogued", 0, 3);
  s_scan_dir("/var/lib/drumlogued/userfs", 0, 4);

  s_write_ln("");
  s_write_header("Running Processes");
  {
    DIR *d = opendir("/proc");
    if (d) {
      struct dirent *e;
      while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9')
          continue;

        int pid = atoi(e->d_name);
        if (pid <= 0) continue;

        char path[128];
        char buf[256];
        char comm[64] = "";
        char state[16] = "";
        char cmdline[256] = "";
        unsigned long vm_rss = 0;
        int threads = 1;
        int ppid = 0;
        unsigned long utime = 0, stime = 0;

        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        FILE *f = fopen(path, "r");
        if (f) {
          if (fgets(comm, sizeof(comm), f)) {
            size_t len = strlen(comm);
            if (len > 0 && comm[len - 1] == '\n')
              comm[len - 1] = '\0';
          }
          fclose(f);
        }

        snprintf(path, sizeof(path), "/proc/%s/cmdline", e->d_name);
        f = fopen(path, "r");
        if (f) {
          size_t n = fread(buf, 1, sizeof(buf) - 1, f);
          if (n > 0) {
            buf[n] = '\0';
            for (size_t i = 0; i < n; i++)
              if (buf[i] == '\0') buf[i] = ' ';
            size_t len = strlen(buf);
            while (len > 0 && buf[len - 1] == ' ')
              buf[--len] = '\0';
            snprintf(cmdline, sizeof(cmdline), "%s", buf);
          }
          fclose(f);
        }

        snprintf(path, sizeof(path), "/proc/%s/status", e->d_name);
        f = fopen(path, "r");
        if (f) {
          while (fgets(buf, sizeof(buf), f)) {
            if (strncmp(buf, "State:", 6) == 0) {
              char *s = buf + 7;
              while (*s == ' ' || *s == '\t') s++;
              size_t len = strlen(s);
              if (len > 0 && s[len - 1] == '\n')
                s[len - 1] = '\0';
              snprintf(state, sizeof(state), "%s", s);
            }
            if (strncmp(buf, "VmRSS:", 6) == 0) {
              sscanf(buf + 6, "%lu", &vm_rss);
            }
            if (strncmp(buf, "Threads:", 8) == 0) {
              sscanf(buf + 8, "%d", &threads);
            }
            if (strncmp(buf, "PPid:", 5) == 0) {
              sscanf(buf + 5, "%d", &ppid);
            }
          }
          fclose(f);
        }

        snprintf(path, sizeof(path), "/proc/%s/stat", e->d_name);
        f = fopen(path, "r");
        if (f) {
          if (fgets(buf, sizeof(buf), f)) {
            int dummy;
            char dummy_s[64];
            sscanf(buf, "%d %s %c %d %d %d %d %d %*u %*u %*u %*u %*u %lu %lu",
                   &dummy, dummy_s, dummy_s, &dummy, &dummy, &dummy,
                   &dummy, &dummy, &utime, &stime);
          }
          fclose(f);
        }

        if (cmdline[0]) {
          s_write_fmt("  PID %5d  %s  [%s]  %4lu kB  %d thr  PPID %d  CPU %lu+%lu\n",
                      pid, comm, state, vm_rss, threads, ppid, utime, stime);
          s_write_fmt("           %s\n", cmdline);
        } else {
          s_write_fmt("  PID %5d  %s  [%s]  %4lu kB  %d thr  PPID %d  CPU %lu+%lu\n",
                      pid, comm, state, vm_rss, threads, ppid, utime, stime);
        }
      }
      closedir(d);
    }
  }

  s_write_ln("");
  s_write_header("/proc Entries");
  {
    DIR *d = opendir("/proc");
    if (d) {
      struct dirent *e;
      int count = 0;
      while ((e = readdir(d)) != nullptr) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
          continue;
        s_write_fmt("  /proc/%s\n", e->d_name);
        count++;
      }
      s_write_fmt("\n  %d entries\n", count);
      closedir(d);
    }
  }

  s_write_ln("");
  s_write_header("Thermal / Hardware");

  s_copy_file("/sys/class/thermal/thermal_zone0/temp", "thermal_zone0/temp");
  s_copy_file("/sys/class/thermal/thermal_zone0/type", "thermal_zone0/type");

  {
    DIR *d = opendir("/sys/class/thermal");
    if (d) {
      struct dirent *e;
      while ((e = readdir(d)) != nullptr) {
        if (strncmp(e->d_name, "thermal_zone", 12) != 0)
          continue;
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp", e->d_name);
        FILE *f = fopen(path, "r");
        if (f) {
          int temp = 0;
          fscanf(f, "%d", &temp);
          s_write_fmt("%s: %d.%03d C\n", e->d_name, temp / 1000, temp % 1000);
          fclose(f);
        }
      }
      closedir(d);
    }
  }

  s_write_ln("");
  s_write_ln("=== END OF REPORT ===");
  s_write_fmt("File size: %ld bytes\n", ftell(g_out));

  fclose(g_out);
  g_out = nullptr;
  g_done = true;

  snprintf(g_status, sizeof(g_status), "Done: DRUSYS.TXT");

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {
  if (g_out) fclose(g_out);
}

__unit_callback void unit_reset() {}
__unit_callback void unit_suspend() {}
__unit_callback void unit_resume() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
  (void)in;
  memset(out, 0, frames * 2 * sizeof(float));
}

__unit_callback void unit_set_param_value(uint8_t index, int32_t value) {
  (void)index;
  (void)value;
}

__unit_callback int32_t unit_get_param_value(uint8_t index) {
  (void)index;
  return 0;
}

__unit_callback const char *unit_get_param_str_value(uint8_t index, int32_t value) {
  (void)value;
  switch (index) {
    case 0: return g_kernel;
    case 1: return g_cpu;
    case 2: return g_mem;
    case 3: return g_status;
    default: return nullptr;
  }
}

__unit_callback const uint8_t *unit_get_param_bmp_value(uint8_t index, int32_t value) {
  (void)index;
  (void)value;
  return nullptr;
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  (void)note;
  (void)velocity;
}

__unit_callback void unit_note_off(uint8_t note) {
  (void)note;
}

__unit_callback void unit_gate_on(uint8_t velocity) {
  (void)velocity;
}

__unit_callback void unit_gate_off() {}

__unit_callback void unit_all_note_off() {}

__unit_callback void unit_pitch_bend(uint16_t pitch_bend) {
  (void)pitch_bend;
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  (void)pressure;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  (void)note;
  (void)aftertouch;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
  (void)counter;
}

__unit_callback void unit_load_preset(uint8_t idx) {
  (void)idx;
}

__unit_callback uint8_t unit_get_preset_index() {
  return 0;
}

__unit_callback const char *unit_get_preset_name(uint8_t idx) {
  (void)idx;
  return nullptr;
}
