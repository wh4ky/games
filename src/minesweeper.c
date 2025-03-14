#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define FPS 10

static inline void winclear(void);

struct map {
  int rows;
  int cols;
  bool **map;
} map;

struct cursor {
  int x;
  int y;
  char *color;
} cursor;

struct winsize win;

int main(void) {
  srand((unsigned int)time(NULL)); // Seed the random number generator.
  char out_buffer[BUFSIZ];         // Stack-based buffer
  setvbuf(stdout, out_buffer, _IOFBF,
          BUFSIZ); // Set full buffering and ensure stack-based buffer.
  char input[5];

  cursor.color = "\x1b[38;5;7m\x1b[48;5;0m\x1b[0";
  cursor.x = 0;
  cursor.y = 0;

  // Init timespecs.
  struct timespec start, end, sleep;
  int sleepfor = (int)1e9 / FPS;

  // Initialize map struct.
  map.rows = 10;
  map.cols = 20;
  map.map = calloc((unsigned long)map.rows, sizeof(bool *));
  for (int i = 0; i < map.rows; i++) {
    map.map[i] = calloc((unsigned long)map.cols, sizeof(bool));
  }

  // Initialize all cells in the map.
  for (int i = 0; i < 20; i++) {
    int r = rand() % map.rows;
    int c = rand() % map.cols;
    map.map[r][c] = !map.map[r][c];
  }

  // Disable echo and canonical mode.
  struct termios default_t, new_t;
  tcgetattr(STDIN_FILENO, &default_t);
  new_t = default_t;
  new_t.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

  // Main game loop.
  while (1) {
    winclear();

    // Start clock (so you can have a smooth frame speed).
    clock_gettime(CLOCK_MONOTONIC, &start);

    printf("\x1b[0;0H");
    for (int i = 0; i < map.rows; i++) {
      for (int j = 0; j < map.cols; j++) {
        printf("%d", map.map[i][j] ? 1 : 0);
      }
      printf("\n");
    }
    printf("\n");

    printf("%s\x1b[%d;%dH", cursor.color, cursor.y, cursor.x);
    fflush(stdout);

    memset(&input, '\0', sizeof(input));          // 'clear' the input buffer.
    read(STDIN_FILENO, input, sizeof(input) - 1); // Get input (POSIX).

    if (input[0] == '\x1b') {
      switch (input[2]) {
      case 'A':
        cursor.y--;
        break;
      case 'B':
        cursor.y++;
        break;
      case 'C':
        cursor.x++;
        break;
      case 'D':
        cursor.x--;
        break;
      }
      if (strcpy(input, "\x1b")) {
        goto exit;
      }
    } else {
      switch (input[0]) {
      case '\n':
        break;
      }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    int remaining_time =
        sleepfor - (int)((end.tv_sec - start.tv_sec) * (long)1e9 +
                         (end.tv_nsec - start.tv_nsec));
    if (remaining_time > 0) {
      sleep.tv_sec = remaining_time / (long)1e9;  // Convert to seconds
      sleep.tv_nsec = remaining_time % (long)1e9; // Remaining nanoseconds
      nanosleep(&sleep, NULL);
    }
  }

exit:

  for (int i = 0; i < map.rows; i++) {
    free(map.map[i]);
  }
  free(map.map);

  tcsetattr(STDIN_FILENO, TCSANOW, &default_t);
  return EXIT_SUCCESS;
}

static inline void winclear(void) {
  fprintf(stdout, "\x1b[2J");
  return;
}
