#include <stdio.h>
#include <stdarg.h>

enum arg_type {
	arg_type_shrug,
	arg_type_int,
	arg_type_double,
	arg_type_ptr_char,
    arg_type_ptr_void
};

typedef struct arg_ {
	enum arg_type type;
	void* data;
} arg;

#define GET_ARG_TYPE(value) _Generic(value, \
	int: arg_type_int, \
	double: arg_type_double, \
	char*: arg_type_ptr_char, \
	const char*: arg_type_ptr_char, \
	void*: arg_type_ptr_void, \
	/* and so on, and so forth... */ \
	default: arg_type_shrug \
)

#define GET_ARG_DATA(value) \
	((__typeof((void)0, (value))[]){ (value) })

#define GET_ARG(value) \
	(arg){ GET_ARG_TYPE(value), GET_ARG_DATA(value) }

void process_arg(arg arg0);

void process_all_args(int marker_argument, ...);

#define PROCESS_ARG_LAST() (arg){ arg_type_shrug, NULL }

#define PROCESS_ARG0()         PROCESS_ARG_LAST() // and we're done!!
#define PROCESS_ARG1(val)      GET_ARG(val), PROCESS_ARG0()
#define PROCESS_ARG2(val, ...) GET_ARG(val), PROCESS_ARG1(__VA_ARGS__)
#define PROCESS_ARG3(val, ...) GET_ARG(val), PROCESS_ARG2(__VA_ARGS__)
#define PROCESS_ARG4(val, ...) GET_ARG(val), PROCESS_ARG3(__VA_ARGS__)
#define PROCESS_ARG5(val, ...) GET_ARG(val), PROCESS_ARG4(__VA_ARGS__)
#define PROCESS_ARG6(val, ...) GET_ARG(val), PROCESS_ARG5(__VA_ARGS__)
#define PROCESS_ARG7(val, ...) GET_ARG(val), PROCESS_ARG6(__VA_ARGS__)

#define PROCESS_ARGS_SELECT(inv1, inv2, inv3, inv4, inv5, inv6, inv7, INVOKE_THIS_ONE, ...) \
	INVOKE_THIS_ONE

#define process_args(...) \
	process_all_args( 0xAAAA, PROCESS_ARGS_SELECT(__VA_ARGS__, \
        PROCESS_ARG7, PROCESS_ARG6, PROCESS_ARG5, \
		PROCESS_ARG4, PROCESS_ARG3, PROCESS_ARG2, \
		PROCESS_ARG1, PROCESS_ARG0)(__VA_ARGS__))

int main () {
	process_args( NULL, "\n", 1, "\n2\n", 3.0 );
	return 0;
}

/* implementation of arg processing! */

void process_all_args(int marker_argument, ...) {
	va_list all_args;
	va_start(all_args, marker_argument);
	while (1) {
		arg current_arg = va_arg(all_args, arg);
		if (current_arg.data == NULL
		   && current_arg.type == arg_type_shrug) {
		   // exit!
		   break;
		}
		process_arg(current_arg);
	}
	va_end(all_args);
}

void process_arg(arg arg0) {
	switch (arg0.type) {
		case arg_type_int:
			{
				// points at a single integer
				int value = *(int*)arg0.data;
				printf("%d", value);
			}
			break;
		case arg_type_double:
			{
				// points at a single double
				double value = *(double*)arg0.data;
				printf("%f", value);
			}
			break;
		case arg_type_ptr_char:
			{
				// points at a character string
				char* value = *(char**)arg0.data;
				printf("%s", value);
			}
			break;
		case arg_type_ptr_void:
			{
				// points at a character string
				void* value = *(void**)arg0.data;
				printf("%p", value);
			}
			break;
		/* and so on, and so forth... */
		default:
			{
				void* value = arg0.data;
				printf("(unknown type!) %p", value);
			}
			break;
	}
}
