#if !defined FMT_H
#define FMT_H

// Usage (see main.c for an example):
// Before using fmt, include "fmt.h"; in one translation unit, #define FMT_IMPL
// first to include the implementation.
//
// Then you can use the macros:
//   int fmt_sn(char *buf, size_t size, const char *fmt, ...);
//   int fmt_fprint(FILE *file, const char *fmt, ...);
// Which are analogous to snprintf() and fprintf().
//
// The core API is based on FmtState:
//   FmtState state;
//   fmt_init(&state, fmt, va);
//   while (fmt_chunk(&state, buf, buf_size)) {
//     process(buf, state.size);
//   }
// This API can be used to implement both snprintf and fprintf without
// allocation.
//
// fmt functions need to be defined as macros, because the varargs include
// type information and a sentinel. See the implementations of fmt_sn(),
// fmt_fprint(), and fmt_malloc() for examples.
//
// To handle custom arguments:
// 1. In each translation unit, before including fmt.h, define
/*
     #define FMT_CUSTOM_TYPES(_) \
       _(YourType, YourTypeId) \
       _(OtherType, OtherTypeId) \
*/
// A type ID can be any integer smaller than FmtArgFirstBuiltin (30000).
// 2. In one translation unit, define a custom formatter
//   bool fmt_custom_arg(FmtArg arg, FmtSpec spec,
//                       void *userdata,
//                       FmtFormatOutput *format_output);
//
// A custom formatter can write up to FMT_SHOW_BUF_MAX bytes (64 by default)
// into the buffer format_output->text points to by default. Alternatively, it
// can write more text into a different buffer (this buffer must be valid until
// it's fully printed, which might take multiple calls to fmt_chunk).
// If it knows how to format a type, it should return true and set
// format_output->text and format_output->size appropriately. Otherwise it
// should return false and not mutate *format_output.
//
// You can set the userdata field on FmtState directly (for example, it might
// point to an arena allocator that it can use for temporary storage of longer
// buffers).
//
// Padding is also supported:
// You can set pad_mode to FmtPadLeft or FmtPadRight, or to FmtPadCustomPos to
// insert padding at a fixed position. You can also set it to FmtPadManual to
// calculated padding amounts yourself.
// You can call va_end after fmt_init(), but the lifetime of the FmtState is
// tied to the varargs function stack frame.
//
//
// Format specifiers are written in the form {id:fmt|custom}.
// * id is the index of the argument. If not specified, the next argument is used.
// * fmt is a printf-style format specifier:
//   It consists of a total formatted size (possibly preceded by 0 to use '0'
//   instead of ' ' for padding), followed by one of the characters x (hexadecimal),
//   b (binary), c (character), or p (pointer). All parts are optional.
//   :p can be applied to any argument to print it as a pointer (note: if the
//   argument is smaller than a pointer, this will read adjacent memory). The
//   other specifiers are for integer types.
// * custom is a string passed into the custom formatting function.
// Examples:
//   {}        // print next argument with default formatting
//   {1:08}    // print second argument as 8 bytes, padded with 0s.
//   {4|hello} // print fifth argument, passing "hello" to fmt_custom_arg
//   {:p}      // print next argument as a pointer
//
// fmt.h requires C11 _Generic and the GNU __typeof__ extension, which means
// it's compatible with gcc, clang, and tcc, but not e.g. MSVC.

// TODO: Better error reporting?

// TODO: A general-purpose format specifier to see any value as U64 or something
// would probably be nice.

// TODO: Floating point support would probably be nice. For now you can
// implement it yourself in terms of snprintf (see main.c).

// TODO: UTF-8 for {:c}?

// TODO: Maybe fmt_custom_arg should always be called if {|...} is passed,
// even for known types.

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef int32_t FmtArgType;

enum {
  // show_U64_bin assumes this is at least 64.
  FMT_SHOW_BUF_MAX = 64,
};

typedef enum FmtPadMode {
  FmtPadLeft,
  FmtPadRight,
  FmtPadCustomPos, // Calculate amount of padding and insert at fixed position.
  FmtPadManual, // Use the pad_pos and pad_size from the FmtFormatOutput.
} FmtPadMode;

typedef struct FmtArg {
  void *data;
  FmtArgType type;
} FmtArg;

typedef struct FmtSpec {
  size_t min_len; // in bytes; should be in code points?
  const char *custom_start;
  size_t custom_len;
  FmtPadMode pad_mode;
  char pad_byte;
   // 0 for default, 'b' for binary, 'x' for hexadecimal, 'c' for character, 'p'
   // for pointer
  char format;
} FmtSpec;

// Rename this!
typedef struct FmtFormatOutput {
  char *text;
  size_t text_size;
  size_t pad_pos;
  int pad_size;
  FmtPadMode pad_mode;
  char pad_byte;
} FmtFormatOutput;

bool fmt_custom_arg(FmtArg arg, FmtSpec spec,
                    void *userdata, FmtFormatOutput *format_output);

enum { FMT_MAX_ARGS = 9 };

typedef enum FmtAction {
  FmtActionParsing,
  FmtActionFormatting,
  FmtActionDone,
} FmtAction;

typedef struct FmtState {
  FmtArg args[FMT_MAX_ARGS];
  int arg_count;

  const char *fmt_at_init;
  const char *fmt;
  void *userdata;

  size_t size;

  int next_arg_ix;

  FmtAction action;

  // Formatting:
  FmtFormatOutput format_output;
  char text_buf[FMT_SHOW_BUF_MAX];
} FmtState;

// Initialize an FmtState by calling fmt_init() with a va_list (the varargs must
// be terminated by the sentinel FMT_ARG_END).
//
// Then, fmt_chunk() can fill a buffer with more output. It returns false
// when there's no more output to produce, and returns the amount it wrote in
// state.size.
//
// If you pass a null pointer in for buf, fmt_chunk will process the rest of
// the format string, and return the size it would have needed in state.size
// (without writing into buf).
//
// fmt_reset() can reset an FmtState to the beginning, so you can e.g. write
// into a buffer after calculating the size.
//
// fmt_chunk() does not nul-terminate its output!
void fmt_init(FmtState *state, const char *fmt, va_list va);
bool fmt_chunk(FmtState *state, char *buf, size_t size);
void fmt_reset(FmtState *fmt);

// Utilities:
int fmt_sn_va(char *buf, size_t size, const char *fmt, ...);
int fmt_fprint_va(FILE *file, const char *fmt, ...);

#define fmt_sn(buf, size, fmt, ...) \
  fmt_sn_va((buf), (size), (fmt), FMT_ARGS(unused, ##__VA_ARGS__) FMT_ARG_END)

#define fmt_fprint(file, fmt, ...) \
  fmt_fprint_va((file), (fmt), FMT_ARGS(unused, ##__VA_ARGS__) FMT_ARG_END)

#define fmt_print(fmt, ...) \
  fmt_fprint(stdout, (fmt), ##__VA_ARGS__)

#define fmt_show(x) fmt_print("fmt_show(" #x "): {}\n", (x))

// TODO: Put the above in some sort of UTILS #if.


#if !defined FMT_CUSTOM_TYPES
  #define FMT_CUSTOM_TYPES(_)
  #define FMT__DEFAULT_CUSTOM_ARG 1
#else
  #define FMT__DEFAULT_CUSTOM_ARG 0
#endif

enum {
  FmtArgFirstBuiltin = 30000,
  FmtArgUnknown,
  FmtArgS64,
  FmtArgS32,
  FmtArgS16,
  FmtArgS8,
  FmtArgU64,
  FmtArgU32,
  FmtArgU16,
  FmtArgU8,

  FmtArgChar, // signed/unsigned char?
  FmtArgBool,
  // float types?
  // Are other integer types plausibly different from the ones listed above?

  FmtArgCharPtr,
  FmtArgVoidPtr,
};


// (__typeof__(x)[]{x}) is a problem when x is a string literal, because the
// type of a string literal is (char[]) rather than (char *). Trigger decay
// explicitly. In gcc you can use __typeof__((void)0,(x)), but that's not
// compatible with tcc, so I'm using __typeof__(1 ? (x) : (x)) instead.
// (TODO: Report this GNU incompatibility to tcc?)

#define FMT_ARG(x) \
  ((FmtArg){(__typeof__((1 ? (x) : (x)))[]){(x)}, FMT_MAKE_FMTTYPE(x)})
#define FMT_ARG_END ((FmtArg){0,0})

#define FMT_0(x)
#define FMT_1(x, ...) FMT_ARG(x), FMT_0(__VA_ARGS__)
#define FMT_2(x, ...) FMT_ARG(x), FMT_1(__VA_ARGS__)
#define FMT_3(x, ...) FMT_ARG(x), FMT_2(__VA_ARGS__)
#define FMT_4(x, ...) FMT_ARG(x), FMT_3(__VA_ARGS__)
#define FMT_5(x, ...) FMT_ARG(x), FMT_4(__VA_ARGS__)
#define FMT_6(x, ...) FMT_ARG(x), FMT_5(__VA_ARGS__)
#define FMT_7(x, ...) FMT_ARG(x), FMT_6(__VA_ARGS__)
#define FMT_8(x, ...) FMT_ARG(x), FMT_7(__VA_ARGS__)
#define FMT_9(x, ...) FMT_ARG(x), FMT_8(__VA_ARGS__)

#define FMT_NTH(unused, _1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME
#define FMT_ARGS(unused, ...) \
  FMT_NTH(unused, ##__VA_ARGS__, \
          FMT_9, FMT_8, FMT_7, FMT_6, FMT_5, \
          FMT_4, FMT_3, FMT_2, FMT_1, FMT_0)(__VA_ARGS__)

#define FMT__GENERIC_CASE(type, val) type: val,

// TODO: uint64_t and unsigned long long might both be unsigned 64-bit integers
// on amd64, but they're not the same type, so unsigned long long goes to the
// unknown case.
#define FMT_MAKE_FMTTYPE(x) (_Generic((x), \
  int64_t: FmtArgS64, \
  int32_t: FmtArgS32, \
  int16_t: FmtArgS16, \
  int8_t: FmtArgS8, \
  uint64_t: FmtArgU64, \
  uint32_t: FmtArgU32, \
  uint16_t: FmtArgU16, \
  uint8_t: FmtArgU8, \
  char: FmtArgChar, \
  bool: FmtArgBool, \
  \
  char *: FmtArgCharPtr, \
  void *: FmtArgVoidPtr, \
  FMT_CUSTOM_TYPES(FMT__GENERIC_CASE) \
  default: FmtArgUnknown))

#endif // FMT_H


#if defined FMT_IMPL

#include <assert.h>
#include <ctype.h>
#include <string.h>

#if FMT__DEFAULT_CUSTOM_ARG
  bool fmt_custom_arg(FmtArg arg, FmtSpec spec,
                      void *userdata,
                      FmtFormatOutput *format_output) {
    (void) arg;
    (void) spec;
    (void) userdata;
    (void) format_output;
    return false;
  }
#endif

static
int show_S64_dec(char *buf, int64_t n) {
  if (n == 0) {
    buf[0] = '0';
    return 1;
  }

  int len = 0;
  for (int64_t m = n; m != 0; m /= 10) len++;

  bool negative = n < 0;
  if (negative) {
    buf[0] = '-';
    len++;
  }

  int index = len - 1;
  for (int64_t m = n; m != 0; m /= 10) {
    char digit = m % 10;
    buf[index] = (negative ? -digit : digit) + '0';
    index--;
  }

  return len;
}

static
int show_U64_dec(char *buf, uint64_t n) {
  if (n == 0) {
    buf[0] = '0';
    return 1;
  }
  int len = 0;
  for (uint64_t m = n; m != 0; m /= 10) len++;

  int index = len - 1;
  for (uint64_t m = n; m != 0; m /= 10) {
    buf[index] = m % 10 + '0';
    index--;
  }

  return len;
}

static
int show_U64_hex(char *buf, uint64_t n) {
  if (n == 0) {
    buf[0] = '0';
    return 1;
  }
  int len = 0;
  for (uint64_t m = n; m != 0; m /= 16) len++;
  int index = len - 1;
  for (uint64_t m = n; m != 0; m /= 16) {
    buf[index] = "0123456789abcdef"[m % 16];
    index--;
  }

  return len;
}

static
int show_U64_bin(char *buf, uint64_t n) {
  if (n == 0) {
    buf[0] = '0';
    return 1;
  }
  int len = 0;
  for (uint64_t m = n; m != 0; m >>= 1) len++;

  int index = len - 1;
  for (uint64_t m = n; m != 0; m >>= 1) {
    buf[index] = '0' + (m & 1);
    index--;
  }
  return len;
}


void fmt_init(FmtState *state, const char *fmt, va_list va) {
  int arg_count;
  for (arg_count = 0; arg_count < FMT_MAX_ARGS; arg_count++) {
    state->args[arg_count] = va_arg(va, FmtArg);
    if (!state->args[arg_count].data) break;
  }

  state->arg_count = arg_count;
  state->fmt_at_init = fmt;
  state->fmt = fmt;

  state->userdata = 0;

  state->size = 0;

  state->next_arg_ix = 0;
  state->action = FmtActionParsing;
}

void fmt_reset(FmtState *state) {
  state->next_arg_ix = 0;
  state->fmt = state->fmt_at_init;
  state->action = FmtActionParsing;
}

static inline
bool fmt_parse_argspec(const char **fmt_ptr,
                       int *out_arg_ix,
                       FmtSpec *out_spec) {
  // At entry, fmt points to the initial '{'.
  const char *fmt = *fmt_ptr + 1;
  bool done = false;
  bool invalid = false;
  int arg_ix = -1;

  FmtSpec spec = {.pad_byte = ' ', .pad_mode = FmtPadLeft};

  if (!done) {
    switch (*fmt) {
      case '{': assert(0); break; // Unreachable.
      case '0': case '1': case '2':
      case '3': case '4': case '5':
      case '6': case '7': case '8':
        arg_ix = *fmt - '0';
        fmt++;
        break;
      case '}': done = true; fmt++; break;
      case ':': case '|': break; // '!'
      default: done = true; invalid = true; break;
    }
  }

  if (!done) {
    switch (*fmt) {
    case ':': {
      fmt++;
      if (*fmt == '-') {
        spec.pad_mode = FmtPadRight;
        fmt++;
      }
      if (*fmt == '0') {
        // Padding with zeros from the right is probably a mistake, but this
        // code will do it anyway.
        spec.pad_byte = '0';
        fmt++;
      }
      if (isdigit(*fmt)) {
        spec.min_len = *fmt - '0';
        fmt++;
        if (isdigit(*fmt)) {
          spec.min_len = 10 * spec.min_len + *fmt - '0';
          fmt++;
        }
      }
      if (*fmt == 'x' || *fmt == 'b' || *fmt == 'c' || *fmt == 'p') {
        spec.format = *fmt;
        fmt++;
      }
      break;
    }
    case '}': done = true; fmt++; break;
    case '|': break;
    default: done = true; invalid = true; break;
    }
  }

  if (!done) {
    switch (*fmt) {
    case '|':
      fmt++;
      spec.custom_start = fmt;
      while (*fmt && *fmt != '}') {
        fmt++;
        spec.custom_len++;
      }
      break;
    case '}': done = true; fmt++; break;
    default: done = true; invalid = true; break;
    }
  }

  if (!done) {
    switch (*fmt) {
    case '}': done = true; fmt++; break;
    default: done = true; invalid = true; break;
    }
  }

  assert(done);

  *out_arg_ix = arg_ix;
  *out_spec = spec;
  *fmt_ptr = fmt;
  return !invalid;
}

static inline
void fmt_format_arg(FmtArg arg, FmtSpec spec,
                    void *userdata, FmtFormatOutput *format_output) {
  // Initially, format_output->text is pointing to text_buf, which is
  // of length FMT_SHOW_BUF_MAX.

  // TODO: If there's enough space (FMT_SHOW_BUF_MAX) in the buffer we're
  // writing to, we can skip text_buf completely, even for custom formatters.

  if (spec.format == 'p' || arg.type == FmtArgVoidPtr) {
    // Treat the argument as a pointer regardless of the actual type data.
    void *ptr = *(void **)arg.data;
    if (ptr == 0) {
      strcpy(format_output->text, "(nil)");
      format_output->text_size = strlen(format_output->text);
      if (format_output->pad_byte == '0') {
        // Pad "(nil)" with spaces.
        format_output->pad_byte = ' ';
      }
    } else {
      format_output->text[0] = '0';
      format_output->text[1] = 'x';
      format_output->text_size = 2 +
        show_U64_hex(format_output->text+2, (uint64_t)ptr);
      if (format_output->pad_byte == '0') {
        format_output->pad_pos = 2; // Pad to the right of the 0x.
        format_output->pad_mode = FmtPadCustomPos;
      }
    }
  } else {
    #define FMT__CHAR_IS_SIGNED ((char)-1 < 0)
    switch (arg.type) {
    case FmtArgS64: case FmtArgS32: case FmtArgS16: case FmtArgS8:
    // Signed char.
    case (FMT__CHAR_IS_SIGNED ? FmtArgChar : FmtArgFirstBuiltin): {
      int64_t val = arg.type == FmtArgS64  ? *(int64_t *)arg.data :
                    arg.type == FmtArgS32  ? *(int32_t *)arg.data :
                    arg.type == FmtArgS16  ? *(int16_t *)arg.data :
                    arg.type == FmtArgS8   ? *(int8_t  *)arg.data :
                                             *(char    *)arg.data;
      if (spec.format == 0) {
        format_output->text_size = show_S64_dec(format_output->text, val);
      } else if (spec.format == 'x') {
        format_output->text_size = show_U64_hex(format_output->text, val);
      } else if (spec.format == 'b') {
        format_output->text_size = show_U64_bin(format_output->text, val);
      } else if (spec.format == 'c') {
        *format_output->text = (char)val;
        format_output->text_size = 1;
      } else assert(0);
      break;
    }
    case FmtArgU64: case FmtArgU32: case FmtArgU16: case FmtArgU8:
    // Unsigned char.
    case (FMT__CHAR_IS_SIGNED ? FmtArgFirstBuiltin : FmtArgChar): {
      uint64_t val = arg.type == FmtArgU64 ? *(uint64_t *)arg.data :
                     arg.type == FmtArgU32 ? *(uint32_t *)arg.data :
                     arg.type == FmtArgU16 ? *(uint16_t *)arg.data :
                     arg.type == FmtArgU8  ? *(uint8_t  *)arg.data :
                                             *(char     *)arg.data;
      if (spec.format == 0) {
        format_output->text_size = show_U64_dec(format_output->text, val);
      } else if (spec.format == 'x') {
        format_output->text_size = show_U64_hex(format_output->text, val);
      } else if (spec.format == 'b') {
        format_output->text_size = show_U64_bin(format_output->text, val);
      } else if (spec.format == 'c') {
        *format_output->text = (char)val;
        format_output->text_size = 1;
      } else assert(0);
      break;
    }
    #undef FMT__CHAR_IS_SIGNED
    case FmtArgBool: {
      bool val = *(bool *)arg.data;
      strcpy(format_output->text, val ? "true" : "false");
      format_output->text_size = strlen(format_output->text);
      break;
    }
    case FmtArgCharPtr: {
      // Use string directly.
      char *str = *(char **)arg.data;
      format_output->text_size = strlen(str);
      format_output->text = str;
      break;
    }
    default: {
      bool known = fmt_custom_arg(arg, spec, userdata, format_output);
      if (!known) {
        strcpy(format_output->text, "{unknown type}");
        format_output->text_size = strlen(format_output->text);
      }
      break;
    }
    }
  }

  bool calculate_padding = true;
  switch (format_output->pad_mode) {
  case FmtPadRight:
    format_output->pad_pos = format_output->text_size;
    break;
  case FmtPadLeft:
    assert(format_output->pad_pos == 0);
    break;
  case FmtPadCustomPos:
    break;
  case FmtPadManual:
    calculate_padding = false;
    break;
  }
  if (calculate_padding) {
    assert(format_output->pad_size == 0);
    if (format_output->text_size < spec.min_len) {
      format_output->pad_size = spec.min_len - format_output->text_size;
    }
  }
}

bool fmt_chunk(FmtState *state, char *buf, size_t buf_size) {
  if (state->action == FmtActionDone) {
    state->size = 0;
    return false;
  }

  char *cur = buf;
  char *end = buf ? buf + buf_size : 0;
  size_t size_written = 0;

  while (true) {
    switch (state->action) {
    case FmtActionParsing:
      if (*state->fmt == '\0') {
        state->action = FmtActionDone;
        break;
      }
      if (*state->fmt != '{' ||
          (state->fmt[0] == '{' && state->fmt[1] == '{')) {
        // Just output a character.
        if (cur) {
          if (cur < end) {
            *cur = *state->fmt;
            cur++;
          } else {
            // No room!
            goto exit_loop;
          }
        }
        size_written++;
        if (*state->fmt == '{') state->fmt++;
        state->fmt++;
      } else {
        // Parse arg.
        int requested_arg_ix;
        FmtSpec spec;
        bool success = fmt_parse_argspec(&state->fmt,
                                         &requested_arg_ix,
                                         &spec);

        const char *error = 0;

        state->format_output.text = state->text_buf;
        state->format_output.text_size = 0;
        state->format_output.pad_pos = 0;
        state->format_output.pad_size = 0;
        state->format_output.pad_byte = spec.pad_byte;
        state->format_output.pad_mode = spec.pad_mode;

        if (success) {
          int actual_arg_ix = requested_arg_ix;
          if (actual_arg_ix == -1) actual_arg_ix = state->next_arg_ix++;
          if (actual_arg_ix < state->arg_count) {
            fmt_format_arg(state->args[actual_arg_ix], spec,
                           state->userdata, &state->format_output);
          } else {
            error = "{invalid arg index}";
          }
        } else {
          // Can't parse argspec.
          error = "{invalid fmt}";
        }

        if (error) {
          strcpy(state->format_output.text, error);
          state->format_output.text_size = strlen(state->format_output.text);
        }
        state->action = FmtActionFormatting;
      }
      break;
    case FmtActionFormatting: {
      if (cur) {
        size_t remaining = end - cur;
        // Padding might be inserted in the middle of the text. So there are
        // three pieces: text before pad_pos, padding, text after pad_pos
        assert(state->format_output.pad_pos <= state->format_output.text_size);
        // This code is kind of a mess.

        // Text before padding:
        if (state->format_output.pad_pos > 0 && remaining > 0) {
          size_t actual_size = state->format_output.pad_pos;
          if (actual_size > remaining) actual_size = remaining;
          memcpy(cur, state->format_output.text, actual_size);
          cur += actual_size;
          remaining -= actual_size;
          state->format_output.text += actual_size;
          state->format_output.text_size -= actual_size;
          state->format_output.pad_pos -= actual_size;
          size_written += actual_size;
        }
        // Padding:
        if (state->format_output.pad_size > 0 && remaining > 0) {
          assert(state->format_output.pad_pos == 0);
          size_t actual_size = state->format_output.pad_size;
          if (actual_size > remaining) actual_size = remaining;
          memset(cur, state->format_output.pad_byte, actual_size);
          cur += actual_size;
          state->format_output.pad_size -= actual_size;
          remaining -= actual_size;
          size_written += actual_size;
        }
        // Text after padding:
        if (state->format_output.text_size > 0 && remaining > 0) {
          assert(state->format_output.pad_size == 0);
          size_t actual_size = state->format_output.text_size;
          if (actual_size > remaining) actual_size = remaining;
          memcpy(cur, state->format_output.text, actual_size);
          cur += actual_size;
          remaining -= actual_size;
          state->format_output.text += actual_size;
          state->format_output.text_size -= actual_size;
          size_written += actual_size;
        }
      } else {
        // Just counting.
        size_written += state->format_output.pad_size + state->format_output.text_size;
        state->format_output.pad_size = 0;
        state->format_output.text_size = 0;
      }
      if (state->format_output.pad_size == 0 &&
          state->format_output.text_size == 0) {
        state->action = FmtActionParsing;
      } else {
        goto exit_loop;
      }
      break;
    }
    case FmtActionDone:
      goto exit_loop;
      break;
    }
  }
  exit_loop:

  state->size = size_written;

  if (size_written == 0 && state->action == FmtActionDone) {
    // We can return false right away if we haven't produced any output and won't
    // produce any more in future calls.
    return false;
  }

  return true;
}

int fmt_sn_va(char *buf, size_t size, const char *fmt, ...) {
  FmtState state;
  size_t size_excluding_nul = size ? size - 1 : 0;
  int result_size = 0;
  va_list va;

  va_start(va, fmt);
  fmt_init(&state, fmt, va);
  va_end(va);

  // Format as much as we can and terminate the string.
  if (fmt_chunk(&state, buf, size_excluding_nul)) {
    result_size += state.size;
  }
  if (buf && size > 0) {
    buf[result_size] = '\0';
  }

  // If we're not done, count how much more space we would have needed.
  if (fmt_chunk(&state, 0, 0)) {
    result_size += state.size;
  }

  return result_size;
}

int fmt_fprint_va(FILE *file, const char *fmt, ...) {
  FmtState state;
  va_list va;

  int total_written_size = 0;

  va_start(va, fmt);
  fmt_init(&state, fmt, va);
  va_end(va);

  char buf[4096];
  while (fmt_chunk(&state, buf, sizeof buf)) {
    size_t written_size = fwrite(buf, 1, state.size, file);
    if (written_size < state.size) {
      return -1;
    }
    total_written_size += written_size;
  }

  return total_written_size;
}

#endif // FMT_IMPL
