/* git-merge-changelog - git "merge" driver for GNU style ChangeLog files.
   Copyright (C) 2008-2010 Bruno Haible <bruno@clisp.org>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* README:
   The default merge driver of 'git' *always* produces conflicts when
   pulling public modifications into a privately modified ChangeLog file.
   This is because ChangeLog files are always modified at the top; the
   default merge driver has no clue how to deal with this. Furthermore
   the conflicts are presented with more <<<< ==== >>>> markers than
   necessary; this is because the default merge driver makes pointless
   efforts to look at the individual line changes inside a ChangeLog entry.

   This program serves as a 'git' merge driver that avoids these problems.
   1. It produces no conflict when ChangeLog entries have been inserted
      at the top both in the public and in the private modification. It
      puts the privately added entries above the publicly added entries.
   2. It respects the structure of ChangeLog files: entries are not split
      into lines but kept together.
   3. It also handles the case of small modifications of past ChangeLog
      entries, or of removed ChangeLog entries: they are merged as one
      would expect it.
   4. Conflicts are presented at the top of the file, rather than where
      they occurred, so that the user will see them immediately. (Unlike
      for source code written in some programming language, conflict markers
      that are located several hundreds lines from the top will not cause
      any syntax error and therefore would be likely to remain unnoticed.)
 */

/* Installation:

   $ gnulib-tool --create-testdir --dir=/tmp/testdir123 git-merge-changelog
   $ cd /tmp/testdir123
   $ ./configure
   $ make
   $ make install

   Additionally, for git users:
     - Add to .git/config of the checkout (or to your $HOME/.gitconfig) the
       lines

          [merge "merge-changelog"]
                  name = GNU-style ChangeLog merge driver
                  driver = /usr/local/bin/git-merge-changelog %O %A %B

     - In every directory that contains a ChangeLog file, add a file
       '.gitattributes' with this line:

          ChangeLog    merge=merge-changelog

       (See "man 5 gitattributes" for more info.)

   Additionally, for bzr users:
     - Install the 'extmerge' bzr plug-in listed at
         <http://doc.bazaar.canonical.com/plugins/en/index.html>
         <http://wiki.bazaar.canonical.com/BzrPlugins>
     - Add to your $HOME/.bazaar/bazaar.conf the line

          external_merge = git-merge-changelog %b %T %o

     - Then, to merge a conflict in a ChangeLog file, use

          $ bzr extmerge ChangeLog

   Additionally, for hg users:
     - Add to your $HOME/.hgrc the lines

        [merge-patterns]
        ChangeLog = git-merge-changelog

        [merge-tools]
        git-merge-changelog.executable = /usr/local/bin/git-merge-changelog
        git-merge-changelog.args = $base $local $other

       See <http://www.selenic.com/mercurial/hgrc.5.html> section merge-tools
       for reference.
 */

/* Use as an alternative to 'diff3':
   git-merge-changelog performs the same role as "diff3 -m", just with
   reordered arguments:
     $ git-merge-changelog %O %A %B
   is comparable to
     $ diff3 -m %A %O %B
 */

/* Calling convention:
   A merge driver is called with three filename arguments:
     1. %O = The common ancestor of %A and %B.
     2. %A = The file's contents from the "current branch".
     3. %B = The file's contents from the "other branch"; this is the contents
        being merged in.

   In case of a "git stash apply" or of an upstream pull (e.g. from a subsystem
   maintainer to a central maintainer) or of a downstream pull with --rebase:
     2. %A = The file's newest pulled contents; modified by other committers.
     3. %B = The user's newest copy of the file; modified by the user.
   In case of a downstream pull (e.g. from a central repository to the user)
   or of an upstream pull with --rebase:
     2. %A = The user's newest copy of the file; modified by the user.
     3. %B = The file's newest pulled contents; modified by other committers.

   It should write its merged output into file %A. It can also echo some
   remarks to stdout.  It should exit with return code 0 if the merge could
   be resolved cleanly, or with non-zero return code if there were conflicts.
 */

/* How it works:
   The structure of a ChangeLog file: It consists of ChangeLog entries. A
   ChangeLog entry starts at a line following a blank line and that starts with
   a non-whitespace character, or at the beginning of a file.
   The merge driver works as follows: It reads the three files into memory and
   dissects them into ChangeLog entries. It then finds the differences between
   %O and %B. They are classified as:
     - removals (some consecutive entries removed),
     - changes (some consecutive entries removed, some consecutive entries
       added),
     - additions (some consecutive entries added).
   The driver then attempts to apply the changes to %A.
   To this effect, it first computes a correspondence between the entries in %O
   and the entries in %A, using fuzzy string matching to still identify changed
   entries.
     - Removals are applied one by one. If the entry is present in %A, at any
       position, it is removed. If not, the removal is marked as a conflict.
     - Additions at the top of %B are applied at the top of %A.
     - Additions between entry x and entry y (y may be the file end) in %B are
       applied between entry x and entry y in %A (if they still exist and are
       still consecutive in %A), otherwise the additions are marked as a
       conflict.
     - Changes are categorized into "simple changes":
         entry1 ... entryn
       are mapped to
         added_entry ... added_entry modified_entry1 ... modified_entryn,
       where the correspondence between entry_i and modified_entry_i is still
       clear; and "big changes": these are all the rest. Simple changes at the
       top of %B are applied by putting the added entries at the top of %A. The
       changes in simple changes are applied one by one; possibly leading to
       single-entry conflicts. Big changes are applied en bloc, possibly
       leading to conflicts spanning multiple entries.
     - Conflicts are output at the top of the file and cause an exit status of
       1.
 */

#include <config.h>

#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "progname.h"
#include "error.h"
#include "read-file.h"
#include "gl_xlist.h"
#include "gl_array_list.h"
#include "gl_linkedhash_list.h"
#include "gl_rbtreehash_list.h"
#include "gl_linked_list.h"
#include "xalloc.h"
#include "xmalloca.h"
#include "fstrcmp.h"
#include "minmax.h"
#include "c-strstr.h"
#include "fwriteerror.h"

#define ASSERT(expr) \
  do                                                                         \
    {                                                                        \
      if (!(expr))                                                           \
        abort ();                                                            \
    }                                                                        \
  while (0)

#define FSTRCMP_THRESHOLD 0.6
#define FSTRCMP_STRICTER_THRESHOLD 0.8

/* Representation of a ChangeLog entry.
   The string may contain NUL bytes; therefore it is represented as a plain
   opaque memory region.  */
struct entry
{
  char *string;
  size_t length;
  /* Cache for the hash code.  */
  bool hashcode_cached;
  size_t hashcode;
};

/* Create an entry.
   The memory region passed by the caller must of indefinite extent.  It is
   *not* copied here.  */
static struct entry *
entry_create (char *string, size_t length)
{
  struct entry *result = XMALLOC (struct entry);
  result->string = string;
  result->length = length;
  result->hashcode_cached = false;
  return result;
}

/* Compare two entries for equality.  */
static bool
entry_equals (const void *elt1, const void *elt2)
{
  const struct entry *entry1 = (const struct entry *) elt1;
  const struct entry *entry2 = (const struct entry *) elt2;
  return entry1->length == entry2->length
         && memcmp (entry1->string, entry2->string, entry1->length) == 0;
}

/* Return a hash code of the contents of a ChangeLog entry.  */
static size_t
entry_hashcode (const void *elt)
{
  struct entry *entry = (struct entry *) elt;
  if (!entry->hashcode_cached)
    {
      /* See http://www.haible.de/bruno/hashfunc.html.  */
      const char *s;
      size_t n;
      size_t h = 0;

      for (s = entry->string, n = entry->length; n > 0; s++, n--)
        h = (unsigned char) *s + ((h << 9) | (h >> (sizeof (size_t) * CHAR_BIT - 9)));

      entry->hashcode = h;
      entry->hashcode_cached = true;
    }
  return entry->hashcode;
}

/* Perform a fuzzy comparison of two ChangeLog entries.
   Return a similarity measure of the two entries, a value between 0 and 1.
   0 stands for very distinct, 1 for identical.
   If the result is < LOWER_BOUND, an arbitrary other value < LOWER_BOUND can
   be returned.  */
static double
entry_fstrcmp (const struct entry *entry1, const struct entry *entry2,
               double lower_bound)
{
  /* fstrcmp works only on NUL terminated strings.  */
  char *memory;
  double similarity;

  if (memchr (entry1->string, '\0', entry1->length) != NULL)
    return 0.0;
  if (memchr (entry2->string, '\0', entry2->length) != NULL)
    return 0.0;
  memory = (char *) xmalloca (entry1->length + 1 + entry2->length + 1);
  {
    char *p = memory;
    memcpy (p, entry1->string, entry1->length);
    p += entry1->length;
    *p++ = '\0';
    memcpy (p, entry2->string, entry2->length);
    p += entry2->length;
    *p++ = '\0';
  }
  similarity =
    fstrcmp_bounded (memory, memory + entry1->length + 1, lower_bound);
  freea (memory);
  return similarity;
}

/* This structure represents an entire ChangeLog file, after it was read
   into memory.  */
struct changelog_file
{
  /* The entries, as a list.  */
  gl_list_t /* <struct entry *> */ entries_list;
  /* The entries, as a list in opposite direction.  */
  gl_list_t /* <struct entry *> */ entries_reversed;
  /* The entries, as an array.  */
  size_t num_entries;
  struct entry **entries;
};

/* Read a ChangeLog file into memory.
   Return the contents in *RESULT.  */
static void
read_changelog_file (const char *filename, struct changelog_file *result)
{
  /* Read the file in text mode, otherwise it's hard to recognize empty
     lines.  */
  size_t length;
  char *contents = read_file (filename, &length);
  if (contents == NULL)
    {
      fprintf (stderr, "could not read file '%s'\n", filename);
      exit (EXIT_FAILURE);
    }

  result->entries_list =
    gl_list_create_empty (GL_LINKEDHASH_LIST, entry_equals, entry_hashcode,
                          NULL, true);
  result->entries_reversed =
    gl_list_create_empty (GL_RBTREEHASH_LIST, entry_equals, entry_hashcode,
                          NULL, true);
  /* A ChangeLog file consists of ChangeLog entries.  A ChangeLog entry starts
     at a line following a blank line and that starts with a non-whitespace
     character, or at the beginning of a file.
     Split the file contents into entries.  */
  {
    char *contents_end = contents + length;
    char *start = contents;
    while (start < contents_end)
      {
        /* Search the end of the current entry.  */
        char *ptr = start;
        struct entry *curr;

        while (ptr < contents_end)
          {
            ptr = memchr (ptr, '\n', contents_end - ptr);
            if (ptr == NULL)
              {
                ptr = contents_end;
                break;
              }
            ptr++;
            if (contents_end - ptr >= 2
                && ptr[0] == '\n'
                && !(ptr[1] == '\n' || ptr[1] == '\t' || ptr[1] == ' '))
              {
                ptr++;
                break;
              }
          }

        curr = entry_create (start, ptr - start);
        gl_list_add_last (result->entries_list, curr);
        gl_list_add_first (result->entries_reversed, curr);

        start = ptr;
      }
  }

  result->num_entries = gl_list_size (result->entries_list);
  result->entries = XNMALLOC (result->num_entries, struct entry *);
  {
    size_t index = 0;
    gl_list_iterator_t iter = gl_list_iterator (result->entries_list);
    const void *elt;
    gl_list_node_t node;
    while (gl_list_iterator_next (&iter, &elt, &node))
      result->entries[index++] = (struct entry *) elt;
    gl_list_iterator_free (&iter);
    ASSERT (index == result->num_entries);
  }
}

/* A mapping (correspondence) between entries of FILE1 and of FILE2.  */
struct entries_mapping
{
  struct changelog_file *file1;
  struct changelog_file *file2;
  /* Mapping from indices in FILE1 to indices in FILE2.
     A value -1 means that the entry from FILE1 is not found in FILE2.
     A value -2 means that it has not yet been computed.  */
  ssize_t *index_mapping;
  /* Mapping from indices in FILE2 to indices in FILE1.
     A value -1 means that the entry from FILE2 is not found in FILE1.
     A value -2 means that it has not yet been computed.  */
  ssize_t *index_mapping_reverse;
};

/* Look up (or lazily compute) the mapping of an entry in FILE1.
   i is the index in FILE1.
   Return the index in FILE2, or -1 when the entry is not found in FILE2.  */
static ssize_t
entries_mapping_get (struct entries_mapping *mapping, ssize_t i)
{
  if (mapping->index_mapping[i] < -1)
    {
      struct changelog_file *file1 = mapping->file1;
      struct changelog_file *file2 = mapping->file2;
      size_t n1 = file1->num_entries;
      size_t n2 = file2->num_entries;
      struct entry *entry_i = file1->entries[i];
      ssize_t j;

      /* Search whether it approximately occurs in file2.  */
      ssize_t best_j = -1;
      double best_j_similarity = 0.0;
      for (j = n2 - 1; j >= 0; j--)
        if (mapping->index_mapping_reverse[j] < 0)
          {
            double similarity =
              entry_fstrcmp (entry_i, file2->entries[j], best_j_similarity);
            if (similarity > best_j_similarity)
              {
                best_j = j;
                best_j_similarity = similarity;
              }
          }
      if (best_j_similarity >= FSTRCMP_THRESHOLD)
        {
          /* Found a similar entry in file2.  */
          struct entry *entry_j = file2->entries[best_j];
          /* Search whether it approximately occurs in file1 at index i.  */
          ssize_t best_i = -1;
          double best_i_similarity = 0.0;
          ssize_t ii;
          for (ii = n1 - 1; ii >= 0; ii--)
            if (mapping->index_mapping[ii] < 0)
              {
                double similarity =
                  entry_fstrcmp (file1->entries[ii], entry_j,
                                 best_i_similarity);
                if (similarity > best_i_similarity)
                  {
                    best_i = ii;
                    best_i_similarity = similarity;
                  }
              }
          if (best_i_similarity >= FSTRCMP_THRESHOLD && best_i == i)
            {
              mapping->index_mapping[i] = best_j;
              mapping->index_mapping_reverse[best_j] = i;
            }
        }
      if (mapping->index_mapping[i] < -1)
        /* It does not approximately occur in FILE2.
           Remember it, for next time.  */
        mapping->index_mapping[i] = -1;
    }
  return mapping->index_mapping[i];
}

/* Look up (or lazily compute) the mapping of an entry in FILE2.
   j is the index in FILE2.
   Return the index in FILE1, or -1 when the entry is not found in FILE1.  */
static ssize_t
entries_mapping_reverse_get (struct entries_mapping *mapping, ssize_t j)
{
  if (mapping->index_mapping_reverse[j] < -1)
    {
      struct changelog_file *file1 = mapping->file1;
      struct changelog_file *file2 = mapping->file2;
      size_t n1 = file1->num_entries;
      size_t n2 = file2->num_entries;
      struct entry *entry_j = file2->entries[j];
      ssize_t i;

      /* Search whether it approximately occurs in file1.  */
      ssize_t best_i = -1;
      double best_i_similarity = 0.0;
      for (i = n1 - 1; i >= 0; i--)
        if (mapping->index_mapping[i] < 0)
          {
            double similarity =
              entry_fstrcmp (file1->entries[i], entry_j, best_i_similarity);
            if (similarity > best_i_similarity)
              {
                best_i = i;
                best_i_similarity = similarity;
              }
          }
      if (best_i_similarity >= FSTRCMP_THRESHOLD)
        {
          /* Found a similar entry in file1.  */
          struct entry *entry_i = file1->entries[best_i];
          /* Search whether it approximately occurs in file2 at index j.  */
          ssize_t best_j = -1;
          double best_j_similarity = 0.0;
          ssize_t jj;
          for (jj = n2 - 1; jj >= 0; jj--)
            if (mapping->index_mapping_reverse[jj] < 0)
              {
                double similarity =
                  entry_fstrcmp (entry_i, file2->entries[jj],
                                 best_j_similarity);
                if (similarity > best_j_similarity)
                  {
                    best_j = jj;
                    best_j_similarity = similarity;
                  }
              }
          if (best_j_similarity >= FSTRCMP_THRESHOLD && best_j == j)
            {
              mapping->index_mapping_reverse[j] = best_i;
              mapping->index_mapping[best_i] = j;
            }
        }
      if (mapping->index_mapping_reverse[j] < -1)
        /* It does not approximately occur in FILE1.
           Remember it, for next time.  */
        mapping->index_mapping_reverse[j] = -1;
    }
  return mapping->index_mapping_reverse[j];
}

/* Compute a mapping (correspondence) between entries of FILE1 and of FILE2.
   The correspondence also takes into account small modifications; i.e. the
   indicated relation is not equality of entries but best-match similarity
   of entries.
   If FULL is true, the maximum of matching is done up-front.  If it is false,
   it is done in a lazy way through the functions entries_mapping_get and
   entries_mapping_reverse_get.
   Return the result in *RESULT.  */
static void
compute_mapping (struct changelog_file *file1, struct changelog_file *file2,
                 bool full,
                 struct entries_mapping *result)
{
  /* Mapping from indices in file1 to indices in file2.  */
  ssize_t *index_mapping;
  /* Mapping from indices in file2 to indices in file1.  */
  ssize_t *index_mapping_reverse;
  size_t n1 = file1->num_entries;
  size_t n2 = file2->num_entries;
  ssize_t i, j;

  index_mapping = XNMALLOC (n1, ssize_t);
  for (i = 0; i < n1; i++)
    index_mapping[i] = -2;

  index_mapping_reverse = XNMALLOC (n2, ssize_t);
  for (j = 0; j < n2; j++)
    index_mapping_reverse[j] = -2;

  for (i = n1 - 1; i >= 0; i--)
    /* Take an entry from file1.  */
    if (index_mapping[i] < -1)
      {
        struct entry *entry = file1->entries[i];
        /* Search whether it occurs in file2.  */
        j = gl_list_indexof (file2->entries_reversed, entry);
        if (j >= 0)
          {
            j = n2 - 1 - j;
            /* Found an exact correspondence.  */
            /* If index_mapping_reverse[j] >= 0, we have already seen other
               copies of this entry, and there were more occurrences of it in
               file1 than in file2.  In this case, do nothing.  */
            if (index_mapping_reverse[j] < 0)
              {
                index_mapping[i] = j;
                index_mapping_reverse[j] = i;
                /* Look for more occurrences of the same entry.  Match them
                   as long as they pair up.  Unpaired occurrences of the same
                   entry are left without mapping.  */
                {
                  ssize_t curr_i = i;
                  ssize_t curr_j = j;

                  for (;;)
                    {
                      ssize_t next_i;
                      ssize_t next_j;

                      next_i =
                        gl_list_indexof_from (file1->entries_reversed,
                                              n1 - curr_i, entry);
                      if (next_i < 0)
                        break;
                      next_j =
                        gl_list_indexof_from (file2->entries_reversed,
                                              n2 - curr_j, entry);
                      if (next_j < 0)
                        break;
                      curr_i = n1 - 1 - next_i;
                      curr_j = n2 - 1 - next_j;
                      ASSERT (index_mapping[curr_i] < 0);
                      ASSERT (index_mapping_reverse[curr_j] < 0);
                      index_mapping[curr_i] = curr_j;
                      index_mapping_reverse[curr_j] = curr_i;
                    }
                }
              }
          }
      }

  result->file1 = file1;
  result->file2 = file2;
  result->index_mapping = index_mapping;
  result->index_mapping_reverse = index_mapping_reverse;

  if (full)
    for (i = n1 - 1; i >= 0; i--)
      entries_mapping_get (result, i);
}

/* An "edit" is a textual modification performed by the user, that needs to
   be applied to the other file.  */
enum edit_type
{
  /* Some consecutive entries were added.  */
  ADDITION,
  /* Some consecutive entries were removed; some other consecutive entries
     were added at the same position.  (Not necessarily the same number of
     entries.)  */
  CHANGE,
  /* Some consecutive entries were removed.  */
  REMOVAL
};

/* This structure represents an edit.  */
struct edit
{
  enum edit_type type;
  /* Range of indices into the entries of FILE1.  */
  ssize_t i1, i2;       /* first, last index; only used for CHANGE, REMOVAL */
  /* Range of indices into the entries of FILE2.  */
  ssize_t j1, j2;       /* first, last index; only used for ADDITION, CHANGE */
};

/* This structure represents the differences from one file, FILE1, to another
   file, FILE2.  */
struct differences
{
  /* An array mapping FILE1 indices to FILE2 indices (or -1 when the entry
     from FILE1 is not found in FILE2).  */
  ssize_t *index_mapping;
  /* An array mapping FILE2 indices to FILE1 indices (or -1 when the entry
     from FILE2 is not found in FILE1).  */
  ssize_t *index_mapping_reverse;
  /* The edits that transform FILE1 into FILE2.  */
  size_t num_edits;
  struct edit **edits;
};

/* Import the difference detection algorithm from GNU diff.  */
#define ELEMENT struct entry *
#define EQUAL entry_equals
#define OFFSET ssize_t
#define EXTRA_CONTEXT_FIELDS \
  ssize_t *index_mapping; \
  ssize_t *index_mapping_reverse;
#define NOTE_DELETE(ctxt, xoff) \
  ctxt->index_mapping[xoff] = -1
#define NOTE_INSERT(ctxt, yoff) \
  ctxt->index_mapping_reverse[yoff] = -1
#include "diffseq.h"

/* Compute the differences between the entries of FILE1 and the entries of
   FILE2.  */
static void
compute_differences (struct changelog_file *file1, struct changelog_file *file2,
                     struct differences *result)
{
  /* Unlike compute_mapping, which mostly ignores the order of the entries and
     therefore works well when some entries are permuted, here we use the order.
     I think this is needed in order to distinguish changes from
     additions+removals; I don't know how to say what is a "change" if the
     files are considered as unordered sets of entries.  */
  struct context ctxt;
  size_t n1 = file1->num_entries;
  size_t n2 = file2->num_entries;
  ssize_t i;
  ssize_t j;
  gl_list_t /* <struct edit *> */ edits;

  ctxt.xvec = file1->entries;
  ctxt.yvec = file2->entries;
  ctxt.index_mapping = XNMALLOC (n1, ssize_t);
  for (i = 0; i < n1; i++)
    ctxt.index_mapping[i] = 0;
  ctxt.index_mapping_reverse = XNMALLOC (n2, ssize_t);
  for (j = 0; j < n2; j++)
    ctxt.index_mapping_reverse[j] = 0;
  ctxt.fdiag = XNMALLOC (2 * (n1 + n2 + 3), ssize_t) + n2 + 1;
  ctxt.bdiag = ctxt.fdiag + n1 + n2 + 3;
  ctxt.too_expensive = n1 + n2;

  /* Store in ctxt.index_mapping and ctxt.index_mapping_reverse a -1 for
     each removed or added entry.  */
  compareseq (0, n1, 0, n2, 0, &ctxt);

  /* Complete the index_mapping and index_mapping_reverse arrays.  */
  i = 0;
  j = 0;
  while (i < n1 || j < n2)
    {
      while (i < n1 && ctxt.index_mapping[i] < 0)
        i++;
      while (j < n2 && ctxt.index_mapping_reverse[j] < 0)
        j++;
      ASSERT ((i < n1) == (j < n2));
      if (i == n1 && j == n2)
        break;
      ctxt.index_mapping[i] = j;
      ctxt.index_mapping_reverse[j] = i;
      i++;
      j++;
    }

  /* Create the edits.  */
  edits = gl_list_create_empty (GL_ARRAY_LIST, NULL, NULL, NULL, true);
  i = 0;
  j = 0;
  while (i < n1 || j < n2)
    {
      if (i == n1)
        {
          struct edit *e;
          ASSERT (j < n2);
          e = XMALLOC (struct edit);
          e->type = ADDITION;
          e->j1 = j;
          e->j2 = n2 - 1;
          gl_list_add_last (edits, e);
          break;
        }
      if (j == n2)
        {
          struct edit *e;
          ASSERT (i < n1);
          e = XMALLOC (struct edit);
          e->type = REMOVAL;
          e->i1 = i;
          e->i2 = n1 - 1;
          gl_list_add_last (edits, e);
          break;
        }
      if (ctxt.index_mapping[i] >= 0)
        {
          if (ctxt.index_mapping_reverse[j] >= 0)
            {
              ASSERT (ctxt.index_mapping[i] == j);
              ASSERT (ctxt.index_mapping_reverse[j] == i);
              i++;
              j++;
            }
          else
            {
              struct edit *e;
              ASSERT (ctxt.index_mapping_reverse[j] < 0);
              e = XMALLOC (struct edit);
              e->type = ADDITION;
              e->j1 = j;
              do
                j++;
              while (j < n2 && ctxt.index_mapping_reverse[j] < 0);
              e->j2 = j - 1;
              gl_list_add_last (edits, e);
            }
        }
      else
        {
          if (ctxt.index_mapping_reverse[j] >= 0)
            {
              struct edit *e;
              ASSERT (ctxt.index_mapping[i] < 0);
              e = XMALLOC (struct edit);
              e->type = REMOVAL;
              e->i1 = i;
              do
                i++;
              while (i < n1 && ctxt.index_mapping[i] < 0);
              e->i2 = i - 1;
              gl_list_add_last (edits, e);
            }
          else
            {
              struct edit *e;
              ASSERT (ctxt.index_mapping[i] < 0);
              ASSERT (ctxt.index_mapping_reverse[j] < 0);
              e = XMALLOC (struct edit);
              e->type = CHANGE;
              e->i1 = i;
              do
                i++;
              while (i < n1 && ctxt.index_mapping[i] < 0);
              e->i2 = i - 1;
              e->j1 = j;
              do
                j++;
              while (j < n2 && ctxt.index_mapping_reverse[j] < 0);
              e->j2 = j - 1;
              gl_list_add_last (edits, e);
            }
        }
    }

  result->index_mapping = ctxt.index_mapping;
  result->index_mapping_reverse = ctxt.index_mapping_reverse;
  result->num_edits = gl_list_size (edits);
  result->edits = XNMALLOC (result->num_edits, struct edit *);
  {
    size_t index = 0;
    gl_list_iterator_t iter = gl_list_iterator (edits);
    const void *elt;
    gl_list_node_t node;
    while (gl_list_iterator_next (&iter, &elt, &node))
      result->edits[index++] = (struct edit *) elt;
    gl_list_iterator_free (&iter);
    ASSERT (index == result->num_edits);
  }
}

/* An empty entry.  */
static struct entry empty_entry = { NULL, 0 };

/* Return the end a paragraph.
   ENTRY is an entry.
   OFFSET is an offset into the entry, OFFSET <= ENTRY->length.
   Return the offset of the end of paragraph, as an offset <= ENTRY->length;
   it is the start of a blank line or the end of the entry.  */
static size_t
find_paragraph_end (const struct entry *entry, size_t offset)
{
  const char *string = entry->string;
  size_t length = entry->length;

  for (;;)
    {
      const char *nl = memchr (string + offset, '\n', length - offset);
      if (nl == NULL)
        return length;
      offset = (nl - string) + 1;
      if (offset < length && string[offset] == '\n')
        return offset;
    }
}

/* Split a merged entry.
   Given an old entry of the form
       TITLE
       BODY
   and a new entry of the form
       TITLE
       BODY1
       BODY'
   where the two titles are the same and BODY and BODY' are very similar,
   this computes two new entries
       TITLE
       BODY1
   and
       TITLE
       BODY'
   and returns true.
   If the entries don't have this form, it returns false.  */
static bool
try_split_merged_entry (const struct entry *old_entry,
                        const struct entry *new_entry,
                        struct entry *new_split[2])
{
  size_t old_title_len = find_paragraph_end (old_entry, 0);
  size_t new_title_len = find_paragraph_end (new_entry, 0);
  struct entry old_body;
  struct entry new_body;
  size_t best_split_offset;
  double best_similarity;
  size_t split_offset;

  /* Same title? */
  if (!(old_title_len == new_title_len
        && memcmp (old_entry->string, new_entry->string, old_title_len) == 0))
    return false;

  old_body.string = old_entry->string + old_title_len;
  old_body.length = old_entry->length - old_title_len;

  /* Determine where to split the new entry.
     This is done by maximizing the similarity between BODY and BODY'.  */
  best_split_offset = split_offset = new_title_len;
  best_similarity = 0.0;
  for (;;)
    {
      double similarity;

      new_body.string = new_entry->string + split_offset;
      new_body.length = new_entry->length - split_offset;
      similarity =
        entry_fstrcmp (&old_body, &new_body, best_similarity);
      if (similarity > best_similarity)
        {
          best_split_offset = split_offset;
          best_similarity = similarity;
        }
      if (best_similarity == 1.0)
        /* It cannot get better.  */
        break;

      if (split_offset < new_entry->length)
        split_offset = find_paragraph_end (new_entry, split_offset + 1);
      else
        break;
    }

  /* BODY' should not be empty.  */
  if (best_split_offset == new_entry->length)
    return false;
  ASSERT (new_entry->string[best_split_offset] == '\n');

  /* A certain similarity between BODY and BODY' is required.  */
  if (best_similarity < FSTRCMP_STRICTER_THRESHOLD)
    return false;

  new_split[0] = entry_create (new_entry->string, best_split_offset + 1);

  {
    size_t len1 = new_title_len;
    size_t len2 = new_entry->length - best_split_offset;
    char *combined = XNMALLOC (len1 + len2, char);
    memcpy (combined, new_entry->string, len1);
    memcpy (combined + len1, new_entry->string + best_split_offset, len2);
    new_split[1] = entry_create (combined, len1 + len2);
  }

  return true;
}

/* Write the contents of an entry to the output stream FP.  */
static void
entry_write (FILE *fp, struct entry *entry)
{
  if (entry->length > 0)
    fwrite (entry->string, 1, entry->length, fp);
}

/* This structure represents a conflict.
   A conflict can occur for various reasons.  */
struct conflict
{
  /* Parts from the ancestor file.  */
  size_t num_old_entries;
  struct entry **old_entries;
  /* Parts of the modified file.  */
  size_t num_modified_entries;
  struct entry **modified_entries;
};

/* Write a conflict to the output stream FP, including markers.  */
static void
conflict_write (FILE *fp, struct conflict *c)
{
  size_t i;

  /* Use the same syntax as git's default merge driver.
     Don't indent the contents of the entries (with things like ">" or "-"),
     otherwise the user needs more textual editing to resolve the conflict.  */
  fputs ("<<<<<<<\n", fp);
  for (i = 0; i < c->num_old_entries; i++)
    entry_write (fp, c->old_entries[i]);
  fputs ("=======\n", fp);
  for (i = 0; i < c->num_modified_entries; i++)
    entry_write (fp, c->modified_entries[i]);
  fputs (">>>>>>>\n", fp);
}

/* Long options.  */
static const struct option long_options[] =
{
  { "help", no_argument, NULL, 'h' },
  { "split-merged-entry", no_argument, NULL, CHAR_MAX + 1 },
  { "version", no_argument, NULL, 'V' },
  { NULL, 0, NULL, 0 }
};

/* Print a usage message and exit.  */
static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try '%s --help' for more information.\n",
             program_name);
  else
    {
      printf ("Usage: %s [OPTION] O-FILE-NAME A-FILE-NAME B-FILE-NAME\n",
              program_name);
      printf ("\n");
      printf ("Merges independent modifications of a ChangeLog style file.\n");
      printf ("O-FILE-NAME names the original file, the ancestor of the two others.\n");
      printf ("A-FILE-NAME names the publicly modified file.\n");
      printf ("B-FILE-NAME names the user-modified file.\n");
      printf ("Writes the merged file into A-FILE-NAME.\n");
      printf ("\n");
      #if 0 /* --split-merged-entry is now on by default.  */
      printf ("Operation modifiers:\n");
      printf ("\
      --split-merged-entry    Possibly split a merged entry between paragraphs.\n\
                              Use this if you have the habit to merge unrelated\n\
                              entries into a single one, separated only by a\n\
                              newline, just because they happened on the same\n\
                              date.\n");
      printf ("\n");
      #endif
      printf ("Informative output:\n");
      printf ("  -h, --help                  display this help and exit\n");
      printf ("  -V, --version               output version information and exit\n");
      printf ("\n");
      fputs ("Report bugs to <bug-gnulib@gnu.org>.\n",
             stdout);
    }

  exit (status);
}

int
main (int argc, char *argv[])
{
  int optchar;
  bool do_help;
  bool do_version;
  bool split_merged_entry;

  /* Set program name for messages.  */
  set_program_name (argv[0]);

  /* Set default values for variables.  */
  do_help = false;
  do_version = false;
  split_merged_entry = true;

  /* Parse command line options.  */
  while ((optchar = getopt_long (argc, argv, "hV", long_options, NULL)) != EOF)
    switch (optchar)
    {
    case '\0':          /* Long option.  */
      break;
    case 'h':
      do_help = true;
      break;
    case 'V':
      do_version = true;
      break;
    case CHAR_MAX + 1:  /* --split-merged-entry */
      break;
    default:
      usage (EXIT_FAILURE);
    }

  if (do_version)
    {
      /* Version information is requested.  */
      printf ("%s\n", program_name);
      printf ("Copyright (C) %s Free Software Foundation, Inc.\n\
License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
",
              "2008");
      printf ("Written by %s.\n", "Bruno Haible");
      exit (EXIT_SUCCESS);
    }

  if (do_help)
    {
      /* Help is requested.  */
      usage (EXIT_SUCCESS);
    }

  /* Test argument count.  */
  if (optind + 3 != argc)
    error (EXIT_FAILURE, 0, "expected three arguments");

  {
    const char *ancestor_file_name; /* O-FILE-NAME */
    const char *destination_file_name; /* A-FILE-NAME */
    bool downstream;
    const char *other_file_name; /* B-FILE-NAME */
    const char *mainstream_file_name;
    const char *modified_file_name;
    struct changelog_file ancestor_file;
    struct changelog_file mainstream_file;
    struct changelog_file modified_file;
    /* Mapping from indices in ancestor_file to indices in mainstream_file.  */
    struct entries_mapping mapping;
    struct differences diffs;
    gl_list_node_t *result_entries_pointers; /* array of pointers into result_entries */
    gl_list_t /* <struct entry *> */ result_entries;
    gl_list_t /* <struct conflict *> */ result_conflicts;

    ancestor_file_name = argv[optind];
    destination_file_name = argv[optind + 1];
    other_file_name = argv[optind + 2];

    /* Heuristic to determine whether it's a pull in downstream direction
       (e.g. pull from a centralized server) or a pull in upstream direction
       (e.g. "git stash apply").

       For ChangeLog this distinction is important. The difference between
       an "upstream" and a "downstream" repository is that more people are
       looking at the "upstream" repository.  They want to be informed about
       changes and expect them to be shown at the top of the ChangeLog.
       When a user pulls downstream, on the other hand, he has two options:
         a) He gets the change entries from the central repository also at the
            top of his ChangeLog, and his own changes come after them.
         b) He gets the change entries from the central repository after those
            he has collected for his branch.  His own change entries stay at
            the top of the ChangeLog file.
       In the case a) he has to reorder the ChangeLog before he can commit.
       No one does that.  So most people want b).
       In other words, the order of entries in a ChangeLog should represent
       the order in which they have flown (or will flow) into the *central*
       repository.

       But in git this is fundamentally indistinguishable, because when Linus
       pulls patches from akpm and akpm pulls patches from Linus, it's not
       clear which of the two is more "upstream".  Also, when you have many
       branches in a repository and pull from one to another, "git" has no way
       to know which branch is more "upstream" than the other.  The git-tag(1)
       manual page also says:
         "One important aspect of git is it is distributed, and being
          distributed largely means there is no inherent "upstream" or
          "downstream" in the system."
       Therefore anyone who attempts to produce a ChangeLog from the merge
       history will fail.

       Here we allow the user to specify the pull direction through an
       environment variable (GIT_UPSTREAM or GIT_DOWNSTREAM).  If these two
       environment variables are not set, we assume a "simple single user"
       usage pattern: He manages local changes through stashes and uses
       "git pull" only to pull downstream.

       How to distinguish these situation? There are several hints:
         - During a "git stash apply", GIT_REFLOG_ACTION is not set.  During
           a "git pull", it is set to 'pull '. During a "git pull --rebase",
           it is set to 'pull --rebase'.  During a "git cherry-pick", it is
           set to 'cherry-pick'.
         - During a "git stash apply", there is an environment variable of
           the form GITHEAD_<40_hex_digits>='Stashed changes'.  */
    {
      const char *var;

      var = getenv ("GIT_DOWNSTREAM");
      if (var != NULL && var[0] != '\0')
        downstream = true;
      else
        {
          var = getenv ("GIT_UPSTREAM");
          if (var != NULL && var[0] != '\0')
            downstream = false;
          else
            {
              var = getenv ("GIT_REFLOG_ACTION");
              #if 0 /* Debugging code */
              printf ("GIT_REFLOG_ACTION=|%s|\n", var);
              #endif
              if (var != NULL
                  && ((strncmp (var, "pull", 4) == 0
                       && c_strstr (var, " --rebase") == NULL)
                      || strncmp (var, "merge origin", 12) == 0))
                downstream = true;
              else
                {
                  /* "git stash apply", "git rebase", "git cherry-pick" and
                     similar.  */
                  downstream = false;
                }
            }
        }
    }

    #if 0 /* Debugging code */
    {
      char buf[1000];
      printf ("First line of %%A:\n");
      sprintf (buf, "head -1 %s", destination_file_name); system (buf);
      printf ("First line of %%B:\n");
      sprintf (buf, "head -1 %s", other_file_name); system (buf);
      printf ("Guessing calling convention: %s\n",
              downstream
              ? "%A = modified by user, %B = upstream"
              : "%A = upstream, %B = modified by user");
    }
    #endif

    if (downstream)
      {
        mainstream_file_name = other_file_name;
        modified_file_name = destination_file_name;
      }
    else
      {
        mainstream_file_name = destination_file_name;
        modified_file_name = other_file_name;
      }

    /* Read the three files into memory.  */
    read_changelog_file (ancestor_file_name, &ancestor_file);
    read_changelog_file (mainstream_file_name, &mainstream_file);
    read_changelog_file (modified_file_name, &modified_file);

    /* Compute correspondence between the entries of ancestor_file and of
       mainstream_file.  */
    compute_mapping (&ancestor_file, &mainstream_file, false, &mapping);
    (void) entries_mapping_reverse_get; /* avoid gcc "defined but not" warning */

    /* Compute differences between the entries of ancestor_file and of
       modified_file.  */
    compute_differences (&ancestor_file, &modified_file, &diffs);

    /* Compute the result.  */
    result_entries_pointers =
      XNMALLOC (mainstream_file.num_entries, gl_list_node_t);
    result_entries =
      gl_list_create_empty (GL_LINKED_LIST, entry_equals, entry_hashcode,
                            NULL, true);
    {
      size_t k;
      for (k = 0; k < mainstream_file.num_entries; k++)
        result_entries_pointers[k] =
          gl_list_add_last (result_entries, mainstream_file.entries[k]);
    }
    result_conflicts =
      gl_list_create_empty (GL_ARRAY_LIST, NULL, NULL, NULL, true);
    {
      size_t e;
      for (e = 0; e < diffs.num_edits; e++)
        {
          struct edit *edit = diffs.edits[e];
          switch (edit->type)
            {
            case ADDITION:
              if (edit->j1 == 0)
                {
                  /* An addition to the top of modified_file.
                     Apply it to the top of mainstream_file.  */
                  ssize_t j;
                  for (j = edit->j2; j >= edit->j1; j--)
                    {
                      struct entry *added_entry = modified_file.entries[j];
                      gl_list_add_first (result_entries, added_entry);
                    }
                }
              else
                {
                  ssize_t i_before;
                  ssize_t i_after;
                  ssize_t k_before;
                  ssize_t k_after;
                  i_before = diffs.index_mapping_reverse[edit->j1 - 1];
                  ASSERT (i_before >= 0);
                  i_after = (edit->j2 + 1 == modified_file.num_entries
                             ? ancestor_file.num_entries
                             : diffs.index_mapping_reverse[edit->j2 + 1]);
                  ASSERT (i_after >= 0);
                  ASSERT (i_after == i_before + 1);
                  /* An addition between ancestor_file.entries[i_before] and
                     ancestor_file.entries[i_after].  See whether these two
                     entries still exist in mainstream_file and are still
                     consecutive.  */
                  k_before = entries_mapping_get (&mapping, i_before);
                  k_after = (i_after == ancestor_file.num_entries
                             ? mainstream_file.num_entries
                             : entries_mapping_get (&mapping, i_after));
                  if (k_before >= 0 && k_after >= 0 && k_after == k_before + 1)
                    {
                      /* Yes, the entry before and after are still neighbours
                         in mainstream_file.  Apply the addition between
                         them.  */
                      if (k_after == mainstream_file.num_entries)
                        {
                          size_t j;
                          for (j = edit->j1; j <= edit->j2; j++)
                            {
                              struct entry *added_entry = modified_file.entries[j];
                              gl_list_add_last (result_entries, added_entry);
                            }
                        }
                      else
                        {
                          gl_list_node_t node_k_after = result_entries_pointers[k_after];
                          size_t j;
                          for (j = edit->j1; j <= edit->j2; j++)
                            {
                              struct entry *added_entry = modified_file.entries[j];
                              gl_list_add_before (result_entries, node_k_after, added_entry);
                            }
                        }
                    }
                  else
                    {
                      /* It's not clear where the additions should be applied.
                         Let the user decide.  */
                      struct conflict *c = XMALLOC (struct conflict);
                      size_t j;
                      c->num_old_entries = 0;
                      c->old_entries = NULL;
                      c->num_modified_entries = edit->j2 - edit->j1 + 1;
                      c->modified_entries =
                        XNMALLOC (c->num_modified_entries, struct entry *);
                      for (j = edit->j1; j <= edit->j2; j++)
                        c->modified_entries[j - edit->j1] = modified_file.entries[j];
                      gl_list_add_last (result_conflicts, c);
                    }
                }
              break;
            case REMOVAL:
              {
                /* Apply the removals one by one.  */
                size_t i;
                for (i = edit->i1; i <= edit->i2; i++)
                  {
                    struct entry *removed_entry = ancestor_file.entries[i];
                    ssize_t k = entries_mapping_get (&mapping, i);
                    if (k >= 0
                        && entry_equals (removed_entry,
                                         mainstream_file.entries[k]))
                      {
                        /* The entry to be removed still exists in
                           mainstream_file.  Remove it.  */
                        gl_list_node_set_value (result_entries,
                                                result_entries_pointers[k],
                                                &empty_entry);
                      }
                    else
                      {
                        /* The entry to be removed was already removed or was
                           modified.  This is a conflict.  */
                        struct conflict *c = XMALLOC (struct conflict);
                        c->num_old_entries = 1;
                        c->old_entries =
                          XNMALLOC (c->num_old_entries, struct entry *);
                        c->old_entries[0] = removed_entry;
                        c->num_modified_entries = 0;
                        c->modified_entries = NULL;
                        gl_list_add_last (result_conflicts, c);
                      }
                  }
              }
              break;
            case CHANGE:
              {
                bool done = false;
                /* When the user usually merges entries from the same day,
                   and this edit is at the top of the file:  */
                if (split_merged_entry && edit->j1 == 0)
                  {
                    /* Test whether the change is "simple merged", i.e. whether
                       it consists of additions, followed by an augmentation of
                       the first changed entry, followed by small changes of the
                       remaining entries:
                         entry_1
                         entry_2
                         ...
                         entry_n
                       are mapped to
                         added_entry
                         ...
                         added_entry
                         augmented_entry_1
                         modified_entry_2
                         ...
                         modified_entry_n.  */
                    if (edit->i2 - edit->i1 <= edit->j2 - edit->j1)
                      {
                        struct entry *split[2];
                        bool simple_merged =
                          try_split_merged_entry (ancestor_file.entries[edit->i1],
                                                  modified_file.entries[edit->i1 + edit->j2 - edit->i2],
                                                  split);
                        if (simple_merged)
                          {
                            size_t i;
                            for (i = edit->i1 + 1; i <= edit->i2; i++)
                              if (entry_fstrcmp (ancestor_file.entries[i],
                                                 modified_file.entries[i + edit->j2 - edit->i2],
                                                 FSTRCMP_THRESHOLD)
                                  < FSTRCMP_THRESHOLD)
                                {
                                  simple_merged = false;
                                  break;
                                }
                          }
                        if (simple_merged)
                          {
                            /* Apply the additions at the top of modified_file.
                               Apply each of the single-entry changes
                               separately.  */
                            size_t num_changed = edit->i2 - edit->i1 + 1; /* > 0 */
                            size_t num_added = (edit->j2 - edit->j1 + 1) - num_changed;
                            ssize_t j;
                            /* First part of the split modified_file.entries[edit->j2 - edit->i2 + edit->i1]:  */
                            gl_list_add_first (result_entries, split[0]);
                            /* The additions.  */
                            for (j = edit->j1 + num_added - 1; j >= edit->j1; j--)
                              {
                                struct entry *added_entry = modified_file.entries[j];
                                gl_list_add_first (result_entries, added_entry);
                              }
                            /* Now the single-entry changes.  */
                            for (j = edit->j1 + num_added; j <= edit->j2; j++)
                              {
                                struct entry *changed_entry =
                                  (j == edit->j1 + num_added
                                   ? split[1]
                                   : modified_file.entries[j]);
                                size_t i = j + edit->i2 - edit->j2;
                                ssize_t k = entries_mapping_get (&mapping, i);
                                if (k >= 0
                                    && entry_equals (ancestor_file.entries[i],
                                                     mainstream_file.entries[k]))
                                  {
                                    gl_list_node_set_value (result_entries,
                                                            result_entries_pointers[k],
                                                            changed_entry);
                                  }
                                else if (!entry_equals (ancestor_file.entries[i],
                                                        changed_entry))
                                  {
                                    struct conflict *c = XMALLOC (struct conflict);
                                    c->num_old_entries = 1;
                                    c->old_entries =
                                      XNMALLOC (c->num_old_entries, struct entry *);
                                    c->old_entries[0] = ancestor_file.entries[i];
                                    c->num_modified_entries = 1;
                                    c->modified_entries =
                                      XNMALLOC (c->num_modified_entries, struct entry *);
                                    c->modified_entries[0] = changed_entry;
                                    gl_list_add_last (result_conflicts, c);
                                  }
                              }
                            done = true;
                          }
                      }
                  }
                if (!done)
                  {
                    bool simple;
                    /* Test whether the change is "simple", i.e. whether it
                       consists of small changes to the old ChangeLog entries
                       and additions before them:
                         entry_1
                         ...
                         entry_n
                       are mapped to
                         added_entry
                         ...
                         added_entry
                         modified_entry_1
                         ...
                         modified_entry_n.  */
                    if (edit->i2 - edit->i1 <= edit->j2 - edit->j1)
                      {
                        size_t i;
                        simple = true;
                        for (i = edit->i1; i <= edit->i2; i++)
                          if (entry_fstrcmp (ancestor_file.entries[i],
                                             modified_file.entries[i + edit->j2 - edit->i2],
                                             FSTRCMP_THRESHOLD)
                              < FSTRCMP_THRESHOLD)
                            {
                              simple = false;
                              break;
                            }
                      }
                    else
                      simple = false;
                    if (simple)
                      {
                        /* Apply the additions and each of the single-entry
                           changes separately.  */
                        size_t num_changed = edit->i2 - edit->i1 + 1; /* > 0 */
                        size_t num_added = (edit->j2 - edit->j1 + 1) - num_changed;
                        if (edit->j1 == 0)
                          {
                            /* A simple change at the top of modified_file.
                               Apply it to the top of mainstream_file.  */
                            ssize_t j;
                            for (j = edit->j1 + num_added - 1; j >= edit->j1; j--)
                              {
                                struct entry *added_entry = modified_file.entries[j];
                                gl_list_add_first (result_entries, added_entry);
                              }
                            for (j = edit->j1 + num_added; j <= edit->j2; j++)
                              {
                                struct entry *changed_entry = modified_file.entries[j];
                                size_t i = j + edit->i2 - edit->j2;
                                ssize_t k = entries_mapping_get (&mapping, i);
                                if (k >= 0
                                    && entry_equals (ancestor_file.entries[i],
                                                     mainstream_file.entries[k]))
                                  {
                                    gl_list_node_set_value (result_entries,
                                                            result_entries_pointers[k],
                                                            changed_entry);
                                  }
                                else
                                  {
                                    struct conflict *c;
                                    ASSERT (!entry_equals (ancestor_file.entries[i],
                                                           changed_entry));
                                    c = XMALLOC (struct conflict);
                                    c->num_old_entries = 1;
                                    c->old_entries =
                                      XNMALLOC (c->num_old_entries, struct entry *);
                                    c->old_entries[0] = ancestor_file.entries[i];
                                    c->num_modified_entries = 1;
                                    c->modified_entries =
                                      XNMALLOC (c->num_modified_entries, struct entry *);
                                    c->modified_entries[0] = changed_entry;
                                    gl_list_add_last (result_conflicts, c);
                                  }
                              }
                            done = true;
                          }
                        else
                          {
                            ssize_t i_before;
                            ssize_t k_before;
                            bool linear;
                            i_before = diffs.index_mapping_reverse[edit->j1 - 1];
                            ASSERT (i_before >= 0);
                            /* A simple change after ancestor_file.entries[i_before].
                               See whether this entry and the following num_changed
                               entries still exist in mainstream_file and are still
                               consecutive.  */
                            k_before = entries_mapping_get (&mapping, i_before);
                            linear = (k_before >= 0);
                            if (linear)
                              {
                                size_t i;
                                for (i = i_before + 1; i <= i_before + num_changed; i++)
                                  if (entries_mapping_get (&mapping, i) != k_before + (i - i_before))
                                    {
                                      linear = false;
                                      break;
                                    }
                              }
                            if (linear)
                              {
                                gl_list_node_t node_for_insert =
                                  result_entries_pointers[k_before + 1];
                                ssize_t j;
                                for (j = edit->j1 + num_added - 1; j >= edit->j1; j--)
                                  {
                                    struct entry *added_entry = modified_file.entries[j];
                                    gl_list_add_before (result_entries, node_for_insert, added_entry);
                                  }
                                for (j = edit->j1 + num_added; j <= edit->j2; j++)
                                  {
                                    struct entry *changed_entry = modified_file.entries[j];
                                    size_t i = j + edit->i2 - edit->j2;
                                    ssize_t k = entries_mapping_get (&mapping, i);
                                    ASSERT (k >= 0);
                                    if (entry_equals (ancestor_file.entries[i],
                                                      mainstream_file.entries[k]))
                                      {
                                        gl_list_node_set_value (result_entries,
                                                                result_entries_pointers[k],
                                                                changed_entry);
                                      }
                                    else
                                      {
                                        struct conflict *c;
                                        ASSERT (!entry_equals (ancestor_file.entries[i],
                                                               changed_entry));
                                        c = XMALLOC (struct conflict);
                                        c->num_old_entries = 1;
                                        c->old_entries =
                                          XNMALLOC (c->num_old_entries, struct entry *);
                                        c->old_entries[0] = ancestor_file.entries[i];
                                        c->num_modified_entries = 1;
                                        c->modified_entries =
                                          XNMALLOC (c->num_modified_entries, struct entry *);
                                        c->modified_entries[0] = changed_entry;
                                        gl_list_add_last (result_conflicts, c);
                                      }
                                  }
                                done = true;
                              }
                          }
                      }
                    else
                      {
                        /* A big change.
                           See whether the num_changed entries still exist
                           unchanged in mainstream_file and are still
                           consecutive.  */
                        ssize_t i_first;
                        ssize_t k_first;
                        bool linear_unchanged;
                        i_first = edit->i1;
                        k_first = entries_mapping_get (&mapping, i_first);
                        linear_unchanged =
                          (k_first >= 0
                           && entry_equals (ancestor_file.entries[i_first],
                                            mainstream_file.entries[k_first]));
                        if (linear_unchanged)
                          {
                            size_t i;
                            for (i = i_first + 1; i <= edit->i2; i++)
                              if (!(entries_mapping_get (&mapping, i) == k_first + (i - i_first)
                                    && entry_equals (ancestor_file.entries[i],
                                                     mainstream_file.entries[entries_mapping_get (&mapping, i)])))
                                {
                                  linear_unchanged = false;
                                  break;
                                }
                          }
                        if (linear_unchanged)
                          {
                            gl_list_node_t node_for_insert =
                              result_entries_pointers[k_first];
                            ssize_t j;
                            size_t i;
                            for (j = edit->j2; j >= edit->j1; j--)
                              {
                                struct entry *new_entry = modified_file.entries[j];
                                gl_list_add_before (result_entries, node_for_insert, new_entry);
                              }
                            for (i = edit->i1; i <= edit->i2; i++)
                              {
                                ssize_t k = entries_mapping_get (&mapping, i);
                                ASSERT (k >= 0);
                                ASSERT (entry_equals (ancestor_file.entries[i],
                                                      mainstream_file.entries[k]));
                                gl_list_node_set_value (result_entries,
                                                        result_entries_pointers[k],
                                                        &empty_entry);
                              }
                            done = true;
                          }
                      }
                  }
                if (!done)
                  {
                    struct conflict *c = XMALLOC (struct conflict);
                    size_t i, j;
                    c->num_old_entries = edit->i2 - edit->i1 + 1;
                    c->old_entries =
                      XNMALLOC (c->num_old_entries, struct entry *);
                    for (i = edit->i1; i <= edit->i2; i++)
                      c->old_entries[i - edit->i1] = ancestor_file.entries[i];
                    c->num_modified_entries = edit->j2 - edit->j1 + 1;
                    c->modified_entries =
                      XNMALLOC (c->num_modified_entries, struct entry *);
                    for (j = edit->j1; j <= edit->j2; j++)
                      c->modified_entries[j - edit->j1] = modified_file.entries[j];
                    gl_list_add_last (result_conflicts, c);
                  }
              }
              break;
            }
        }
    }

    /* Output the result.  */
    {
      FILE *fp = fopen (destination_file_name, "w");
      if (fp == NULL)
        {
          fprintf (stderr, "could not write file '%s'\n", destination_file_name);
          exit (EXIT_FAILURE);
        }

      /* Output the conflicts at the top.  */
      {
        size_t n = gl_list_size (result_conflicts);
        size_t i;
        for (i = 0; i < n; i++)
          conflict_write (fp, (struct conflict *) gl_list_get_at (result_conflicts, i));
      }
      /* Output the modified and unmodified entries, in order.  */
      {
        gl_list_iterator_t iter = gl_list_iterator (result_entries);
        const void *elt;
        gl_list_node_t node;
        while (gl_list_iterator_next (&iter, &elt, &node))
          entry_write (fp, (struct entry *) elt);
        gl_list_iterator_free (&iter);
      }

      if (fwriteerror (fp))
        {
          fprintf (stderr, "error writing to file '%s'\n", destination_file_name);
          exit (EXIT_FAILURE);
        }
    }

    exit (gl_list_size (result_conflicts) > 0 ? EXIT_FAILURE : EXIT_SUCCESS);
  }
}
