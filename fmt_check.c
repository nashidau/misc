#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum {
  FmtTypeStructTmPtr = 1000,
  FmtTypePoint,
  FmtTypeFloat,
  FmtTypeDouble,
};

typedef struct Point {
  int x;
  int y;
} Point;

#define FMT_CUSTOM_TYPES(_) \
  _(struct tm *, FmtTypeStructTmPtr) \
  _(Point, FmtTypePoint) \
  _(float, FmtTypeFloat) \
  _(double, FmtTypeDouble)

#define FMT_IMPL
#include "fmt.h"

bool fmt_custom_arg(FmtArg arg, FmtSpec spec,
                    void *userdata, FmtFormatOutput *format_output) {
  (void) userdata;
  if (arg.type == FmtTypeStructTmPtr) {
    char timefmt_buf[spec.custom_len + 1];
    char *timefmt = timefmt_buf;
    if (spec.custom_len > 0) {
      memcpy(timefmt_buf, spec.custom_start, spec.custom_len);
      timefmt[spec.custom_len] = '\0';
    } else {
      timefmt = "%Y-%m-%d";
    }
    struct tm *tm = *(struct tm **)arg.data;
    int res = strftime(format_output->text, FMT_SHOW_BUF_MAX, timefmt, tm);
    format_output->text_size = res;
    return true;
  }
  if (arg.type == FmtTypePoint) {
    struct Point p = *(struct Point *)arg.data;
    format_output->text_size = fmt_sn(format_output->text, FMT_SHOW_BUF_MAX,
                               "{{{},{}}", p.x, p.y); // }
    return true;
  }
  if (arg.type == FmtTypeFloat || arg.type == FmtTypeDouble) {
    double x = arg.type == FmtTypeDouble ? *(double *)arg.data
                                         : *(float *)arg.data;
    char format_buf[10];
    {
      int i = 0;
      format_buf[i++] = '%';
      for (size_t j = 0; j < spec.custom_len && j < 5; j++) {
        format_buf[i++] = spec.custom_start[j];
      }
      format_buf[i++] = 'f';
      format_buf[i++] = '\0';
    }

    int wanted = snprintf(format_output->text, FMT_SHOW_BUF_MAX, format_buf, x);
    format_output->text_size = wanted < FMT_SHOW_BUF_MAX ? wanted : FMT_SHOW_BUF_MAX - 1;
    return true;
  }
  return false;
}

// malloc a nul-terminated string and print into it.
char *fmt_malloc_va(const char *fmt, ...) {
  FmtState state;
  va_list va;
  va_start(va, fmt);
  fmt_init(&state, fmt, va);
  va_end(va);

  fmt_chunk(&state, 0, 0);
  size_t counted_size = state.size;
  fmt_reset(&state);

  char *mem = malloc(counted_size + 1);
  if (!mem) return 0;

  fmt_chunk(&state, mem, counted_size);
  size_t formatted_size = state.size;
  assert(counted_size == formatted_size);
  mem[formatted_size] = '\0';

  return mem;
}

#define fmt_malloc(fmt, ...) \
  fmt_malloc_va((fmt), FMT_ARGS(unused, ##__VA_ARGS__) FMT_ARG_END)


int main(int argc, char **argv) {
  char c = 'x';
  time_t now = time(0);
  struct tm *tm = localtime(&now);

  fmt_print("hello {} {}\n", 123, "hi");
  fmt_print("hello {} {:c}\n", c, c);
  fmt_print("hello {:05} 0x{:05x}\n", 123, 0xa66e);
  fmt_print("no arguments\n");
  fmt_print("positional arguments: {1:c}{0:c}{2:c}{2:c}{3:c}\n", 'e', 'h', 'l', 'o');
  fmt_print("pointers: {:p} {:p} {:p}\n", "abc", (void *)0, &c);
  fmt_print("space-padded pointer          : {:16p}\n", "test");
  fmt_print(" zero-padded pointer (whoops!): {:016p}\n", "test");
  fmt_print("custom formatting: now is {|%Y-%m-%d %H:%M:%S} (default format: {0})\n",
            tm);
  fmt_print("->? {}\n", tm->tm_year);

  fmt_print("extraneous {} {1}\n");
  fmt_print("bad format string { blah {5 blah }}\n");
  fmt_print("unknown type {}\n", argv);

  fmt_print("several arguments: {} {} {} {} {} {} {} {} {}\n",
            1, 2, 3, 4, 5, 6, 7, 8, 9);
  fmt_print("indexed arguments: {8} {7} {6} {5} {4} {3} {2} {1} {0}\n",
            1, 2, 3, 4, 5, 6, 7, 8, 9);
  fmt_print("escaping: {{ }}\n");

  fmt_print("bools: {:5} {:5}\n", (bool) 0, (bool) 1);

  fmt_print("INT64_MIN: {}\n", INT64_MIN);

  // Uh oh: MACRO((T){x, y}) is expanded as two arguments: "(T){x" and "y}", so
  // you need extra parentheses. Is there a way around that?
  fmt_print("point: {}\n", ((Point){1, 2}));

  fmt_print("{} command-line argument{}:\n", argc, argc == 1 ? "" : "s");
  for (int i = 0; i < argc; i++) {
    fmt_print("  argv[{:-3}] = {}\n", i, argv[i]);
  }

  {
    // Test snprintf-style printing.
    char buf[5];
    int res;

    res = fmt_sn(buf, sizeof buf, "abcd");
    fmt_print("{} {}\n", res, buf);

    res = fmt_sn(buf, sizeof buf, "abcde");
    fmt_print("{} {}\n", res, buf);

    res = fmt_sn(buf, sizeof buf, "{}", "fghi");
    fmt_print("{} {}\n", res, buf);

    res = fmt_sn(buf, sizeof buf, "{}", "fghij");
    fmt_print("{} {}\n", res, buf);

    res = fmt_sn(0, 0, "{} {}", 123, "hello");
    fmt_print("length: {}\n", res);

    buf[0] = 'A';
    fmt_sn(buf, sizeof buf, "");
    fmt_print("empty fmt_sn terminator: {}\n", buf[0]);
  }

  {
    float x = 0.1;
    double y = 0.2;
    fmt_print("float {} + double {} = {|.10}\n", x, y, x + y);
  }

  {
    char *s = fmt_malloc("{} {}", "some memory", 123);
    fmt_print("allocated: {}\n", s);
    free(s);
  }

  return 0;
}
