#pragma once

#include <ctime>

#define FMT_ESC = "\033["
#define FMT_END = "m"

#define FMT_RESET "\033[0m"
#define FMT_RED "\033[31m"
#define FMT_GREEN "\033[32m"
#define FMT_YELLOW "\033[33m"
#define FMT_BLUE "\033[34m"
#define FMT_GRAY "\033[90m"

class Logger {
  public:
	void set(const char *fmt) {
		printf("%s", fmt);
	}
	void reset() {
		printf(FMT_RESET);
	}
	void color(const char *fmt, const char *msg) {
		printf("%s%s%s", fmt, msg, FMT_RESET);
	}

	void print_pad(int val) {
		if (val < 10) printf("0");
		printf("%i", val);
	}

	void str_pad(char *str, int val) {
		if (val < 10) {
			strcat(str, "0");
		}
		char buf[4] = "";
		sprintf(buf, "%i", val);
		strcat(str, buf);
	}

	void date_string(char *str) {
		time_t t = time(NULL);
		struct tm datetime = *localtime(&t);

		str_pad(str, datetime.tm_year + 1900);
		strcat(str, "-");
		str_pad(str, datetime.tm_mon + 1);
		strcat(str, "-");
		str_pad(str, datetime.tm_mday);
	}

	void time_string(char *str) {
		time_t t = time(NULL);
		struct tm datetime = *localtime(&t);

		str_pad(str, datetime.tm_hour);
		strcat(str, ":");
		str_pad(str, datetime.tm_min);
		strcat(str, ":");
		str_pad(str, datetime.tm_sec);
	}

	void datetime_string(char *str) {
		char time_buf[64] = "";
		char date_buf[64] = "";
		date_string(date_buf);
		time_string(time_buf);

		sprintf(str, "%s %s", date_buf, time_buf);
	}

	void timestamp() {
		char buf[128] = "";
		datetime_string(buf);

		set(FMT_GRAY);
		printf("[%s] ", buf);
		reset();
	}

	void err(const char *msg, ...) {
		va_list args;
		va_start(args, msg);
		timestamp();
		color(FMT_RED, "ERROR:   ");
		vprintf(msg, args);
		va_end(args);

		printf("\n");
	}

	void note(const char *msg, ...) {
		va_list args;
		va_start(args, msg);
		timestamp();
		color(FMT_BLUE, "NOTE:    ");
		vprintf(msg, args);
		va_end(args);

		printf("\n");
	}

	void warn(const char *msg, ...) {
		va_list args;
		va_start(args, msg);
		timestamp();
		color(FMT_YELLOW, "WARNING: ");
		vprintf(msg, args);
		va_end(args);

		printf("\n");
	}

	void net(const char *msg, ...) {
		va_list args;
		va_start(args, msg);
		timestamp();
		color(FMT_GREEN, "NETWORK: ");
		vprintf(msg, args);
		va_end(args);

		printf("\n");
	}
};