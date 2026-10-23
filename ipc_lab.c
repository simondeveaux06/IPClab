// ipc_lab.c  Operating Systems Assignment 2: Interprocess communication with pipes
//
// Build: copy into xv6-riscv/user/, add $U/_ipc_lab\ to UPROGS in the Makefile,
//        then run make qemu.
// Usage: ipc_lab message | pingpong
//
// Complete the six TODO blocks (TODOs 1-3: Member 1, TODOs 4-6: Member 2).
// Until a TODO is done, its mode prints a "TODO ... not done yet" line.
//
// Replace the identification line below with both members' names and student IDs.
static char *GROUP = "Member 1: NAME (ID)   Member 2: NAME (ID)";

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Newer xv6-riscv versions renamed sleep() to pause().
// If the build reports that sleep is undeclared, change sleep to pause here.
#define NAP(ticks) sleep(ticks)

// ---- Experiment settings (Parts A and B change these) -----------------------

// Part A: one-way messages
#define MSG_COUNT     4     // messages the parent sends
#define SEND_GAP      10    // ticks the parent waits between messages

// Part B: two-way request and reply
#define ROUNDS        2000  // request/reply round trips
#define SHOW_ROUNDS   3     // how many rounds to print in detail
#define PARENT_READS_FIRST 0 // 0 = parent writes then reads; 1 = parent reads then writes

// ---- Line-at-a-time output ------------------------------------------------
// xv6 printf writes one character per system call, so output from several
// processes can mix in the middle of a line. These helpers build a whole line
// first and send it with a single write() to fd 2 (the console).

static char lb[200];
static int ln;

// A one-byte pipe used as a lock, so that only one process writes a line to
// the console at a time. main() puts a single token byte in it. A process
// takes the token with read() (blocking if another process holds it), prints
// its line, then puts the token back with write(). This is itself a small
// example of interprocess communication: a pipe acting as a mutex.
static int conslock[2] = {-1, -1};

static void
ls(const char *s)
{
  while (*s && ln < (int)sizeof(lb) - 2)
    lb[ln++] = *s++;
}

static void
li(int n)
{
  char t[12];
  int i = 0;
  uint u = n;

  if (n < 0) {
    ls("-");
    u = -n;
  }
  do {
    t[i++] = '0' + u % 10;
    u /= 10;
  } while (u);
  while (i > 0 && ln < (int)sizeof(lb) - 2)
    lb[ln++] = t[--i];
}

static void
tag(const char *who)          // start a line: [t=TICK] who
{
  ln = 0;
  ls("[t=");
  li(uptime());
  ls("] ");
  ls(who);
  ls(" ");
}

static void
lend(void)                    // finish the line and print it
{
  char token;

  lb[ln++] = '\n';
  if (conslock[0] >= 0)
    read(conslock[0], &token, 1);     // take the console lock
  write(2, lb, ln);
  if (conslock[0] >= 0)
    write(conslock[1], &token, 1);    // give it back
  ln = 0;
}

static void
die(const char *msg)
{
  tag("error ");
  ls(msg);
  lend();
  exit(1);
}

// ---- Given helpers for Part A ----------------------------------------------

// Builds the text "message <i>\n" in msg and returns its length in bytes.
__attribute__((unused)) static int
make_msg(char *msg, int i)
{
  char *s = "message ";
  char t[12];
  int len = 0, k = 0;

  while (*s)
    msg[len++] = *s++;
  do {
    t[k++] = '0' + i % 10;
    i /= 10;
  } while (i);
  while (k > 0)
    msg[len++] = t[--k];
  msg[len++] = '\n';
  return len;
}

// Prints one line for a read() that returned data.
__attribute__((unused)) static void
show_read(int reads, char *buf, int n)
{
  int j;

  buf[n] = 0;
  for (j = 0; j < n; j++)
    if (buf[j] == '\n')
      buf[j] = '|';
  tag("child ");
  ls("read #");
  li(reads);
  ls(" returned ");
  li(n);
  ls(" bytes: ");
  ls(buf);
  lend();
}

// Reads exactly 4 bytes (one int) from fd into *x.
// Returns 1 on success, 0 at end of file.
__attribute__((unused)) static int
readint(int fd, int *x)
{
  // ===== TODO 3 (Member 1): read exactly 4 bytes ============================
  char *p = (char *)x;
  int received = 0;

  while (received < 4) {
    int n = read(fd, p + received, 4 - received);
    if (n <= 0)
      return 0;
    received += n;
  }
  return 1;
  // ===== END TODO 3 ==========================================================
}

// ---- Part A: one-way communication (Member 1) ------------------------------

static void
message(void)
{
  int p[2];
  int pid, i, st, w, closed_at;
  int sent_at[MSG_COUNT];

  if (pipe(p) < 0)
    die("pipe failed");
  tag("parent");
  ls("created pipe: read end fd ");
  li(p[0]);
  ls(", write end fd ");
  li(p[1]);
  lend();
  tag("parent");
  ls("will send ");
  li(MSG_COUNT);
  ls(" messages, ");
  li(SEND_GAP);
  ls(" ticks apart");
  lend();

  pid = fork();
  if (pid < 0)
    die("fork failed");

  if (pid == 0) {
    int reads = 0, total = 0;

    // ===== TODO 2 (Member 1): the child receives until end of file =========
    char buf[101];
    int n;

    close(p[1]);
    while ((n = read(p[0], buf, sizeof(buf) - 1)) > 0) {
      reads++;
      total += n;
      show_read(reads, buf, n);
    }
    // ===== END TODO 2 ========================================================
    tag("child ");
    ls("read returned 0 (end of file) after ");
    li(reads);
    ls(" reads and ");
    li(total);
    ls(" bytes");
    lend();
    close(p[0]);
    exit(reads);   // exit status = number of read() calls that returned data
  }

  // The parent stays silent while the child runs. It records when each
  // message was sent and reports afterwards.
  // ===== TODO 1 (Member 1): the parent sends the messages ====================
  close(p[0]);
  for (i = 1; i <= MSG_COUNT; i++) {
    char msg[32];
    int len = make_msg(msg, i);

    sent_at[i - 1] = uptime();
    write(p[1], msg, len);
    if (SEND_GAP > 0)
      NAP(SEND_GAP);
  }
  close(p[1]);
  closed_at = uptime();
  // ===== END TODO 1 ==========================================================

  w = wait(&st);
  for (i = 1; i <= MSG_COUNT; i++) {
    tag("parent");
    ls("had sent message ");
    li(i);
    ls(" at t=");
    li(sent_at[i - 1]);
    lend();
  }
  tag("parent");
  ls("had closed the write end at t=");
  li(closed_at);
  lend();
  tag("parent");
  ls("wait returned pid ");
  li(w);
  ls(", child exit status ");
  li(st);
  ls(" (number of reads)");
  lend();
}

// ---- Part B: two-way request and reply (Member 2) --------------------------

static void
pingpong(void)
{
  int down[2], up[2];     // down: parent -> child, up: child -> parent
  int r, x, start, end;

  if (pipe(down) < 0 || pipe(up) < 0)
    die("pipe failed");

  if (fork() == 0) {
    // ===== TODO 4, part 1 (Member 2): ends the child does not use ===========
    close(down[1]);
    close(up[0]);
    // ===== END TODO 4 ========================================================
    // ===== TODO 5 (Member 2): the child's request-and-reply service ========
    for (r = 0; r < ROUNDS; r++) {
      if (readint(down[0], &x) == 0)
        break;
      x = x + 1;
      write(up[1], &x, 4);
    }
    // ===== END TODO 5 ========================================================
    exit(0);
  }

  // ===== TODO 4, part 2 (Member 2): ends the parent does not use =============
  close(down[0]);
  close(up[1]);
  // ===== END TODO 4 ==========================================================
  x = 0;
  start = uptime();
  tag("parent");
  ls("starting ");
  li(ROUNDS);
  ls(" round trips");
  lend();

  for (r = 1; r <= ROUNDS; r++) {
    int sent = x;
    // ===== TODO 6 (Member 2): one request and its reply ====================
    if (PARENT_READS_FIRST == 0) {
      write(down[1], &x, 4);
      readint(up[0], &x);
    } else {
      readint(up[0], &x);
      write(down[1], &x, 4);
    }
    // ===== END TODO 6 ========================================================
    if (r <= SHOW_ROUNDS) {
      tag("parent");
      ls("round ");
      li(r);
      ls(": sent ");
      li(sent);
      ls(", got back ");
      li(x);
      lend();
    }
  }
  end = uptime();
  close(down[1]);
  close(up[0]);
  wait(0);

  tag("parent");
  ls("finished ");
  li(ROUNDS);
  ls(" round trips, final value ");
  li(x);
  ls(", elapsed ticks ");
  li(end - start);
  lend();
}

// ---- main ------------------------------------------------------------------

static void
usage(void)
{
  printf("usage: ipc_lab message | pingpong\n");
  printf("  message   one-way messages through one pipe (Part A)\n");
  printf("  pingpong  two-way request and reply with two pipes (Part B)\n");
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    usage();
    exit(0);
  }

  if (pipe(conslock) == 0)
    write(conslock[1], "L", 1);       // the single lock token
  else
    conslock[0] = conslock[1] = -1;   // no lock: output may mix

  tag("ipc_lab");
  ls(GROUP);
  lend();

  if (strcmp(argv[1], "message") == 0)
    message();
  else if (strcmp(argv[1], "pingpong") == 0)
    pingpong();
  else
    usage();

  exit(0);
}
