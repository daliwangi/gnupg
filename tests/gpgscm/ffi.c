/* FFI interface for TinySCHEME.
 *
 * Copyright (C) 2016 g10 code GmbH
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <gpg-error.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "../../common/util.h"
#include "../../common/exechelp.h"
#include "../../common/sysutils.h"

#include "private.h"
#include "ffi.h"
#include "ffi-private.h"



int
ffi_bool_value (scheme *sc, pointer p)
{
  return ! (p == sc->F);
}



static pointer
do_logand (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  unsigned int v, acc = ~0;
  while (args != sc->NIL)
    {
      FFI_ARG_OR_RETURN (sc, unsigned int, v, number, args);
      acc &= v;
    }
  FFI_RETURN_INT (sc, acc);
}

static pointer
do_logior (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  unsigned int v, acc = 0;
  while (args != sc->NIL)
    {
      FFI_ARG_OR_RETURN (sc, unsigned int, v, number, args);
      acc |= v;
    }
  FFI_RETURN_INT (sc, acc);
}

static pointer
do_logxor (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  unsigned int v, acc = 0;
  while (args != sc->NIL)
    {
      FFI_ARG_OR_RETURN (sc, unsigned int, v, number, args);
      acc ^= v;
    }
  FFI_RETURN_INT (sc, acc);
}

static pointer
do_lognot (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  unsigned int v;
  FFI_ARG_OR_RETURN (sc, unsigned int, v, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  FFI_RETURN_INT (sc, ~v);
}

/* User interface.  */

static pointer
do_flush_stdio (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  fflush (stdout);
  fflush (stderr);
  FFI_RETURN (sc);
}


int use_libreadline;

/* Read a string, and return a pointer to it.  Returns NULL on EOF. */
char *
rl_gets (const char *prompt)
{
  static char *line = NULL;
  char *p;
  xfree (line);

#if HAVE_LIBREADLINE
    {
      line = readline (prompt);
      if (line && *line)
        add_history (line);
    }
#else
    {
      size_t max_size = 0xff;
      printf ("%s", prompt);
      fflush (stdout);
      line = xtrymalloc (max_size);
      if (line != NULL)
        fgets (line, max_size, stdin);
    }
#endif

  /* Strip trailing whitespace.  */
  if (line && strlen (line) > 0)
    for (p = &line[strlen (line) - 1]; isspace (*p); p--)
      *p = 0;

  return line;
}

static pointer
do_prompt (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  const char *prompt;
  const char *line;
  FFI_ARG_OR_RETURN (sc, const char *, prompt, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  line = rl_gets (prompt);
  if (! line)
    FFI_RETURN_POINTER (sc, sc->EOF_OBJ);

  FFI_RETURN_STRING (sc, line);
}

static pointer
do_sleep (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  unsigned int seconds;
  FFI_ARG_OR_RETURN (sc, unsigned int, seconds, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  sleep (seconds);
  FFI_RETURN (sc);
}

static pointer
do_usleep (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  useconds_t microseconds;
  FFI_ARG_OR_RETURN (sc, useconds_t, microseconds, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  usleep (microseconds);
  FFI_RETURN (sc);
}

static pointer
do_chdir (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *name;
  FFI_ARG_OR_RETURN (sc, char *, name, path, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  if (chdir (name))
    FFI_RETURN_ERR (sc, errno);
  FFI_RETURN (sc);
}

static pointer
do_strerror (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  int error;
  FFI_ARG_OR_RETURN (sc, int, error, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  FFI_RETURN_STRING (sc, gpg_strerror (error));
}

static pointer
do_getenv (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *name;
  FFI_ARG_OR_RETURN (sc, char *, name, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  FFI_RETURN_STRING (sc, getenv (name) ?: "");
}

static pointer
do_setenv (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *name;
  char *value;
  int overwrite;
  FFI_ARG_OR_RETURN (sc, char *, name, string, args);
  FFI_ARG_OR_RETURN (sc, char *, value, string, args);
  FFI_ARG_OR_RETURN (sc, int, overwrite, bool, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  FFI_RETURN_ERR (sc, gnupg_setenv (name, value, overwrite));
}

static pointer
do_exit (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  int retcode;
  FFI_ARG_OR_RETURN (sc, int, retcode, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  exit (retcode);
}

/* XXX: use gnupgs variant b/c mode as string */
static pointer
do_open (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  int fd;
  char *pathname;
  int flags;
  mode_t mode = 0;
  FFI_ARG_OR_RETURN (sc, char *, pathname, path, args);
  FFI_ARG_OR_RETURN (sc, int, flags, number, args);
  if (args != sc->NIL)
    FFI_ARG_OR_RETURN (sc, mode_t, mode, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  fd = open (pathname, flags, mode);
  if (fd == -1)
    FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
  FFI_RETURN_INT (sc, fd);
}

static pointer
do_fdopen (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  FILE *stream;
  int fd;
  char *mode;
  int kind;
  FFI_ARG_OR_RETURN (sc, int, fd, number, args);
  FFI_ARG_OR_RETURN (sc, char *, mode, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  stream = fdopen (fd, mode);
  if (stream == NULL)
    FFI_RETURN_ERR (sc, gpg_error_from_syserror ());

  if (setvbuf (stream, NULL, _IONBF, 0) != 0)
    FFI_RETURN_ERR (sc, gpg_error_from_syserror ());

  kind = 0;
  if (strchr (mode, 'r'))
    kind |= port_input;
  if (strchr (mode, 'w'))
    kind |= port_output;

  FFI_RETURN_POINTER (sc, sc->vptr->mk_port_from_file (sc, stream, kind));
}

static pointer
do_close (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  int fd;
  FFI_ARG_OR_RETURN (sc, int, fd, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  FFI_RETURN_ERR (sc, close (fd) == 0 ? 0 : gpg_error_from_syserror ());
}

static pointer
do_mkdtemp (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *template;
  char buffer[128];
  FFI_ARG_OR_RETURN (sc, char *, template, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  if (strlen (template) > sizeof buffer - 1)
    FFI_RETURN_ERR (sc, EINVAL);
  strncpy (buffer, template, sizeof buffer);

  FFI_RETURN_STRING (sc, gnupg_mkdtemp (buffer));
}

static pointer
do_unlink (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *name;
  FFI_ARG_OR_RETURN (sc, char *, name, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  if (unlink (name) == -1)
    FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
  FFI_RETURN (sc);
}

static gpg_error_t
unlink_recursively (const char *name)
{
  gpg_error_t err = 0;
  struct stat st;

  if (stat (name, &st) == -1)
    return gpg_error_from_syserror ();

  if (S_ISDIR (st.st_mode))
    {
      DIR *dir;
      struct dirent *dent;

      dir = opendir (name);
      if (dir == NULL)
        return gpg_error_from_syserror ();

      while ((dent = readdir (dir)))
        {
          char *child;

          if (strcmp (dent->d_name, ".") == 0
              || strcmp (dent->d_name, "..") == 0)
            continue;

          child = xtryasprintf ("%s/%s", name, dent->d_name);
          if (child == NULL)
            {
              err = gpg_error_from_syserror ();
              goto leave;
            }

          err = unlink_recursively (child);
          xfree (child);
          if (err == gpg_error_from_errno (ENOENT))
            err = 0;
          if (err)
            goto leave;
        }

    leave:
      closedir (dir);
      if (! err)
        rmdir (name);
      return err;
    }
  else
    if (unlink (name) == -1)
      return gpg_error_from_syserror ();
  return 0;
}

static pointer
do_unlink_recursively (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *name;
  FFI_ARG_OR_RETURN (sc, char *, name, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  err = unlink_recursively (name);
  FFI_RETURN (sc);
}

static pointer
do_rename (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *old;
  char *new;
  FFI_ARG_OR_RETURN (sc, char *, old, string, args);
  FFI_ARG_OR_RETURN (sc, char *, new, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  if (rename (old, new) == -1)
    FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
  FFI_RETURN (sc);
}

static pointer
do_getcwd (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  pointer result;
  char *cwd;
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  cwd = gnupg_getcwd ();
  if (cwd == NULL)
    FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
  result = sc->vptr->mk_string (sc, cwd);
  xfree (cwd);
  FFI_RETURN_POINTER (sc, result);
}

static pointer
do_mkdir (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *name;
  char *mode;
  FFI_ARG_OR_RETURN (sc, char *, name, string, args);
  FFI_ARG_OR_RETURN (sc, char *, mode, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  if (gnupg_mkdir (name, mode) == -1)
    FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
  FFI_RETURN (sc);
}

static pointer
do_rmdir (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *name;
  FFI_ARG_OR_RETURN (sc, char *, name, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  if (rmdir (name) == -1)
    FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
  FFI_RETURN (sc);
}



/* estream functions.  */

struct es_object_box
{
  estream_t stream;
  int closed;
};

static void
es_object_finalize (scheme *sc, void *data)
{
  struct es_object_box *box = data;
  (void) sc;

  if (! box->closed)
    es_fclose (box->stream);
  xfree (box);
}

static void
es_object_to_string (scheme *sc, char *out, size_t size, void *data)
{
  struct es_object_box *box = data;
  (void) sc;

  snprintf (out, size, "#estream %p", box->stream);
}

static struct foreign_object_vtable es_object_vtable =
  {
    es_object_finalize,
    es_object_to_string,
  };

static pointer
es_wrap (scheme *sc, estream_t stream)
{
  struct es_object_box *box = xmalloc (sizeof *box);
  if (box == NULL)
    return sc->NIL;

  box->stream = stream;
  box->closed = 0;
  return sc->vptr->mk_foreign_object (sc, &es_object_vtable, box);
}

static struct es_object_box *
es_unwrap (scheme *sc, pointer object)
{
  (void) sc;

  if (! is_foreign_object (object))
    return NULL;

  if (sc->vptr->get_foreign_object_vtable (object) != &es_object_vtable)
    return NULL;

  return sc->vptr->get_foreign_object_data (object);
}

#define CONVERSION_estream(SC, X)	es_unwrap (SC, X)
#define IS_A_estream(SC, X)		es_unwrap (SC, X)

static pointer
do_es_fclose (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  struct es_object_box *box;
  FFI_ARG_OR_RETURN (sc, struct es_object_box *, box, estream, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  err = es_fclose (box->stream);
  if (! err)
    box->closed = 1;
  FFI_RETURN (sc);
}

static pointer
do_es_read (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  struct es_object_box *box;
  size_t bytes_to_read;

  pointer result;
  void *buffer;
  size_t bytes_read;

  FFI_ARG_OR_RETURN (sc, struct es_object_box *, box, estream, args);
  FFI_ARG_OR_RETURN (sc, size_t, bytes_to_read, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  buffer = xtrymalloc (bytes_to_read);
  if (buffer == NULL)
    FFI_RETURN_ERR (sc, ENOMEM);

  err = es_read (box->stream, buffer, bytes_to_read, &bytes_read);
  if (err)
    FFI_RETURN_ERR (sc, err);

  result = sc->vptr->mk_counted_string (sc, buffer, bytes_read);
  xfree (buffer);
  FFI_RETURN_POINTER (sc, result);
}

static pointer
do_es_feof (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  struct es_object_box *box;
  FFI_ARG_OR_RETURN (sc, struct es_object_box *, box, estream, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  FFI_RETURN_POINTER (sc, es_feof (box->stream) ? sc->T : sc->F);
}

static pointer
do_es_write (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  struct es_object_box *box;
  const char *buffer;
  size_t bytes_to_write, bytes_written;

  FFI_ARG_OR_RETURN (sc, struct es_object_box *, box, estream, args);
  /* XXX how to get the length of the string buffer?  scheme strings
     may contain \0.  */
  FFI_ARG_OR_RETURN (sc, const char *, buffer, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  bytes_to_write = strlen (buffer);
  while (bytes_to_write > 0)
    {
      err = es_write (box->stream, buffer, bytes_to_write, &bytes_written);
      if (err)
        break;
      bytes_to_write -= bytes_written;
      buffer += bytes_written;
    }

  FFI_RETURN (sc);
}



/* Process handling.  */

static pointer
do_spawn_process (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  pointer arguments;
  char **argv;
  size_t len;
  unsigned int flags;

  estream_t infp;
  estream_t outfp;
  estream_t errfp;
  pid_t pid;

  FFI_ARG_OR_RETURN (sc, pointer, arguments, list, args);
  FFI_ARG_OR_RETURN (sc, unsigned int, flags, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  err = ffi_list2argv (sc, arguments, &argv, &len);
  if (err == gpg_error (GPG_ERR_INV_VALUE))
    return ffi_sprintf (sc, "%luth element of first argument is "
                        "neither string nor symbol",
                        (unsigned long) len);
  if (err)
    FFI_RETURN_ERR (sc, err);

  if (verbose > 1)
    {
      char **p;
      fprintf (stderr, "Executing:");
      for (p = argv; *p; p++)
        fprintf (stderr, " '%s'", *p);
      fprintf (stderr, "\n");
    }

  err = gnupg_spawn_process (argv[0], (const char **) &argv[1],
                             GPG_ERR_SOURCE_DEFAULT,
                             NULL,
                             flags,
                             &infp, &outfp, &errfp, &pid);
  xfree (argv);
#define IMC(A, B)                                                       \
  _cons (sc, sc->vptr->mk_integer (sc, (unsigned long) (A)), (B), 1)
#define IMS(A, B)                                                       \
  _cons (sc, es_wrap (sc, (A)), (B), 1)
  FFI_RETURN_POINTER (sc, IMS (infp,
                              IMS (outfp,
                                   IMS (errfp,
                                        IMC (pid, sc->NIL)))));
#undef IMS
#undef IMC
}

static pointer
do_spawn_process_fd (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  pointer arguments;
  char **argv;
  size_t len;
  int infd, outfd, errfd;

  pid_t pid;

  FFI_ARG_OR_RETURN (sc, pointer, arguments, list, args);
  FFI_ARG_OR_RETURN (sc, int, infd, number, args);
  FFI_ARG_OR_RETURN (sc, int, outfd, number, args);
  FFI_ARG_OR_RETURN (sc, int, errfd, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  err = ffi_list2argv (sc, arguments, &argv, &len);
  if (err == gpg_error (GPG_ERR_INV_VALUE))
    return ffi_sprintf (sc, "%luth element of first argument is "
                        "neither string nor symbol",
                        (unsigned long) len);
  if (err)
    FFI_RETURN_ERR (sc, err);

  if (verbose > 1)
    {
      char **p;
      fprintf (stderr, "Executing:");
      for (p = argv; *p; p++)
        fprintf (stderr, " '%s'", *p);
      fprintf (stderr, "\n");
    }

  err = gnupg_spawn_process_fd (argv[0], (const char **) &argv[1],
                                infd, outfd, errfd, &pid);
  xfree (argv);
  FFI_RETURN_INT (sc, pid);
}

static pointer
do_wait_process (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  const char *name;
  pid_t pid;
  int hang;

  int retcode;

  FFI_ARG_OR_RETURN (sc, const char *, name, string, args);
  FFI_ARG_OR_RETURN (sc, pid_t, pid, number, args);
  FFI_ARG_OR_RETURN (sc, int, hang, bool, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  err = gnupg_wait_process (name, pid, hang, &retcode);
  if (err == GPG_ERR_GENERAL)
    err = 0;	/* Let the return code speak for itself.  */

  FFI_RETURN_INT (sc, retcode);
}


static pointer
do_wait_processes (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  pointer list_names;
  char **names;
  pointer list_pids;
  size_t i, count;
  pid_t *pids;
  int hang;
  int *retcodes;
  pointer retcodes_list = sc->NIL;

  FFI_ARG_OR_RETURN (sc, pointer, list_names, list, args);
  FFI_ARG_OR_RETURN (sc, pointer, list_pids, list, args);
  FFI_ARG_OR_RETURN (sc, int, hang, bool, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  if (sc->vptr->list_length (sc, list_names)
      != sc->vptr->list_length (sc, list_pids))
    return
      sc->vptr->mk_string (sc, "length of first two arguments must match");

  err = ffi_list2argv (sc, list_names, &names, &count);
  if (err == gpg_error (GPG_ERR_INV_VALUE))
    return ffi_sprintf (sc, "%luth element of first argument is "
                        "neither string nor symbol",
                        (unsigned long) count);
  if (err)
    FFI_RETURN_ERR (sc, err);

  err = ffi_list2intv (sc, list_pids, (int **) &pids, &count);
  if (err == gpg_error (GPG_ERR_INV_VALUE))
    return ffi_sprintf (sc, "%luth element of second argument is "
                        "neither string nor symbol",
                        (unsigned long) count);
  if (err)
    FFI_RETURN_ERR (sc, err);

  retcodes = xtrycalloc (sizeof *retcodes, count);
  if (retcodes == NULL)
    {
      xfree (names);
      xfree (pids);
      FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
    }

  err = gnupg_wait_processes ((const char **) names, pids, count, hang,
                              retcodes);
  if (err == GPG_ERR_GENERAL)
    err = 0;	/* Let the return codes speak.  */

  for (i = 0; i < count; i++)
    retcodes_list =
      (sc->vptr->cons) (sc,
                        sc->vptr->mk_integer (sc,
                                              (long) retcodes[count-1-i]),
                        retcodes_list);

  xfree (names);
  xfree (pids);
  xfree (retcodes);
  FFI_RETURN_POINTER (sc, retcodes_list);
}


static pointer
do_pipe (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  int filedes[2];
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  err = gnupg_create_pipe (filedes);
#define IMC(A, B)                                                       \
  _cons (sc, sc->vptr->mk_integer (sc, (unsigned long) (A)), (B), 1)
  FFI_RETURN_POINTER (sc, IMC (filedes[0],
                              IMC (filedes[1], sc->NIL)));
#undef IMC
}

static pointer
do_inbound_pipe (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  int filedes[2];
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  err = gnupg_create_inbound_pipe (filedes, NULL, 0);
#define IMC(A, B)                                                       \
  _cons (sc, sc->vptr->mk_integer (sc, (unsigned long) (A)), (B), 1)
  FFI_RETURN_POINTER (sc, IMC (filedes[0],
                              IMC (filedes[1], sc->NIL)));
#undef IMC
}

static pointer
do_outbound_pipe (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  int filedes[2];
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  err = gnupg_create_outbound_pipe (filedes, NULL, 0);
#define IMC(A, B)                                                       \
  _cons (sc, sc->vptr->mk_integer (sc, (unsigned long) (A)), (B), 1)
  FFI_RETURN_POINTER (sc, IMC (filedes[0],
                              IMC (filedes[1], sc->NIL)));
#undef IMC
}



/* Test helper functions.  */
static pointer
do_file_equal (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  pointer result = sc->F;
  char *a_name, *b_name;
  int binary;
  const char *mode;
  FILE *a_stream = NULL, *b_stream = NULL;
  struct stat a_stat, b_stat;
#define BUFFER_SIZE	1024
  char a_buf[BUFFER_SIZE], b_buf[BUFFER_SIZE];
#undef BUFFER_SIZE
  size_t chunk;

  FFI_ARG_OR_RETURN (sc, char *, a_name, string, args);
  FFI_ARG_OR_RETURN (sc, char *, b_name, string, args);
  FFI_ARG_OR_RETURN (sc, int, binary, bool, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  mode = binary ? "rb" : "r";
  a_stream = fopen (a_name, mode);
  if (a_stream == NULL)
    goto errout;

  b_stream = fopen (b_name, mode);
  if (b_stream == NULL)
    goto errout;

  if (fstat (fileno (a_stream), &a_stat) < 0)
    goto errout;

  if (fstat (fileno (b_stream), &b_stat) < 0)
    goto errout;

  if (binary && a_stat.st_size != b_stat.st_size)
    {
      if (verbose)
        fprintf (stderr, "Files %s and %s differ in size %lu != %lu\n",
                 a_name, b_name, (unsigned long) a_stat.st_size,
                 (unsigned long) b_stat.st_size);

      goto out;
    }

  while (! feof (a_stream))
    {
      chunk = sizeof a_buf;

      chunk = fread (a_buf, 1, chunk, a_stream);
      if (chunk == 0 && ferror (a_stream))
        goto errout;	/* some error */

      if (fread (b_buf, 1, chunk, b_stream) < chunk)
        {
          if (feof (b_stream))
            goto out;	/* short read */
          goto errout;	/* some error */
        }

      if (chunk > 0 && memcmp (a_buf, b_buf, chunk) != 0)
        goto out;
    }

  fread (b_buf, 1, 1, b_stream);
  if (! feof (b_stream))
    goto out;	/* b is longer */

  /* They match.  */
  result = sc->T;

 out:
  if (a_stream)
    fclose (a_stream);
  if (b_stream)
    fclose (b_stream);
  FFI_RETURN_POINTER (sc, result);
 errout:
  err = gpg_error_from_syserror ();
  goto out;
}

static pointer
do_splice (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  int source;
  int sink;
  ssize_t len = -1;
  char buffer[1024];
  ssize_t bytes_read;
  FFI_ARG_OR_RETURN (sc, int, source, number, args);
  FFI_ARG_OR_RETURN (sc, int, sink, number, args);
  if (args != sc->NIL)
    FFI_ARG_OR_RETURN (sc, ssize_t, len, number, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  while (len == -1 || len > 0)
    {
      size_t want = sizeof buffer;
      if (len > 0 && (ssize_t) want > len)
        want = (size_t) len;

      bytes_read = read (source, buffer, want);
      if (bytes_read == 0)
        break;
      if (bytes_read < 0)
        FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
      if (write (sink, buffer, bytes_read) != bytes_read)
        FFI_RETURN_ERR (sc, gpg_error_from_syserror ());
      if (len != -1)
        len -= bytes_read;
    }
  FFI_RETURN (sc);
}

static pointer
do_string_index (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *haystack;
  char needle;
  ssize_t offset = 0;
  char *position;
  FFI_ARG_OR_RETURN (sc, char *, haystack, string, args);
  FFI_ARG_OR_RETURN (sc, char, needle, character, args);
  if (args != sc->NIL)
    {
      FFI_ARG_OR_RETURN (sc, ssize_t, offset, number, args);
      if (offset < 0)
        return ffi_sprintf (sc, "offset must be positive");
      if (offset > strlen (haystack))
        return ffi_sprintf (sc, "offset exceeds haystack");
    }
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  position = strchr (haystack+offset, needle);
  if (position)
    FFI_RETURN_INT (sc, position - haystack);
  else
    FFI_RETURN_POINTER (sc, sc->F);
}

static pointer
do_string_rindex (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *haystack;
  char needle;
  ssize_t offset = 0;
  char *position;
  FFI_ARG_OR_RETURN (sc, char *, haystack, string, args);
  FFI_ARG_OR_RETURN (sc, char, needle, character, args);
  if (args != sc->NIL)
    {
      FFI_ARG_OR_RETURN (sc, ssize_t, offset, number, args);
      if (offset < 0)
        return ffi_sprintf (sc, "offset must be positive");
      if (offset > strlen (haystack))
        return ffi_sprintf (sc, "offset exceeds haystack");
    }
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  position = strrchr (haystack+offset, needle);
  if (position)
    FFI_RETURN_INT (sc, position - haystack);
  else
    FFI_RETURN_POINTER (sc, sc->F);
}

static pointer
do_string_contains (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  char *haystack;
  char *needle;
  FFI_ARG_OR_RETURN (sc, char *, haystack, string, args);
  FFI_ARG_OR_RETURN (sc, char *, needle, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);
  FFI_RETURN_POINTER (sc, strstr (haystack, needle) ? sc->T : sc->F);
}

static pointer
do_glob (scheme *sc, pointer args)
{
  FFI_PROLOG ();
  pointer result = sc->NIL;
  size_t i;
  char *pattern;
  glob_t pglob;
  FFI_ARG_OR_RETURN (sc, char *, pattern, string, args);
  FFI_ARGS_DONE_OR_RETURN (sc, args);

  switch (glob (pattern, 0, NULL, &pglob))
    {
    case 0:
      for (i = 0; i < pglob.gl_pathc; i++)
        result =
          (sc->vptr->cons) (sc,
                            sc->vptr->mk_string (sc, pglob.gl_pathv[i]),
                            result);
      globfree (&pglob);
      break;

    case GLOB_NOMATCH:
      /* Return the empty list.  */
      break;

    case GLOB_NOSPACE:
      return ffi_sprintf (sc, "out of memory");
    case GLOB_ABORTED:
      return ffi_sprintf (sc, "read error");
    default:
      assert (! "not reached");
    }
  FFI_RETURN_POINTER (sc, result);
}


gpg_error_t
ffi_list2argv (scheme *sc, pointer list, char ***argv, size_t *len)
{
  int i;

  *len = sc->vptr->list_length (sc, list);
  *argv = xtrycalloc (*len + 1, sizeof **argv);
  if (*argv == NULL)
    return gpg_error_from_syserror ();

  for (i = 0; sc->vptr->is_pair (list); list = sc->vptr->pair_cdr (list))
    {
      if (sc->vptr->is_string (sc->vptr->pair_car (list)))
        (*argv)[i++] = sc->vptr->string_value (sc->vptr->pair_car (list));
      else if (sc->vptr->is_symbol (sc->vptr->pair_car (list)))
        (*argv)[i++] = sc->vptr->symname (sc->vptr->pair_car (list));
      else
        {
          xfree (*argv);
          *argv = NULL;
          *len = i;
          return gpg_error (GPG_ERR_INV_VALUE);
        }
    }
  (*argv)[i] = NULL;
  return 0;
}

gpg_error_t
ffi_list2intv (scheme *sc, pointer list, int **intv, size_t *len)
{
  int i;

  *len = sc->vptr->list_length (sc, list);
  *intv = xtrycalloc (*len, sizeof **intv);
  if (*intv == NULL)
    return gpg_error_from_syserror ();

  for (i = 0; sc->vptr->is_pair (list); list = sc->vptr->pair_cdr (list))
    {
      if (sc->vptr->is_number (sc->vptr->pair_car (list)))
        (*intv)[i++] = sc->vptr->ivalue (sc->vptr->pair_car (list));
      else
        {
          xfree (*intv);
          *intv = NULL;
          *len = i;
          return gpg_error (GPG_ERR_INV_VALUE);
        }
    }

  return 0;
}


char *
ffi_schemify_name (const char *s, int macro)
{
  char *n = strdup (s), *p;
  if (n == NULL)
    return s;
  for (p = n; *p; p++)
    {
      *p = (char) tolower (*p);
       /* We convert _ to - in identifiers.  We allow, however, for
	  function names to start with a leading _.  The functions in
	  this namespace are not yet finalized and might change or
	  vanish without warning.  Use them with care.	*/
      if (! macro
	  && p != n
	  && *p == '_')
	*p = '-';
    }
  return n;
}

pointer
ffi_sprintf (scheme *sc, const char *format, ...)
{
  pointer result;
  va_list listp;
  char *expression;
  int size, written;

  va_start (listp, format);
  size = vsnprintf (NULL, 0, format, listp);
  va_end (listp);

  expression = xtrymalloc (size + 1);
  if (expression == NULL)
    return NULL;

  va_start (listp, format);
  written = vsnprintf (expression, size + 1, format, listp);
  va_end (listp);

  assert (size == written);

  result = sc->vptr->mk_string (sc, expression);
  xfree (expression);
  return result;
}

void
ffi_scheme_eval (scheme *sc, const char *format, ...)
{
  va_list listp;
  char *expression;
  int size, written;

  va_start (listp, format);
  size = vsnprintf (NULL, 0, format, listp);
  va_end (listp);

  expression = xtrymalloc (size + 1);
  if (expression == NULL)
    return;

  va_start (listp, format);
  written = vsnprintf (expression, size + 1, format, listp);
  va_end (listp);

  assert (size == written);

  sc->vptr->load_string (sc, expression);
  xfree (expression);
}

gpg_error_t
ffi_init (scheme *sc, const char *argv0, int argc, const char **argv)
{
  int i;
  pointer args = sc->NIL;

  /* bitwise arithmetic */
  ffi_define_function (sc, logand);
  ffi_define_function (sc, logior);
  ffi_define_function (sc, logxor);
  ffi_define_function (sc, lognot);

  /* libc.  */
  ffi_define_constant (sc, O_RDONLY);
  ffi_define_constant (sc, O_WRONLY);
  ffi_define_constant (sc, O_RDWR);
  ffi_define_constant (sc, O_CREAT);
  ffi_define_constant (sc, O_APPEND);
#ifndef O_BINARY
# define O_BINARY	0
#endif
#ifndef O_TEXT
# define O_TEXT		0
#endif
  ffi_define_constant (sc, O_BINARY);
  ffi_define_constant (sc, O_TEXT);
  ffi_define_constant (sc, STDIN_FILENO);
  ffi_define_constant (sc, STDOUT_FILENO);
  ffi_define_constant (sc, STDERR_FILENO);

  ffi_define_function (sc, sleep);
  ffi_define_function (sc, usleep);
  ffi_define_function (sc, chdir);
  ffi_define_function (sc, strerror);
  ffi_define_function (sc, getenv);
  ffi_define_function (sc, setenv);
  ffi_define_function (sc, exit);
  ffi_define_function (sc, open);
  ffi_define_function (sc, fdopen);
  ffi_define_function (sc, close);
  ffi_define_function (sc, mkdtemp);
  ffi_define_function (sc, unlink);
  ffi_define_function (sc, unlink_recursively);
  ffi_define_function (sc, rename);
  ffi_define_function (sc, getcwd);
  ffi_define_function (sc, mkdir);
  ffi_define_function (sc, rmdir);

  /* Process management.  */
  ffi_define_function (sc, spawn_process);
  ffi_define_function (sc, spawn_process_fd);
  ffi_define_function (sc, wait_process);
  ffi_define_function (sc, wait_processes);
  ffi_define_function (sc, pipe);
  ffi_define_function (sc, inbound_pipe);
  ffi_define_function (sc, outbound_pipe);

  /* estream functions.  */
  ffi_define_function_name (sc, "es-fclose", es_fclose);
  ffi_define_function_name (sc, "es-read", es_read);
  ffi_define_function_name (sc, "es-feof", es_feof);
  ffi_define_function_name (sc, "es-write", es_write);

  /* Test helper functions.  */
  ffi_define_function (sc, file_equal);
  ffi_define_function (sc, splice);
  ffi_define_function (sc, string_index);
  ffi_define_function (sc, string_rindex);
  ffi_define_function_name (sc, "string-contains?", string_contains);
  ffi_define_function (sc, glob);

  /* User interface.  */
  ffi_define_function (sc, flush_stdio);
  ffi_define_function (sc, prompt);

  /* Configuration.  */
  ffi_define (sc, "*verbose*", sc->vptr->mk_integer (sc, verbose));

  ffi_define (sc, "*argv0*", sc->vptr->mk_string (sc, argv0));
  for (i = argc - 1; i >= 0; i--)
    {
      pointer value = sc->vptr->mk_string (sc, argv[i]);
      args = (sc->vptr->cons) (sc, value, args);
    }
  ffi_define (sc, "*args*", args);

#if _WIN32
  ffi_define (sc, "*pathsep*", sc->vptr->mk_character (sc, ';'));
#else
  ffi_define (sc, "*pathsep*", sc->vptr->mk_character (sc, ':'));
#endif

  ffi_define (sc, "*stdin*",
              sc->vptr->mk_port_from_file (sc, stdin, port_input));
  ffi_define (sc, "*stdout*",
              sc->vptr->mk_port_from_file (sc, stdout, port_output));
  ffi_define (sc, "*stderr*",
              sc->vptr->mk_port_from_file (sc, stderr, port_output));

  return 0;
}
