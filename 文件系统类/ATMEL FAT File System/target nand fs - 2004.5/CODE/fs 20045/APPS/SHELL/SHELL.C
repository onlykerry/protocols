/***********************************************************************/
/*                                                                     */
/*   Module:  shell.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2004.3                                                   */
/*   Purpose: Implement the shell application                          */
/*                                                                     */
/***********************************************************************/
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#include "shellp.h"
#include "../../posix.h"
#include "../../ffs_stdio.h"

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define DISPLAY_WIDTH  80
#define MAX_ARG        51
#define MAX_LINE       301
#define PAGE_SIZE      512
#define TRUE           1
#define FALSE          0

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
typedef struct
{
  char *cmd;
  int  (*func)(char *cmd_line);
  char *help;
} ShellEntry;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
/*
** Shell Functions
*/
static int sh_append(char *cmd_line);
static int sh_cd(char *cmd_line);
static int sh_cp(char *cmd_line);
static int sh_create(char *cmd_line);
static int sh_dir(char *cmd_line);
static int sh_display(char *cmd_line);
static int sh_format(char *cmd_line);
static int sh_fstat(char *cmd_line);
static int sh_help(char *cmd_line);
static int sh_link(char *cmd_line);
static int sh_ls(char *cmd_line);
static int sh_mkdir(char *cmd_line);
static int sh_more(char *cmd_line);
static int sh_mount(char *cmd_line);
static int sh_mount_all(char *cmd_line);
static int sh_mv(char *cmd_line);
static int sh_pwd(char *cmd_line);
static int sh_rm(char *cmd_line);
static int sh_rmdir(char *cmd_line);
static int sh_stat(char *cmd_line);
static int sh_sortdir(char *cmd_line);
static int sh_time(char *cmd_line);
static int sh_trunc(char *cmd_line);
static int sh_unformat(char *cmd_line);
static int sh_unmount(char *cmd_line);
static int sh_vstat(char *cmd_line);

/*
** Auxiliary Functions
*/
static int get_arg(char *cmd_line, char *arg);

/***********************************************************************/
/* Global Variable Definitions                                         */
/***********************************************************************/
static ShellEntry ShellFuncs[] =
{
/* command name     function */
/* help string */
  "append",        sh_append,
  "append [file] [text] - Appends text to a file\n",
  "cd",            sh_cd,
  "cd [directory] - Change current working directory\n",
  "cp",            sh_cp,
  "cp [file1] [file2] - Copy contents of file1 into file2\n",
  "create",        sh_create,
  "create [file] [text] - Creates a file containing text\n",
  "dir",           sh_dir,
  "dir [directory] - Lists contents of directory\n",
  "display",       sh_display,
  "display [file] - Displays contents of file (no splits)\n",
  "format",        sh_format,
  "format [device] - Formats device (volume)\n",
  "fstat",         sh_fstat,
  "fstat [file] - Gets stats about an open file\n",
  "help",          sh_help,
  "help - Displays a list of all availabel shell commnads\n",
  "link",          sh_link,
  "link [file1] [file2] - Links file2 to file1\n",
  "ls",            sh_ls,
  "ls [directory] - Lists contents of directory\n",
  "mkdir",         sh_mkdir,
  "mkdir [directory] - Creates new directory\n",
  "more",          sh_more,
  "more [file] - Displays contents of file\n",
  "mount",         sh_mount,
  "mount [device] - Mounts device (volume)\n",
  "mount_all",     sh_mount_all,
  "mount_all - Mounts all existing unmounted devices\n",
  "mv",            sh_mv,
  "mv [loc1] [loc2] - Moves file/directory from loc1 to loc2\n",
  "pwd",           sh_pwd,
  "pwd - Displays the current working directory\n",
  "rm",            sh_rm,
  "rm [file] - Deletes file\n",
  "rmdir",         sh_rmdir,
  "rmdir [directory] - Deletes directory, must be empty\n",
  "stat",          sh_stat,
  "stat [file] - Displays statistics of file\n",
  "sortdir",       sh_sortdir,
  "sortdir [directory] [sort] - Sort a directory\n"
  "\t[directory] = directory_path\n"
  "\t[sort] = 1 by name, 2 by size, 3 by fileno, 4 by mod time\n",
  "time",          sh_time,
  "time [cmd] - Time any valid shell command\n",
  "trunc",         sh_trunc,
  "trunc [file] [size] - Truncate file to size bytes\n",
  "unformat",      sh_unformat,
  "unformat [device] - Unformats device\n",
  "unmount",       sh_unmount,
  "unmount [device] - Unmounts device\n",
  "vstat",         sh_vstat,
  "vstat [device] - Get statistics about a device (volume)\n",
  NULL,            NULL,
  NULL,  /* ends w/ name, func, and help == NULL */
};

/*
** Sort option for sortdir()
*/
static int SortDir = 1;

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/* process_cmd: Execute shell command                                  */
/*                                                                     */
/*     Returns: -1 upon exit request or I/O error. Otherwise, 0        */
/*                                                                     */
/***********************************************************************/
static int process_cmd(char *cmd_line)
{
  char cmd[MAX_ARG];
  int i, curr;

  /*-------------------------------------------------------------------*/
  /* Get the command from the command line.                            */
  /*-------------------------------------------------------------------*/
  curr = get_arg(cmd_line, cmd);

  /*-------------------------------------------------------------------*/
  /* If a command was entered, execute it.                             */
  /*-------------------------------------------------------------------*/
  if (curr)
  {
    /*-----------------------------------------------------------------*/
    /* If it's the 'quit', 'logout', or 'exit' command, stop.          */
    /*-----------------------------------------------------------------*/
    if (!strcmp(cmd, "quit") || !strcmp(cmd, "logout") ||
        !strcmp(cmd, "exit"))
      return -1;

    /*-----------------------------------------------------------------*/
    /* Go through the list of commands and try to find it.             */
    /*-----------------------------------------------------------------*/
    for (i = 0; ShellFuncs[i].func; ++i)
    {
      /*---------------------------------------------------------------*/
      /* If the function is found stop.                                */
      /*---------------------------------------------------------------*/
      if (!strcmp(ShellFuncs[i].cmd, cmd))
        break;
    }

    /*-----------------------------------------------------------------*/
    /* If the function was found execute it, else output not found.    */
    /*-----------------------------------------------------------------*/
    if (ShellFuncs[i].func)
    {
      /*---------------------------------------------------------------*/
      /* Clear errno before calling function to test for errors.       */
      /*---------------------------------------------------------------*/
      errno = 0;

      /*---------------------------------------------------------------*/
      /* If the function fails, but not because of an I/O error        */
      /* output error message.                                         */
      /*---------------------------------------------------------------*/
      if (ShellFuncs[i].func(&cmd_line[curr]))
      {
        if (errno == EIO)
          return -1;
        else if (errno)
          printf("shell: %s\n", strerror(errno));
        else
          printf("syntax error\n");
      }
    }
    else
      printf("shell: %s command not found\n", cmd);
  }

  /*-------------------------------------------------------------------*/
  /* Return 0 to try next command.                                     */
  /*-------------------------------------------------------------------*/
  return 0;
}

/***********************************************************************/
/*     to_int: Convert a decimal string into its value                 */
/*                                                                     */
/*      Input: str = string to be converted                            */
/*                                                                     */
/*    Returns: value of string or -1 on error                          */
/*                                                                     */
/***********************************************************************/
static int to_int(char *str)
{
  int r_value = 0;

  for (; *str != '\0'; ++str)
  {
    /*-----------------------------------------------------------------*/
    /* If the current char is not a digit return -1.                   */
    /*-----------------------------------------------------------------*/
    if (!isdigit(*str))
      return -1;
    else
      r_value = 10 * r_value + (*str - '0');
  }
  return r_value;
}

/***********************************************************************/
/*   get_line: Get a whole command line from the user                  */
/*                                                                     */
/*      Input: cmd_line = place to store the line                      */
/*                                                                     */
/*    Returns: 0 on success, -1 on error                               */
/*                                                                     */
/***********************************************************************/
static int get_line(char *cmd_line)
{
  int i, ch;

  /*-------------------------------------------------------------------*/
  /* Interactive loop to edit command string.                          */
  /*-------------------------------------------------------------------*/
  for (i = 0;;)
  {
    /*-----------------------------------------------------------------*/
    /* Get next key and return -1 if I/O error occurs.                 */
    /*-----------------------------------------------------------------*/
    ch = getchar();
    if (ch == -1)
      return -1;

    /*-----------------------------------------------------------------*/
    /* Process backspace key.                                          */
    /*-----------------------------------------------------------------*/
    if (ch == '\b')
    {
      if (i)
      {
        --i;
        printf("\b \b");
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else if it's new line, output it, NULL terminate, and return.   */
    /*-----------------------------------------------------------------*/
    else if (ch == '\n')
    {
      putchar('\n');
      cmd_line[i] = '\0';
      return 0;
    }

    /*-----------------------------------------------------------------*/
    /* Echo printable characters if line not full, else emit alert.    */
    /*-----------------------------------------------------------------*/
    else
    {
      if (i < (MAX_LINE - 1))
      {
        cmd_line[i++] = ch;
        putchar(ch);
      }
      else
        putchar('\a');
    }
  }
}

/***********************************************************************/
/*     get_arg: Get an argument from a command line                    */
/*                                                                     */
/*      Inputs: cmd_line = command line from which arg is taken        */
/*              arg = pointer to string to store arg                   */
/*                                                                     */
/*     Returns: Position in command line where we stopped              */
/*                                                                     */
/***********************************************************************/
static int get_arg(char *cmd_line, char *arg)
{
  char *start = cmd_line;

  /*-------------------------------------------------------------------*/
  /* Skip the leading spaces.                                          */
  /*-------------------------------------------------------------------*/
  while (isspace(*cmd_line))
    ++cmd_line;

  /*-------------------------------------------------------------------*/
  /* Store at most MAX_ARG chars into arg.                             */
  /*-------------------------------------------------------------------*/
  while ((cmd_line - start) < MAX_ARG - 1)
  {
    /*-----------------------------------------------------------------*/
    /* If we've reached another space or the end of line, stop.        */
    /*-----------------------------------------------------------------*/
    if (isspace(*cmd_line) || (*cmd_line == '\0'))
      break;

    /*-----------------------------------------------------------------*/
    /* Copy current char from command line into arg.                   */
    /*-----------------------------------------------------------------*/
    *arg++ = *cmd_line++;
  }

  /*-------------------------------------------------------------------*/
  /* Skip trailing spaces if any.                                      */
  /*-------------------------------------------------------------------*/
  while (isspace(*cmd_line))
    ++cmd_line;

  /*-------------------------------------------------------------------*/
  /* Terminate the arg string with '\0' and return location in line.   */
  /*-------------------------------------------------------------------*/
  *arg = '\0';
  return cmd_line - start;
}

/***********************************************************************/
/*   sh_append: Append a string to a file                              */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_append(char *cmd_line)
{
  int curr, num_bytes, r_value = 0, fid;
  char filename[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get the name of the file.                                         */
  /*-------------------------------------------------------------------*/
  curr = get_arg(cmd_line, filename);

  /*-------------------------------------------------------------------*/
  /* If no filename entered, return error.                             */
  /*-------------------------------------------------------------------*/
  if (!curr)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Open file in append mode.                                         */
  /*-------------------------------------------------------------------*/
  fid = open(filename, O_RDWR | O_APPEND);

  /*-------------------------------------------------------------------*/
  /* If open failed, return error.                                     */
  /*-------------------------------------------------------------------*/
  if (fid == -1)
    return -1;

  /*-------------------------------------------------------------------*/
  /* If writing string to file fails, return error.                    */
  /*-------------------------------------------------------------------*/
  num_bytes = strlen(&cmd_line[curr]);
  if (write(fid, &cmd_line[curr], num_bytes) != num_bytes)
    r_value = -1;

  /*-------------------------------------------------------------------*/
  /* If closing the file fails, return error.                          */
  /*-------------------------------------------------------------------*/
  if (close(fid))
    r_value = -1;

  return r_value;
}

/***********************************************************************/
/*       sh_cd: Change working directory for the shell                 */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_cd(char *cmd_line)
{
  char dirname[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* If no dirname entered, return error.                              */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, dirname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Change directory.                                                 */
  /*-------------------------------------------------------------------*/
  return chdir(dirname);
}

/***********************************************************************/
/*       sh_cp: Copy a file into another file                          */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_cp(char *cmd_line)
{
  char orig_file[MAX_ARG], new_file[MAX_ARG], buf[300];
  int curr, num_bytes, orig_fid, new_fid, r_value = 0;

  /*-------------------------------------------------------------------*/
  /* Get name of original file.                                        */
  /*-------------------------------------------------------------------*/
  curr = get_arg(cmd_line, orig_file);
  if (!curr)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Get name of new file.                                             */
  /*-------------------------------------------------------------------*/
  if (!get_arg(&cmd_line[curr], new_file))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Open original file as read only.                                  */
  /*-------------------------------------------------------------------*/
  orig_fid = open(orig_file, O_RDONLY);
  if (orig_fid == -1)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Open new file as write-only.                                      */
  /*-------------------------------------------------------------------*/
  new_fid = open(new_file, O_WRONLY | O_CREAT | O_EXCL,
                 S_IRUSR | S_IWUSR | S_IXUSR);
  if (new_fid == -1)
  {
    close(orig_fid);
    return  -1;
  }

  /*-------------------------------------------------------------------*/
  /* Copy original file into new file (300 bytes at a time).           */
  /*-------------------------------------------------------------------*/
  for (num_bytes = 300; num_bytes == 300;)
  {
    /*-----------------------------------------------------------------*/
    /* Read 300 bytes (or less) from original file.                    */
    /*-----------------------------------------------------------------*/
    num_bytes = read(orig_fid, buf, 300);

    /*-----------------------------------------------------------------*/
    /* If error occurred while writing to file, stop.                  */
    /*-----------------------------------------------------------------*/
    if (write(new_fid, buf, num_bytes) != num_bytes)
    {
      r_value = -1;
      break;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Close both files.                                                 */
  /*-------------------------------------------------------------------*/
  if (close(new_fid))
    r_value = -1;
  if (close(orig_fid))
    r_value = -1;

  return r_value;
}

/***********************************************************************/
/*   sh_create: Create a new file and adds a string to it              */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_create(char *cmd_line)
{
  char filename[MAX_ARG];
  int curr, num_bytes, r_value = 0, fid;

  /*-------------------------------------------------------------------*/
  /* Get name of file to be created.                                   */
  /*-------------------------------------------------------------------*/
  curr = get_arg(cmd_line, filename);
  if (!curr)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Open the file for write only.                                     */
  /*-------------------------------------------------------------------*/
  fid = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
  if (fid == -1)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Write the command line string to the file.                        */
  /*-------------------------------------------------------------------*/
  num_bytes = strlen(&cmd_line[curr]);
  if (write(fid, &cmd_line[curr], num_bytes) != num_bytes)
    r_value = -1;

  /*-------------------------------------------------------------------*/
  /* If failed to close file, return error.                            */
  /*-------------------------------------------------------------------*/
  if (close(fid))
    r_value = -1;

  return r_value;
}

/***********************************************************************/
/*      sh_dir: List the contents of a directory                       */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_dir(char *cmd_line)
{
  char dirname[MAX_ARG];
  struct dirent *entry;
  DIR *curr_dir;
  int r_value = 0;

  /*-------------------------------------------------------------------*/
  /* If unable to get the current working directory, return error.     */
  /*-------------------------------------------------------------------*/
  if (!getcwd(dirname, MAX_ARG - 1))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Open the current directory.                                       */
  /*-------------------------------------------------------------------*/
  curr_dir = opendir(dirname);
  if (curr_dir == NULL)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Loop to read every file in the current directory.                 */
  /*-------------------------------------------------------------------*/
  for (;;)
  {
    /*-----------------------------------------------------------------*/
    /* Read an entry at a time from directory until end is reached.    */
    /*-----------------------------------------------------------------*/
    entry = readdir(curr_dir);
    if (entry == NULL)
      break;

    /*-----------------------------------------------------------------*/
    /* Output contents of entry.                                       */
    /*-----------------------------------------------------------------*/
    printf("%s\n", entry->d_name);
  }

  /*-------------------------------------------------------------------*/
  /* If failed to close directory, return error.                       */
  /*-------------------------------------------------------------------*/
  if (closedir(curr_dir))
    return -1;

  return r_value;
}

/***********************************************************************/
/*  sh_display: Display the contents of a file                         */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_display(char *cmd_line)
{
  char filename[MAX_ARG], output[DISPLAY_WIDTH], c;
  int i, j, count = 0, s_out = 0, e_out = 0, fid, len;

  /*-------------------------------------------------------------------*/
  /* If no file name entered, return error.                            */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, filename))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Open file in read only mode, returning error if unable.           */
  /*-------------------------------------------------------------------*/
  fid = open(filename, O_RDONLY);
  if (fid == -1)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Loop to display a line at a time.                                 */
  /*-------------------------------------------------------------------*/
  for (;;)
  {
    /*-----------------------------------------------------------------*/
    /* Read into buf a line worth of characters.                       */
    /*-----------------------------------------------------------------*/
    for (i = count; i < DISPLAY_WIDTH; ++i)
    {
      /*---------------------------------------------------------------*/
      /* Read character at a time, breaking if unable.                 */
      /*---------------------------------------------------------------*/
      len = read(fid, &c, 1);
      if (len != 1)
        break;

      /*---------------------------------------------------------------*/
      /* Remember the last ' ' encountered.                            */
      /*---------------------------------------------------------------*/
      if (c == ' ')
        e_out = s_out + i;

      /*---------------------------------------------------------------*/
      /* Store the character read, c, in the output buffer.            */
      /*---------------------------------------------------------------*/
      output[(s_out + i) % DISPLAY_WIDTH] = c;
    }

    /*-----------------------------------------------------------------*/
    /* If -1, display output buffer and quit.                          */
    /*-----------------------------------------------------------------*/
    if (len != 1)
    {
      for (j = 0; j < i; ++j)
        putchar(output[(s_out + j) % DISPLAY_WIDTH]);
      putchar('\n');
      break;
    }

    /*-----------------------------------------------------------------*/
    /* If there is a space in the output buffer, display from          */
    /* the beginning (s_out) until last space (e_out).                 */
    /*-----------------------------------------------------------------*/
    if (e_out > s_out)
    {
      for (j = s_out; j < e_out; ++j)
        putchar(output[j % DISPLAY_WIDTH]);
      count = DISPLAY_WIDTH - e_out + s_out - 1;
      s_out = e_out + 1;
    }

    /*-----------------------------------------------------------------*/
    /* Else there is no space so display the whole thing.              */
    /*-----------------------------------------------------------------*/
    else
    {
      for (j = 0; j < i; ++j)
        putchar(output[(s_out + j) % DISPLAY_WIDTH]);
      count = 0;
      s_out += i;
    }

    putchar('\n');
  }

  /*-------------------------------------------------------------------*/
  /* close the file.                                                   */
  /*-------------------------------------------------------------------*/
  if (close(fid))
    return -1;

  return 0;
}

/***********************************************************************/
/*   sh_format: Format storage media for the file system               */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_format(char *cmd_line)
{
  char fsname[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get name of volume.                                               */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, fsname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Format and return the result.                                     */
  /*-------------------------------------------------------------------*/
  return format(fsname);
}

/***********************************************************************/
/*    sh_fstat: Show statistics of a file                              */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_fstat(char *cmd_line)
{
  struct stat buf;
  char fname[MAX_ARG];
  int fid;

  /*-------------------------------------------------------------------*/
  /* Get name of file.                                                 */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, fname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Open the file.                                                    */
  /*-------------------------------------------------------------------*/
  fid = open(fname, O_RDONLY);
  if (fid < 0)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Get file's statistics.                                            */
  /*-------------------------------------------------------------------*/
  if (fstat(fid, &buf))
  {
    close(fid);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Close the file.                                                   */
  /*-------------------------------------------------------------------*/
  if (close(fid))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Output some of the stats for file.                                */
  /*-------------------------------------------------------------------*/
  printf("File Access   time: %s", ctime(&buf.st_atime));
  printf("File Modified time: %s", ctime(&buf.st_mtime));
  printf("File Mode: %d\n", buf.st_mode);
  printf("File Serial Number: %d\n", buf.st_ino);
  printf("File Num Links: %d\n", buf.st_nlink);
  printf("File Size (bytes): %u\n", buf.st_size);
  return 0;
}

/***********************************************************************/
/*     sh_help: Display help about shell commands                      */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_help(char *cmd_line)
{
  char command[MAX_ARG];
  int detailed = 0, i;

  /*-------------------------------------------------------------------*/
  /* If extra parameter entered, it's a detailed help.                 */
  /*-------------------------------------------------------------------*/
  if (get_arg(cmd_line, command))
    detailed = 1;

  /*-------------------------------------------------------------------*/
  /* If it's a detailed help look for command for which to display     */
  /* the detailed message, else display all available commands.        */
  /*-------------------------------------------------------------------*/
  if (detailed)
  {
    /*-----------------------------------------------------------------*/
    /* Step through all the available commands looking for right one.  */
    /*-----------------------------------------------------------------*/
    for (i = 0; ShellFuncs[i].func; ++i)
    {
      /*---------------------------------------------------------------*/
      /* If command is found, output its help message.                 */
      /*---------------------------------------------------------------*/
      if (!strcmp(ShellFuncs[i].cmd, command))
      {
        printf("%s", ShellFuncs[i].help);
        break;
      }
    }

    /*-----------------------------------------------------------------*/
    /* If command is not found, return error.                          */
    /*-----------------------------------------------------------------*/
    if (ShellFuncs[i].func == NULL)
      return -1;
  }
  else
  {
    /*-----------------------------------------------------------------*/
    /* Display four commands per line.                                 */
    /*-----------------------------------------------------------------*/
    printf("LIST of available commands.\n");
    printf("Type 'help command' to view specific command help\n");
    for (i = 0; ShellFuncs[i].func; ++i)
    {
      /*---------------------------------------------------------------*/
      /* Every four commands display and end of line.                  */
      /*---------------------------------------------------------------*/
      if (!(i % 4))
        putchar('\n');

      /*---------------------------------------------------------------*/
      /* Display command name.                                         */
      /*---------------------------------------------------------------*/
      printf("%15s ", ShellFuncs[i].cmd);
    }

    /*-----------------------------------------------------------------*/
    /* Display and end of line at the end.                             */
    /*-----------------------------------------------------------------*/
    putchar('\n');
  }
  return 0;
}

/***********************************************************************/
/*     sh_link: Link a file to another file                            */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_link(char *cmd_line)
{
  int curr;
  char filename[MAX_ARG], linkname[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get name of original file.                                        */
  /*-------------------------------------------------------------------*/
  curr = get_arg(cmd_line, filename);
  if (!curr)
    return -1;

  /*-------------------------------------------------------------------*/
  /* If no name entered for the link, return error.                    */
  /*-------------------------------------------------------------------*/
  if (!get_arg(&cmd_line[curr], linkname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Link and return the result.                                       */
  /*-------------------------------------------------------------------*/
  return link(filename, linkname);
}

/***********************************************************************/
/*       sh_ls: List the contents of a directory                       */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_ls(char *cmd_line)
{
  char flag[MAX_ARG], temp_time[14], permissions[11];
  struct dirent *entry;
  struct stat buf;
  DIR *curr_dir;
  int r_value = 0;

  /*-------------------------------------------------------------------*/
  /* If no flag entered, it's the same as the 'dir' command.           */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, flag))
    return sh_dir(cmd_line);

  /*-------------------------------------------------------------------*/
  /* Else, if it's the -l flag, display additional contents.           */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Ignore all other flags.                                         */
    /*-----------------------------------------------------------------*/
    if (strcmp(flag, "-l"))
      return sh_dir(cmd_line);
    else
    {
      /*---------------------------------------------------------------*/
      /* If unable to get the current working directory, return error. */
      /*---------------------------------------------------------------*/
      if (!getcwd(flag, MAX_ARG - 1))
        return -1;

      /*---------------------------------------------------------------*/
      /* Open the current directory.                                   */
      /*---------------------------------------------------------------*/
      curr_dir = opendir(flag);
      if (curr_dir == NULL)
        return -1;

      /*---------------------------------------------------------------*/
      /* Loop to read every file in the directory.                     */
      /*---------------------------------------------------------------*/
      for (;;)
      {
        /*-------------------------------------------------------------*/
        /* Read one entry at a time from directory.                    */
        /*-------------------------------------------------------------*/
        entry = readdir(curr_dir);

        /*-------------------------------------------------------------*/
        /* If we've reached the end of the directory, stop.            */
        /*-------------------------------------------------------------*/
        if (!entry)
          break;

        /*-------------------------------------------------------------*/
        /* Output contents of entry.                                   */
        /*-------------------------------------------------------------*/
        if (!stat(entry->d_name, &buf))
        {
          /*-----------------------------------------------------------*/
          /* Check if the entry is a directory or a file.              */
          /*-----------------------------------------------------------*/
          permissions[10] = '\0';
          if (S_ISDIR(buf.st_mode))
            permissions[0] = 'd';
          else
            permissions[0] = '-';

          /*-----------------------------------------------------------*/
          /* Set string with permissions for entry.                    */
          /*-----------------------------------------------------------*/
          if (buf.st_mode & S_IRUSR)
            permissions[1] = 'r';
          else
            permissions[1] = '-';
          if (buf.st_mode & S_IWUSR)
            permissions[2] = 'w';
          else
            permissions[2] = '-';
          if (buf.st_mode & S_IXUSR)
            permissions[3] = 'x';
          else
            permissions[3] = '-';
          if (buf.st_mode & S_IRGRP)
            permissions[4] = 'r';
          else
            permissions[4] = '-';
          if (buf.st_mode & S_IWGRP)
            permissions[5] = 'w';
          else
            permissions[5] = '-';
          if (buf.st_mode & S_IXGRP)
            permissions[6] = 'x';
          else
            permissions[6] = '-';
          if (buf.st_mode & S_IROTH)
            permissions[7] = 'r';
          else
            permissions[7] = '-';
          if (buf.st_mode & S_IWOTH)
            permissions[8] = 'w';
          else
            permissions[8] = '-';
          if (buf.st_mode & S_IXOTH)
            permissions[9] = 'x';
          else
            permissions[9] = '-';

          /*-----------------------------------------------------------*/
          /* Create a string version of the access time for the entry. */
          /*-----------------------------------------------------------*/
          strftime(temp_time, 13,"%b %d %H:%M",localtime(&buf.st_atime));

          /*-----------------------------------------------------------*/
          /* Output contents of the entry.                             */
          /*-----------------------------------------------------------*/
          printf("%s %u %5u %5u %10d %s %.*s\n", permissions,
                 buf.st_nlink, buf.st_uid, buf.st_gid, buf.st_size,
                 temp_time, FILENAME_MAX, entry->d_name);
        }
        else
        {
          r_value = -1;
          break;
        }
      }

      /*---------------------------------------------------------------*/
      /* Reset the directory position indicator.                       */
      /*---------------------------------------------------------------*/
      rewinddir(curr_dir);

      /*---------------------------------------------------------------*/
      /* If failed to close directory, return error.                   */
      /*---------------------------------------------------------------*/
      if (closedir(curr_dir))
        return -1;
    }
  }
  return r_value;
}

/***********************************************************************/
/*    sh_mkdir: Create a directory                                     */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_mkdir(char *cmd_line)
{
  char dirname[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get directory name.                                               */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, dirname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Make directory and return result.                                 */
  /*-------------------------------------------------------------------*/
  return mkdir(dirname, S_IRUSR | S_IWUSR | S_IXUSR);
}

/***********************************************************************/
/*     sh_more: Display contents of a file                             */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_more(char *cmd_line)
{
  char filename[MAX_ARG], buf[2 * PAGE_SIZE + 1];
  int num_bytes, fid;

  /*-------------------------------------------------------------------*/
  /* Get name of file.                                                 */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, filename))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Open file in read only mode.                                      */
  /*-------------------------------------------------------------------*/
  fid = open(filename, O_RDONLY);
  if (fid == -1)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Go through file and display it 2 * PAGE_SIZE bytes at a time.     */
  /*-------------------------------------------------------------------*/
  for (num_bytes = 2 * PAGE_SIZE; num_bytes == 2 * PAGE_SIZE;)
  {
    /*-----------------------------------------------------------------*/
    /* Read 2 * PAGE_SIZE bytes at a time from the file.               */
    /*-----------------------------------------------------------------*/
    num_bytes = read(fid, buf, 2 * PAGE_SIZE);

    /*-----------------------------------------------------------------*/
    /* Output the read bytes.                                          */
    /*-----------------------------------------------------------------*/
    buf[num_bytes] = '\0';
    printf("%s", buf);
  }
  putchar('\n');

  /*-------------------------------------------------------------------*/
  /* If closing the file fails, return error.                          */
  /*-------------------------------------------------------------------*/
  if (close(fid))
    return -1;

  return 0;
}

/***********************************************************************/
/*    sh_mount: Mount a file system volume                             */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_mount(char *cmd_line)
{
  char fsname[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get the file system name.                                         */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, fsname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Mount system and return result.                                   */
  /*-------------------------------------------------------------------*/
  return mount(fsname);
}

/***********************************************************************/
/* sh_mount_all: Mount all existing unmounted devices                  */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_mount_all(char *cmd_line)
{
  return mount_all(TRUE);
}

/***********************************************************************/
/*       sh_mv: Move file/dir to new location                          */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_mv(char *cmd_line)
{
  char old_location[MAX_ARG], new_location[MAX_ARG];
  int curr;

  /*-------------------------------------------------------------------*/
  /* Get the old name.                                                 */
  /*-------------------------------------------------------------------*/
  curr = get_arg(cmd_line, old_location);
  if (!curr)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Get the new path name.                                            */
  /*-------------------------------------------------------------------*/
  if (!get_arg(&cmd_line[curr], new_location))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Call rename() function, return its result code.                   */
  /*-------------------------------------------------------------------*/
  return renameFFS(old_location, new_location);
}

/***********************************************************************/
/*      sh_pwd: Get the current working directory                      */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_pwd(char *cmd_line)
{
  char *cwd;

  /*-------------------------------------------------------------------*/
  /* Get name of current working directory.                            */
  /*-------------------------------------------------------------------*/
  cwd = getcwd(NULL, 0);
  if (cwd == NULL)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Display directory name and free memory allocated for name.        */
  /*-------------------------------------------------------------------*/
  puts(cwd);
  free(cwd);
  return 0;
}

/***********************************************************************/
/*       sh_rm: Delete a file                                          */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_rm(char *cmd_line)
{
  char filename[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* If no file name entered, return error.                            */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, filename))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Remove the file and return result.                                */
  /*-------------------------------------------------------------------*/
  return unlink(filename);
}

/***********************************************************************/
/*    sh_rmdir: Delete a directory                                     */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_rmdir(char *cmd_line)
{
  char dirname[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* If no dir name entered, return error.                             */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, dirname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Remove the directory and return result.                           */
  /*-------------------------------------------------------------------*/
  return rmdir(dirname);
}

/***********************************************************************/
/*     sh_stat: Show statistics of a file                              */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_stat(char *cmd_line)
{
  struct stat buf;
  char filename[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get name of file.                                                 */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, filename))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Call stat() to get the file's statistics.                         */
  /*-------------------------------------------------------------------*/
  if (stat(filename, &buf))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Display some of the statistics.                                   */
  /*-------------------------------------------------------------------*/
  printf("File Access   time: %s", ctime(&buf.st_atime));
  printf("File Modified time: %s", ctime(&buf.st_mtime));
  printf("File Mode: %d\n", buf.st_mode);
  printf("File Serial Number: %d\n", buf.st_ino);
  printf("File Num Links: %d\n", buf.st_nlink);
  printf("File Size (bytes): %u\n", buf.st_size);
  return 0;
}

/***********************************************************************/
/* sortdir_cmp: Comparison function used by sortdir(). Comparisons     */
/*              are done by one of: name, file number, size, mod time  */
/*                                                                     */
/*      Inputs: e1 = first entry to compare                            */
/*              e2 = second entry to compare                           */
/*                                                                     */
/*     Returns: -1 if e1 < e2, 0 if e1 = e2, 1 if e1 > e2              */
/*                                                                     */
/***********************************************************************/
static int sortdir_cmp(const DirEntry *e1, const DirEntry *e2)
{
  switch (SortDir)
  {
    case 2:
    {
      if (e1->st_size < e2->st_size)
        return -1;
      else if (e1->st_size > e2->st_size)
        return 1;
      else
        return 0;
    }

    case 3:
    {
      if (e1->st_ino < e2->st_ino)
        return -1;
      else if (e1->st_ino > e2->st_ino)
        return 1;
      else
        return 0;
    }

    case 4:
    {
      if (e1->st_mtime < e2->st_mtime)
        return -1;
      else if (e1->st_mtime > e2->st_mtime)
        return 1;
      else
        return 0;
    }

    default:
      return strcmp(e1->st_name, e2->st_name);
  }
}

/***********************************************************************/
/*  sh_sortdir: Sort a directory                                       */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_sortdir(char *cmd_line)
{
  int curr;
  char dirname[MAX_ARG], sortoption[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get name of directory.                                            */
  /*-------------------------------------------------------------------*/
  curr = get_arg(cmd_line, dirname);
  if (!curr)
    return -1;

  /*-------------------------------------------------------------------*/
  /* If no sortoption assume its by name.                              */
  /*-------------------------------------------------------------------*/
  if (!get_arg(&cmd_line[curr], sortoption))
    SortDir = 1;
  else
  {
    if (!strcmp(sortoption, "2"))
      SortDir = 2;
    else if (!strcmp(sortoption, "3"))
      SortDir = 3;
    else if (!strcmp(sortoption, "4"))
      SortDir = 4;
    else
      SortDir = 1;
  }

  /*-------------------------------------------------------------------*/
  /* Sort the directory.                                               */
  /*-------------------------------------------------------------------*/
  return sortdir(dirname, sortdir_cmp);
}

/***********************************************************************/
/*     sh_time: Time any valid shell command                           */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_time(char *cmd_line)
{
  clock_t delta, sample;

  /*-------------------------------------------------------------------*/
  /* Start time measurement for app.                                   */
  /*-------------------------------------------------------------------*/
  sample = clock();

  /*-------------------------------------------------------------------*/
  /* Execute shell command.                                            */
  /*-------------------------------------------------------------------*/
  process_cmd(cmd_line);

  /*-------------------------------------------------------------------*/
  /* Get ending time stamp and display result.                         */
  /*-------------------------------------------------------------------*/
  delta = clock() - sample;
  printf("shell command took %u ticks (%u seconds)\n",
         delta, delta / CLOCKS_PER_SEC);
  return 0;
}

/***********************************************************************/
/*    sh_trunc: Truncate a file                                        */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_trunc(char *cmd_line)
{
  int curr, size;
  char fname[MAX_ARG], size_str[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get name of the file.                                             */
  /*-------------------------------------------------------------------*/
  curr = get_arg(cmd_line, fname);
  if (!curr)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Get the size of the file after truncation.                        */
  /*-------------------------------------------------------------------*/
  if (!get_arg(&cmd_line[curr], size_str))
    return -1;
  size = atoi(size_str);
  if (size < 0)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Truncate file.                                                    */
  /*-------------------------------------------------------------------*/
  return truncate(fname, (off_t)size);
}

/***********************************************************************/
/* sh_unformat: Unformat a file system                                 */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_unformat(char *cmd_line)
{
  char fsname[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get the file system name.                                         */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, fsname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Unformat file system and return result.                           */
  /*-------------------------------------------------------------------*/
  return unformat(fsname);
}

/***********************************************************************/
/*  sh_unmount: Unmount a file system volume                           */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_unmount(char *cmd_line)
{
  char fsname[MAX_ARG];

  /*-------------------------------------------------------------------*/
  /* Get the file system name.                                         */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, fsname))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Unmount file system and return result.                            */
  /*-------------------------------------------------------------------*/
  return unmount(fsname);
}

/***********************************************************************/
/*    sh_vstat: Show statistics of a volume                            */
/*                                                                     */
/*       Input: cmd_line = rest of line from user                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int sh_vstat(char *cmd_line)
{
  char device[MAX_ARG];
  union vstat buf;

  /*-------------------------------------------------------------------*/
  /* Get volume name.                                                  */
  /*-------------------------------------------------------------------*/
  if (!get_arg(cmd_line, device))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Get volume statistics.                                            */
  /*-------------------------------------------------------------------*/
  if (vstat(device, &buf))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Output some of the stats for volume.                              */
  /*-------------------------------------------------------------------*/
  if (buf.ffs.vol_type == FFS_VOL)
  {
    printf("TargetFFS-%s volume\n", (buf.ffs.flash_type == FFS_NOR) ?
           "NOR" : "NAND");
    printf("Sector Size (bytes): %u\n", buf.ffs.sect_size);
    printf("Total Num Sectors: %u\n", buf.ffs.num_sects);
    printf("Num Data Sectors: %u\n", buf.ffs.used_sects);
    printf("Available Sectors: %u\n", buf.ffs.avail_sects);
    printf("Wear Count: %u\n", buf.ffs.wear_count);
  }
  else if (buf.rfs.vol_type == RFS_VOL)
  {
    printf("TargetRFS volume\n");
    printf("Sector Size (bytes): %u\n", buf.rfs.sect_size);
    printf("Total Num Sectors: %u\n", buf.rfs.num_sects);
  }
  else if (buf.rfs.vol_type == ZFS_VOL)
  {
    printf("TargetZFS volume\n");
    printf("Num Entries: %u\n", buf.zfs.num_entries);
    printf("Unc Block Size: %u\n", buf.zfs.block_size);
    printf("Com Block Size: %u\n", buf.zfs.cblock_size);
  }
  else /* FAT_VOL */
  {
    printf("TargetFAT volume\n");
    printf("Cluster Size: %u\n", buf.fat.clust_size);
    printf("Num Clusters: %u\n", buf.fat.num_clusts);
    printf("Used Clusters: %u\n", buf.fat.used_clusts);
    printf("Free Clusters: %u\n", buf.fat.free_clusts);
    printf("FAT Type: FAT%u\n", buf.fat.fat_type == FAT12 ? 12 :
                                buf.fat.fat_type == FAT16 ? 16 : 32);
    printf("Root Dir Sects: %u\n", buf.fat.root_dir_sects);
    printf("Sector Size (bytes): %u\n", buf.fat.sect_size);
    printf("Sects Per FAT: %u\n", buf.fat.sects_per_fat);
    printf("Num FATs: %u\n", buf.fat.num_fats);
  }
  return 0;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*       Shell: Implement the shell                                    */
/*                                                                     */
/*      Inputs: input = input stream                                   */
/*              output = output stream                                 */
/*                                                                     */
/***********************************************************************/
void Shell(FILE *input, FILE *output)
{
  char cmd_line[MAX_LINE];

  /*-------------------------------------------------------------------*/
  /* Shell is loop: command->action.                                   */
  /*-------------------------------------------------------------------*/
  for (;;)
  {
    /*-----------------------------------------------------------------*/
    /* Output the shell symbol.                                        */
    /*-----------------------------------------------------------------*/
    printf("%% ");

    /*-----------------------------------------------------------------*/
    /* If fail to get command line from user, return.                  */
    /*-----------------------------------------------------------------*/
    if (get_line(cmd_line))
      return;

    /*-----------------------------------------------------------------*/
    /* Process command. Return upon request or I/O error.              */
    /*-----------------------------------------------------------------*/
    if (process_cmd(cmd_line))
      return;
  }
}

