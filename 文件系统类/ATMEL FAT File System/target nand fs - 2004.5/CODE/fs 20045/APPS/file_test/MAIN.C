/***********************************************************************/
/*                                                                     */
/*   Module:  main.c                                                   */
/*   Release: 2004.5                                                   */
/*   Version: 2004.3                                                   */
/*   Purpose: Implement the file test application                      */
/*                                                                     */
/***********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <targetos.h>
#include <kernel.h>
#include <sys.h>
#include <pccard.h>
#include "..\..\posix.h"
#include "..\..\ffs_stdio.h"

/***********************************************************************/
/* Configuration                                                       */
/***********************************************************************/
#define ID_REG          0
#define CWD_WD1         1
#define CWD_WD2         2
#define BUF_SIZE        100
#define RFS_NAME        "rfs"

/***********************************************************************/
/* Global Data Declarations                                            */
/***********************************************************************/
static char *Volume[] =
{
#if NUM_RFS_VOLS
  RFS_NAME,
#endif
#if PCCARD_SUPPORT
  "ata",
#endif
#if INC_FAT_FIXED
  "fat",
#endif
#if NUM_NAND_FTLS
  "ftld",
#endif
#if INC_NAND_FS
  "nand",
#endif
#if INC_NOR_FS
  "nor",
#endif
};

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*   file_test: Test assorted file system calls                        */
/*                                                                     */
/*       Input: vol_name = name of file system volume                  */
/*                                                                     */
/***********************************************************************/
static int file_test(char *vol_name)
{
  int i, k, fid, fid2;
  char hello[] = "Hello World, I'm testing the cool FFS!\n";
  const char *fname = "hello.c", *fname2 = "rhello.c";
  struct stat stbuf;
  char buf[BUF_SIZE];
  union vstat stats;
  off_t offset;

  /*-------------------------------------------------------------------*/
  /* Test stat() on volume name.                                       */
  /*-------------------------------------------------------------------*/
  snprintf(buf, BUF_SIZE, "/%s", vol_name);
  if (stat(buf, &stbuf) && S_ISREG(stbuf.st_mode))
  {
    perror("Error running stat() on volume name");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* The next tests are not compatible with FAT.                       */
  /*-------------------------------------------------------------------*/
  vstat(vol_name, &stats);
  if (stats.fat.vol_type != FAT_VOL)
  {
    /*-----------------------------------------------------------------*/
    /* Test re-opening a previously opened and deleted file.           */
    /*-----------------------------------------------------------------*/
    sprintf(buf, "//%s//bar", vol_name);
    fid = open(buf, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IXUSR);
    if (fid < 0)
    {
      perror("open()");
      return -1;
    }
    if (unlink(buf))
    {
      perror("unlink()");
      return -1;
    }
    fid2 = open(buf, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IXUSR);
    if (fid2 < 0)
    {
      perror("open()");
      return -1;
    }
    if (unlink(buf))
    {
      perror("unlink()");
      return -1;
    }
    if (close(fid))
    {
      perror("close()");
      return -1;
    }
    if (close(fid2))
    {
      perror("close()");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Check that open descriptor on a link still functions correctly  */
    /* until closed even after the actual link is removed.             */
    /*-----------------------------------------------------------------*/
    sprintf(buf, "//%s//foo", vol_name);
    fid = creat(buf, 0666);
    if (fid == -1)
    {
      perror("Error creating file");
      return -1;
    }
    if (unlink("foo"))
    {
      perror("Error removing file");
      return -1;
    }
    if (write(fid, buf, 100) != 100)
    {
      perror("Error writing to file");
      return -1;
    }
    if (close(fid))
    {
      perror("Error closing file");
      return -1;
    }
    fid = open("foo", O_RDONLY);
    if (fid != -1)
    {
      perror("File shouldn't exist");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Check that an open descriptor on a link works across a rename   */
    /* call for that link.                                             */
    /*-----------------------------------------------------------------*/
    fid = creat("foo", 0666);
    if (fid == -1)
    {
      perror("Error creating file");
      return -1;
    }
    if (renameFFS("foo", "foo2"))
    {
      perror("Error renaming file");
      return -1;
    }
    if (write(fid, buf, 700) != 700)
    {
      perror("Error writing to file");
      return -1;
    }
    if (close(fid))
    {
      perror("Error closing file");
      return -1;
    }
    if (unlink("foo2"))
    {
      perror("Error removing file");
      return -1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Successfully rename a file to its existing name.                  */
  /*-------------------------------------------------------------------*/
  fid = open("snork", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IXUSR);
  if (write(fid, buf, BUF_SIZE) != BUF_SIZE)
  {
    perror("Error writing to file");
    return -1;
  }
  if (close(fid))
  {
    perror("close()");
    return -1;
  }
  if (renameFFS("snork", "snork"))
  {
    perror("Error renaming file to its existing name");
    return -1;
  }
  if (unlink("snork"))
  {
    perror("unlink()");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Successfully rename an open file.                                 */
  /*-------------------------------------------------------------------*/
  fid = open("snit", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IXUSR);
  if (write(fid, buf, BUF_SIZE) != BUF_SIZE)
  {
    perror("Error writing to file");
    return -1;
  }
  if (renameFFS("snit", "snout"))
  {
    perror("Error renaming file to its existing name");
    return -1;
  }
  if (close(fid))
  {
    perror("close()");
    return -1;
  }
  if (unlink("snout"))
  {
    perror("unlink()");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Other renaming tests.                                             */
  /*-------------------------------------------------------------------*/
  if (mkdir("dira", S_IRUSR | S_IWUSR | S_IXUSR))
  {
    perror("Error making directory");
    return -1;
  }
  if (renameFFS("dira", "dirb"))
  {
    perror("Error renaming directory");
    return -1;
  }
  if (renameFFS("dirb", "dira"))
  {
    perror("Error renaming directory");
    return -1;
  }
  if (rmdir("dira"))
  {
    perror("Error removing directory");
    return -1;
  }
  fid = creat("filea.txt", S_IRUSR | S_IWUSR | S_IXUSR);
  if (fid == -1)
  {
    perror("Error creating the file");
    return -1;
  }
  if (write(fid, buf, 500) != 500)
  {
    perror("Error writing to file");
    return -1;
  }
  if (close(fid))
  {
    perror("Error closing file");
    return -1;
  }
  if (renameFFS("filea.txt", "fileb.txt"))
  {
    perror("Error renaming file");
    return -1;
  }
  if (renameFFS("fileb.txt", "filea.txt"))
  {
    perror("Error renaming file");
    return -1;
  }
  if (unlink("filea.txt"))
  {
    perror("Error removing file");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Test the seek routines.                                           */
  /*-------------------------------------------------------------------*/
  fid = creat("write.txt", S_IRUSR | S_IWUSR | S_IXUSR);
  if (fid == -1)
  {
    perror("Error creating the file");
    return -1;
  }
  if (write(fid, buf, 678) != 678)
  {
    perror("Error writing to file");
    return -1;
  }
  offset = lseek(fid, -352, SEEK_CUR);
  if (offset == (off_t)-1)
  {
    perror("Error seeking in file");
    return -1;
  }
  if (write(fid, buf, 797) != 797)
  {
    perror("Error writing to file");
    return -1;
  }
  if (fstat(fid, &stbuf))
  {
    perror("Error getting stats for file");
    return -1;
  }
  if (stbuf.st_size != 1123)
  {
    printf("fstat() st_size is unexpected!\n");
    return -1;
  }
  if (close(fid))
  {
    perror("Error closing file");
    return -1;
  }
  if (unlink("write.txt"))
  {
    perror("Error removing file");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* File creation, deletion loop.                                     */
  /*-------------------------------------------------------------------*/
  for (k = 0; k < 3; ++k)
  {
    /*-----------------------------------------------------------------*/
    /* Open two text files.                                            */
    /*-----------------------------------------------------------------*/
    fid = open(fname, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IXUSR);
    if (fid == -1)
    {
      perror("Error opening the file");
      return -1;
    }

    fid2 = open(fname2, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IXUSR);
    if (fid2 == -1)
    {
      perror("Error opening the file");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Exercise fcntl() with F_GETFL and F_SETFL.                      */
    /*-----------------------------------------------------------------*/
    i = fcntl(fid, F_GETFL);
    printf("flags = 0x%02X\n", i);
    fcntl(fid, F_SETFL, O_ASYNC | O_NONBLOCK);
    i = fcntl(fid, F_GETFL);
    printf("flags = 0x%02X\n", i);

    /*-----------------------------------------------------------------*/
    /* Write contents of hello[] to files.                             */
    /*-----------------------------------------------------------------*/
    if (write(fid, hello, sizeof(hello)) != sizeof(hello) )
    {
      perror("Error writing to file");
      return -1;
    }

    for (i = 0; i < 1000; ++i)
    {
      if (write(fid2, hello, sizeof(hello)) != sizeof(hello))
      {
        perror("Error writing to file");
        return -1;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Retrieve stats on the two files.                                */
    /*-----------------------------------------------------------------*/
    if (fstat(fid, &stbuf))
      perror("Error reading file stats");
    else
    {
      printf("File access time:   %s", ctime(&stbuf.st_atime));
      printf("File modified time: %s", ctime(&stbuf.st_mtime));
      printf("File mode:          %d\n", stbuf.st_mode);
      printf("File serial number: %d\n", stbuf.st_ino);
      printf("File num links:     %d\n", stbuf.st_nlink);
      printf("File size (byte):   %u\n", stbuf.st_size);
      printf("File user ID:       %u\n", stbuf.st_uid);
      printf("File group ID:      %u\n", stbuf.st_gid);
    }

    if (fstat(fid2, &stbuf))
      perror("Error reading file stats");
    else
    {
      printf("File access time:   %s", ctime(&stbuf.st_atime));
      printf("File modified time: %s", ctime(&stbuf.st_mtime));
      printf("File mode:          %d\n", stbuf.st_mode);
      printf("File serial number: %d\n", stbuf.st_ino);
      printf("File num links:     %d\n", stbuf.st_nlink);
      printf("File size (byte):   %u\n", stbuf.st_size);
      printf("File user ID:       %u\n", stbuf.st_uid);
      printf("File group ID:      %u\n", stbuf.st_gid);
    }

    /*-----------------------------------------------------------------*/
    /* Use ftruncate to shorten the file to half its length.           */
    /*-----------------------------------------------------------------*/
    offset = lseek(fid2, 0, SEEK_END);
    if (ftruncate(fid2, offset / 2))
    {
      perror("Error truncating file");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Display and verify the file's size.                             */
    /*-----------------------------------------------------------------*/
    stat(fname2, &stbuf);
    printf("\"%s\" size after truncate(., %d) = %d\n",
           fname2, offset / 2, stbuf.st_size);
    if (stbuf.st_size != (offset / 2))
    {
      printf("File length is unexpected\n");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Move file position to the new end of file.                      */
    /*-----------------------------------------------------------------*/
    if (lseek(fid2, (off_t)0, SEEK_END) < 0)
    {
      perror("lseek() failed");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Write four bytes and verify the file's size.                    */
    /*-----------------------------------------------------------------*/
    if (write(fid2, "\0\1\2\3", 4) != 4)
    {
      perror("Error writing to file");
      return -1;
    }
    stat(fname2, &stbuf);
    printf("\"%s\" size after write() = %d\n", fname2, stbuf.st_size);
    if (stbuf.st_size != (offset / 2 + 4))
    {
      printf("File length is unexpected\n");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Use truncate() to extend the file to its original length.       */
    /*-----------------------------------------------------------------*/
    if (truncate(fname2, offset))
    {
      perror("Error truncating file");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Display and verify the file's length.                           */
    /*-----------------------------------------------------------------*/
    stat(fname2, &stbuf);
    printf("\"%s\" size after truncate(., %d) = %d\n",
           fname2, offset, stbuf.st_size);
    if (stbuf.st_size != offset)
    {
      printf("File length is unexpected\n");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* truncate() should not affect the file offset. So write four     */
    /* bytes and verify the length is unchanged.                       */
    /*-----------------------------------------------------------------*/
    if (write(fid2, "\0\1\2\3", 4) != 4)
    {
      perror("Error writing to file");
      return -1;
    }
    stat(fname2, &stbuf);
    printf("\"%s\" size after write() = %d\n", fname2, stbuf.st_size);
    if (stbuf.st_size != offset)
    {
      printf("File length is unexpected\n");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Ensure file holds zero from current position to original end.   */
    /*-----------------------------------------------------------------*/
    for (i = offset / 2 + 8; i < offset; ++i)
    {
      ui8 ch;

      if (read(fid2, &ch, 1) != 1)
      {
        perror("Error reading from file");
        return -1;
      }
      if (ch)
      {
        printf("File not zero-filled correctly\n");
        return -1;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Truncate the file to a multiple of the sector size.             */
    /*-----------------------------------------------------------------*/
    if (ftruncate(fid2, stats.ffs.sect_size * 3))
    {
      perror("Error truncating file");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Ensure the truncated file has the correct length.               */
    /*-----------------------------------------------------------------*/
    if (fstat(fid2, &stbuf))
    {
      perror("Error reading file's statistics");
      return -1;
    }
    if (stbuf.st_size != stats.ffs.sect_size * 3)
    {
      perror("file has wrong size after truncation");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Close the working files.                                        */
    /*-----------------------------------------------------------------*/
    if (close(fid))
    {
      perror("Error closing file");
      return -1;
    }
    if (close(fid2))
    {
      perror("Error closing file");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Delete the working files.                                       */
    /*-----------------------------------------------------------------*/
    if (unlink(fname))
    {
      perror("Error removing file");
      return -1;
    }
    if (unlink(fname2))
    {
      perror("Error removing file");
      return -1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Return success.                                                   */
  /*-------------------------------------------------------------------*/
  return 0;
}

/***********************************************************************/
/* creatn_test: Test the creatn() function                             */
/*                                                                     */
/*       Input: vol_name = name of file system volume                  */
/*                                                                     */
/***********************************************************************/
static int creatn_test(char *vol_name)
{
  int fid, i, rc, sect_size;
  char *fname = "log_file";
  struct stat stbuf;
  union vstat stats;
  void *buf;

  /*-------------------------------------------------------------------*/
  /* Determine the volume's sector size.                               */
  /*-------------------------------------------------------------------*/
  if (vstat(vol_name, &stats))
  {
    perror("Error reading volume statistics");
    return -1;
  }
  sect_size = stats.ffs.sect_size;

  /*-------------------------------------------------------------------*/
  /* Allocate a sector-sized buffer for writing to the file.           */
  /*-------------------------------------------------------------------*/
  buf = malloc(sect_size);
  if (buf == NULL)
  {
    perror("Error allocating creatn() buffer");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Create a special file with the length of 10 sectors.              */
  /*-------------------------------------------------------------------*/
  fid = creatn(fname, 0666, 10 * sect_size);
  if (fid == -1)
  {
    perror("Error creating file");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Many writes that should all succeed and not trigger a recycle.    */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < 10; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Write a full sector of data.                                    */
    /*-----------------------------------------------------------------*/
    if (write(fid, buf, sect_size) != sect_size)
    {
      perror("Error writing to file");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Read file's statistics and check that its size hasn't changed.  */
    /*-----------------------------------------------------------------*/
    if (fstat(fid, &stbuf))
    {
      perror("Error reading file's statistics");
      return -1;
    }
    if (stbuf.st_size != 10 * sect_size)
    {
      perror("creatn() file size changed");
      return -1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Try to write past the end.                                        */
  /*-------------------------------------------------------------------*/
  rc = lseek(fid, 0, SEEK_END);
  if (rc < 0)
  {
    perror("Error seeking to end");
    return -1;
  }
  if (write(fid, buf, sect_size) != -1)
  {
    perror("Able to write past creatn() end");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Try to seek past the end.                                         */
  /*-------------------------------------------------------------------*/
  rc = lseek(fid, sect_size, SEEK_END);
  if (rc > 0)
  {
    perror("Able to seek past creatn() end");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Close and delete the file.                                        */
  /*-------------------------------------------------------------------*/
  if (close(fid))
  {
    perror("Error closing file");
    return -1;
  }
  if (unlink(fname))
  {
    perror("Error removing file");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Free sector write buffer and return success.                      */
  /*-------------------------------------------------------------------*/
  free(buf);
  return 0;
}

/***********************************************************************/
/*    cmp_name: Compare directory entries by name                      */
/*                                                                     */
/***********************************************************************/
static int cmp_name(const DirEntry *e1, const DirEntry *e2)
{
  return strcmp(e1->st_name, e2->st_name);
}

/***********************************************************************/
/*    cmp_size: Compare directory entries by size                      */
/*                                                                     */
/***********************************************************************/
static int cmp_size(const DirEntry *e1, const DirEntry *e2)
{
  return e1->st_size - e2->st_size;
}

/***********************************************************************/
/*   sort_test: Test directory entry sorting                           */
/*                                                                     */
/*       Input: vol_name = name of file system volume                  */
/*                                                                     */
/***********************************************************************/
static int sort_test(char *vol_name)
{
  int fid, i;
  DIR *dir;
  struct stat pbuf, ebuf;
  struct dirent *entry, *prev;
  char fname[2], nbuf[FILENAME_MAX];

  /*-------------------------------------------------------------------*/
  /* Create test directory and change CWD to it.                       */
  /*-------------------------------------------------------------------*/
  if (mkdir("sort", S_IRUSR | S_IWUSR | S_IXUSR) || chdir("sort"))
  {
    perror("Error making or cd'ing into directory");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Create the test files.                                            */
  /*-------------------------------------------------------------------*/
  fname[1] = 0;
  for (i = 0; i < 20; ++i)
  {
    fname[0] = 'A' + i;
    fid = creat(fname, 0666);
    write(fid, "heap", 200 - i);
    close(fid);
  }

  /*-------------------------------------------------------------------*/
  /* List the directory contents.                                      */
  /*-------------------------------------------------------------------*/
  dir = opendir("../sort");
  while ((entry = readdir(dir)) != NULL)
    printf("  %s", entry->d_name);
  putchar('\n');
  closedir(dir);

  /*-------------------------------------------------------------------*/
  /* Sort directory entries by alphanumeric name order.                */
  /*-------------------------------------------------------------------*/
  if (sortdir("../sort", cmp_name))
    return -1;

  /*-------------------------------------------------------------------*/
  /* List the directory contents.                                      */
  /*-------------------------------------------------------------------*/
  dir = opendir("../sort");
  while ((entry = readdir(dir)) != NULL)
    printf("  %s", entry->d_name);
  putchar('\n');
  closedir(dir);

  /*-------------------------------------------------------------------*/
  /* Test the directory order.                                         */
  /*-------------------------------------------------------------------*/
  dir = opendir("../sort");
  for (prev = readdir(dir);; prev = entry)
  {
    strncpy(nbuf, prev->d_name, FILENAME_MAX);
    entry = readdir(dir);
    if (entry == NULL)
      break;
    if (strcmp(nbuf, entry->d_name) > 0)
      return -1;
  }
  closedir(dir);

  /*-------------------------------------------------------------------*/
  /* Sort directory entries by file size order.                        */
  /*-------------------------------------------------------------------*/
  if (sortdir("../sort", cmp_size))
    return -1;

  /*-------------------------------------------------------------------*/
  /* List the directory contents.                                      */
  /*-------------------------------------------------------------------*/
  dir = opendir("../sort");
  while ((entry = readdir(dir)) != NULL)
    printf("  %s", entry->d_name);
  putchar('\n');
  closedir(dir);

  /*-------------------------------------------------------------------*/
  /* Test the directory order.                                         */
  /*-------------------------------------------------------------------*/
  dir = opendir("../sort");
  entry = readdir(dir);
  stat(entry->d_name, &pbuf);
  while ((entry = readdir(dir)) != NULL)
  {
    stat(entry->d_name, &ebuf);
    if (pbuf.st_size > ebuf.st_size)
      return -1;
    pbuf.st_size = ebuf.st_size;
  }
  closedir(dir);

  /*-------------------------------------------------------------------*/
  /* Delete the test files.                                            */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < 20; ++i)
  {
    fname[0] = 'A' + i;
    unlink(fname);
  }

  /*-------------------------------------------------------------------*/
  /* Change CWD to volume root and delete test directory.              */
  /*-------------------------------------------------------------------*/
  if (chdir("..") || rmdir("sort"))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Return success.                                                   */
  /*-------------------------------------------------------------------*/
  return 0;
}

/***********************************************************************/
/*   seek_test: Test file postion after reads and seeks                */
/*                                                                     */
/***********************************************************************/
static int seek_test(void)
{
  int fid, res, rc = -1;
  struct stat st;
  ui8 buf[600];
  off_t cur, end;

  /*-------------------------------------------------------------------*/
  /* Create file "test.txt" and write 11 bytes to it.                  */
  /*-------------------------------------------------------------------*/
  fid = open("test.txt", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IXUSR);
  if (fid == -1)
  {
    perror("open() failed");
    return -1;
  }
  if (write(fid, "01234567890", 11) != 11)
  {
    perror("write() failed");
    goto seek_err;
  }

  /*-------------------------------------------------------------------*/
  /* Close file and then check its size.                               */
  /*-------------------------------------------------------------------*/
  close(fid);
  stat("test.txt", &st);
  if (st.st_size != 11)
  {
    unlink("test.txt");
    perror("stat() reported wrong size");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Open file "test.txt" as read-only file.                           */
  /*-------------------------------------------------------------------*/
  fid = open("test.txt", O_RDONLY);
  if (fid == -1)
  {
    unlink("test.txt");
    perror("open() failed");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Read some bytes and check read()'s return value.                  */
  /*-------------------------------------------------------------------*/
  res = read(fid, buf, 1);
  if (res != 1)
    goto seek_err;
  res = read(fid, buf, 1);
  if (res != 1)
    goto seek_err;
  res = read(fid, buf, 4);
  if (res != 4)
    goto seek_err;

  /*-------------------------------------------------------------------*/
  /* Read remaining bytes with buf size bigger than the sector size.   */
  /*-------------------------------------------------------------------*/
  res = read(fid, buf, 600);
  if (res != 5)
    goto seek_err;

  /*-------------------------------------------------------------------*/
  /* java.io.FileInputStream.available() behaves like this.            */
  /*-------------------------------------------------------------------*/
  cur = lseek(fid, 0, SEEK_CUR);
  if (cur != 11)
    goto seek_err;
  end = lseek(fid, 0, SEEK_END);
  if (end != 11)
    goto seek_err;
  lseek(fid, cur, SEEK_SET);

  /*-------------------------------------------------------------------*/
  /* Read again. We should get 0 as the result.                        */
  /*-------------------------------------------------------------------*/
  res = read(fid, buf, 600);
  if (res)
    goto seek_err;
  res = read(fid, buf, 600);
  if (res)
    goto seek_err;

  /*-------------------------------------------------------------------*/
  /* Either clear return value or print error message. Clean up.       */
  /*-------------------------------------------------------------------*/
  rc = 0;
seek_err:
  if (rc)
    perror("Wrong read() return value");
  close(fid);
  unlink("test.txt");
  return rc;
}

/***********************************************************************/
/*  seek_test2: Test seeks that hit and cross sector boundaries        */
/*                                                                     */
/***********************************************************************/
static int seek_test2(void)
{
  int fid, i, rc = -1;
  ui8 bv;

  /*-------------------------------------------------------------------*/
  /* Create file "test.txt" and write repeating pattern to it.         */
  /*-------------------------------------------------------------------*/
  fid = open("seek.tst", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IXUSR);
  if (fid == -1)
  {
    perror("open() failed");
    return -1;
  }
  for (i = 0; i < 1234; ++i)
  {
    bv = (ui8)(1234 - i);
    write(fid, &bv, 1);
  }

  /*-------------------------------------------------------------------*/
  /* Repeatedly seek, check position, read, and check value.           */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < 1234; ++i)
  {
    lseek(fid, 0, SEEK_SET);
    lseek(fid, i, SEEK_SET);
    if (ftellFFS(fdopenFFS(fid, NULL)) != i)
    {
      perror("Seek test position error");
      goto seek_err;
    }
    read(fid, &bv, 1);
    if (bv != (ui8)(1234 - i))
    {
      perror("Seek test data error");
      goto seek_err;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Clear return value if successful. Clean up.                       */
  /*-------------------------------------------------------------------*/
  rc = 0;
seek_err:
  close(fid);
  unlink("seek.tst");
  return rc;
}

/***********************************************************************/
/*  utime_test: Test utime()                                           */
/*                                                                     */
/*       Input: vol_name = name of file system volume                  */
/*                                                                     */
/***********************************************************************/
static int utime_test(char *vol_name)
{
  int fid;
  uid_t uid;
  gid_t gid;
  union vstat stats;

  /*-------------------------------------------------------------------*/
  /* Check detection of ENOENT errors.                                 */
  /*-------------------------------------------------------------------*/
  if (mkdir("diru", S_IRUSR | S_IWUSR | S_IXUSR))
  {
    perror("Error making directory");
    return -1;
  }
  if (utime("dira/file", NULL) == 0)
  {
    perror("Missed need for execute permission");
    return -1;
  }
  if (utime("", NULL) == 0)
  {
    perror("Missed need for non-empty path");
    return -1;
  }
  if (errno != ENOENT)
  {
    perror("Set wrong errno value");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Check detection of EFAULT errors.                                 */
  /*-------------------------------------------------------------------*/
  if (utime(NULL, NULL) == 0)
  {
    perror("Missed need for non-NULL path");
    return -1;
  }
  if (errno != EFAULT)
  {
    perror("Set wrong errno value");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Check detection of ENOTDIR errors.                                */
  /*-------------------------------------------------------------------*/
  fid = creat("notdir", 0666);
  if (fid == -1)
  {
    perror("Error creating file");
    return -1;
  }
  if (utime("notdir/file", NULL) == 0)
  {
    perror("Missed need for non-NULL path");
    return -1;
  }
  if (errno != ENOTDIR)
  {
    perror("Set wrong errno value");
    return -1;
  }
  if (close(fid))
  {
    perror("Error closing file");
    return -1;
  }
  if (unlink("notdir"))
  {
    perror("Error removing file");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Skip test for access and permission errors if TargetFAT volume.   */
  /*-------------------------------------------------------------------*/
  vstat(vol_name, &stats);
  if (stats.fat.vol_type != FAT_VOL)
  {
    /*-----------------------------------------------------------------*/
    /* Check detection of EACCES errors.                               */
    /*-----------------------------------------------------------------*/
    fid = creat("diru/file", 0666);
    if (fid == -1)
    {
      perror("Error creating file");
      return -1;
    }
    if (chmod("diru", S_IRGRP | S_IWGRP))
    {
      perror("Error changing permissions");
      return -1;
    }
    if (utime("diru/file", NULL) == 0)
    {
      perror("Missed need for dir execute permission");
      return -1;
    }
    if (errno != EACCES)
    {
      perror("Set wrong errno value");
      return -1;
    }
    if (chmod("diru", S_IRGRP | S_IWGRP | S_IXGRP))
    {
      perror("Error changing permissions");
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Check detection of EPERM errors.                                */
    /*-----------------------------------------------------------------*/
    FsGetId(&uid, &gid);
    FsSetId(uid + 1, gid);
    if (utime("diru/file", NULL) == 0)
    {
      perror("Missed need for file permission");
      return -1;
    }
    if (errno != EPERM)
    {
      perror("Set wrong errno value");
      return -1;
    }
    FsSetId(uid, gid);
    if (close(fid))
    {
      perror("Error closing file");
      return -1;
    }
    if (unlink("diru/file"))
    {
      perror("Error removing file");
      return -1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Check detection of ENAMETOOLONG errors.                           */
  /*-------------------------------------------------------------------*/
  {
    static char too_long[PATH_MAX + 2];

    memset(too_long, 'A', PATH_MAX + 1);
    if (utime(too_long, NULL) == 0)
    {
      perror("Missed overly long file name");
      return -1;
    }
    if (errno != ENAMETOOLONG)
    {
      perror("Set wrong errno value");
      return -1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Clean up.                                                         */
  /*-------------------------------------------------------------------*/
  if (rmdir("diru"))
  {
    perror("Error removing directory");
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Return success.                                                   */
  /*-------------------------------------------------------------------*/
  return 0;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*        main: Application Entry Point                                */
/*                                                                     */
/***********************************************************************/
void main(ui32 unused)
{
  int i;
  char *vol_name;
  union vstat stats;

  /*-------------------------------------------------------------------*/
  /* Lower interrupt mask and start scheduling.                        */
  /*-------------------------------------------------------------------*/
  OsStart();

  /*-------------------------------------------------------------------*/
  /* Lower our priority.                                               */
  /*-------------------------------------------------------------------*/
  taskSetPri(RunningTask, 20);

  /*-------------------------------------------------------------------*/
  /* Null CWD variables and assign this task's user and group ID.      */
  /*-------------------------------------------------------------------*/
  FsSetId(0, 0);
  FsSaveCWD(0, 0);

  /*-------------------------------------------------------------------*/
  /* Initialize the file system.                                       */
  /*-------------------------------------------------------------------*/
  if (InitFFS())
    SysFatalError(errno);

#if NUM_RFS_VOLS
  /*-------------------------------------------------------------------*/
  /* Add the TargetRFS volume.                                         */
  /*-------------------------------------------------------------------*/
  if (RfsAddVol(RFS_NAME))
    SysFatalError(errno);
#endif

  /*-------------------------------------------------------------------*/
  /* Run the test programs in an endless loop.                         */
  /*-------------------------------------------------------------------*/
  for (i = 1;; ++i)
  {
    printf("\n****************** Test Loop %u ******************\n", i);
    vol_name = Volume[i % (sizeof(Volume) / sizeof(char *))];
    printf("Volume = %s\n", vol_name);

    /*-----------------------------------------------------------------*/
    /* Format each volume on its first loop.                           */
    /*-----------------------------------------------------------------*/
    if (i <= (sizeof(Volume) / sizeof(char *)))
      if (format(vol_name))
        SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Mount the volume and change to its root directory.              */
    /*-----------------------------------------------------------------*/
    if (mount(vol_name))
      SysFatalError(errno);
    if (chdir(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Test file postion after reads and seeks.                        */
    /*-----------------------------------------------------------------*/
    if (seek_test())
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Test seeks that hit and cross sector boundaries.                */
    /*-----------------------------------------------------------------*/
    if (seek_test2())
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Test utime().                                                   */
    /*-----------------------------------------------------------------*/
    if (utime_test(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Test assorted file system calls.                                */
    /*-----------------------------------------------------------------*/
    if (file_test(vol_name))
      SysFatalError(errno);

    /*-----------------------------------------------------------------*/
    /* Test the creatn() function if flash file system.                */
    /*-----------------------------------------------------------------*/
    vstat(vol_name, &stats);
    if (stats.fat.vol_type == FFS_VOL)
    {
      if (creatn_test(vol_name))
        SysFatalError(errno);
    }

    /*-----------------------------------------------------------------*/
    /* Test directory entry sorting for flash and RAM file systems.    */
    /*-----------------------------------------------------------------*/
    if (stats.fat.vol_type == FFS_VOL || stats.fat.vol_type == RFS_VOL)
    {
      if (sort_test(vol_name))
        SysFatalError(errno);
    }

    /*-----------------------------------------------------------------*/
    /* Unmount volume before starting next test.                       */
    /*-----------------------------------------------------------------*/
    if (unmount(vol_name))
      SysFatalError(errno);
    printf("Success!\n");
  }
}

/***********************************************************************/
/*  OsIdleTask: kernel idle task                                       */
/*                                                                     */
/***********************************************************************/
void OsIdleTask(ui32 unused)
{
  /*-------------------------------------------------------------------*/
  /* Loop forever. Feel free to add code here, but nothing that could  */
  /* block (Don't try a printf()).                                     */
  /*-------------------------------------------------------------------*/
  for (;;) OsAuditStacks();
}

/***********************************************************************/
/*   AppModule: Application interface to software module manager       */
/*                                                                     */
/*       Input: req = module request code                              */
/*              ... = additional parameters specific to request        */
/*                                                                     */
/***********************************************************************/
void *AppModule(int req, ...)
{
  switch (req)
  {
#if PCCARD_SUPPORT
    va_list ap;
    pccSocket *sock;

    case kCardInserted:
      printf("Known card inserted\n");

      /*---------------------------------------------------------------*/
      /* Use va_arg mechanism to fetch pointer to command string.      */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      sock = va_arg(ap, pccSocket *);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Parse card type.                                              */
      /*---------------------------------------------------------------*/
      switch (sock->card.type)
      {
        case PCCC_ATA:
          printf("ATA Drive\n");
          if (pccAddATA(sock, "ata"))
          {
            perror("pccAddATA");
            break;
          }
          break;

        default:
          printf("unk type = %d\n", sock->card.type);
          break;
      }
      break;

    case kCardRemoved:
      printf("Card removed\n");
      break;
#endif
  }

  return NULL;
}

/***********************************************************************/
/*     FsGetId: Get the user and group ID of process                   */
/*                                                                     */
/*      Inputs: uid = place to store user ID                           */
/*              gid = place to store group ID                          */
/*                                                                     */
/***********************************************************************/
void FsGetId(uid_t *uid, gid_t *gid)
{
  ui32 id = taskGetReg(RunningTask, ID_REG);

  *uid = (uid_t)id;
  *gid = id >> 16;
}

/***********************************************************************/
/*     FsSetId: Set the user and group ID of process                   */
/*                                                                     */
/*      Inputs: uid = user ID                                          */
/*              gid = group ID                                         */
/*                                                                     */
/***********************************************************************/
void FsSetId(uid_t uid, gid_t gid)
{
  ui32 id = (gid << 16) | uid;

  taskSetReg(RunningTask, ID_REG, id);
}

/***********************************************************************/
/*   FsSaveCWD: Save per-task current working directory state          */
/*                                                                     */
/*      Inputs: word1 = 1 of 2 words to save                           */
/*              word2 = 2 of 2 words to save                           */
/*                                                                     */
/***********************************************************************/
void FsSaveCWD(ui32 word1, ui32 word2)
{
  taskSetReg(RunningTask, CWD_WD1, word1);
  taskSetReg(RunningTask, CWD_WD2, word2);
}

/***********************************************************************/
/*   FsReadCWD: Read per-task current working directory state          */
/*                                                                     */
/*     Outputs: word1 = 1 of 2 words to retrieve                       */
/*              word2 = 2 of 2 words to retrieve                       */
/*                                                                     */
/***********************************************************************/
void FsReadCWD(ui32 *word1, ui32 *word2)
{
  *word1 = taskGetReg(RunningTask, CWD_WD1);
  *word2 = taskGetReg(RunningTask, CWD_WD2);
}

