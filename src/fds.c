/************************************************************************
 *   IRC - Internet Relay Chat, src/fds.c
 *   Copyright (C) 2003 Lucas Madar
 *
 *   fds.c -- file descriptor list management routines
 *
 */

/* $Id$ */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "fds.h"

#include <sys/poll.h>

void engine_add_fd(int);
void engine_del_fd(int);
void engine_change_fd_state(int, unsigned int);

struct afd_entry {
   int type;
   void *value;
   unsigned int flags;
   void *internal;
};

struct afd_entry fd_list[MAXCONNECTIONS];

static inline void fd_range_assert(int fd)
{
   if(fd < 0 || fd >= MAXCONNECTIONS)
      abort();
}

static inline void fd_notused_assert(int fd)
{
   if(fd_list[fd].type != FDT_NONE)
      abort();
}

static inline void fd_used_assert(int fd)
{
   if(fd_list[fd].type == FDT_NONE)
      abort();
}

void remap_fd(int oldfd, int newfd)
{
   fdfprintf(stderr, "remap_fd: %d %d\n", oldfd, newfd);

   fd_range_assert(oldfd);
   fd_used_assert(oldfd); 
   fd_range_assert(newfd);
   fd_notused_assert(newfd); 

   memcpy(&fd_list[newfd], &fd_list[oldfd], sizeof(struct afd_entry));
   del_fd(oldfd);
   engine_add_fd(newfd);
}

void add_fd(int fd, int type, void *value)
{
   fdfprintf(stderr, "add_fd: %d %d %x\n", fd, type, (int) value);

   fd_range_assert(fd);
   fd_notused_assert(fd);

   fd_list[fd].type = type;
   fd_list[fd].value = value;
   fd_list[fd].flags = 0;
   engine_add_fd(fd);
}

void del_fd(int fd)
{
   fdfprintf(stderr, "del_fd: %d\n", fd);

   fd_range_assert(fd);
   fd_used_assert(fd);

   engine_del_fd(fd);

   fd_list[fd].type = 0;
   fd_list[fd].value = NULL;
   fd_list[fd].internal = NULL;
}

void set_fd_flags(int fd, unsigned int flags)
{
   int oldflags;
   fd_range_assert(fd);
   fd_used_assert(fd);

   oldflags = fd_list[fd].flags;

   fd_list[fd].flags |= flags;

   fdfprintf(stderr, "set_fd_flags: %d %x [%x -> %x]\n", fd, flags, oldflags, fd_list[fd].flags);

   if(oldflags != fd_list[fd].flags)
      engine_change_fd_state(fd, fd_list[fd].flags);
}

void unset_fd_flags(int fd, unsigned int flags)
{
   int oldflags;
   fd_range_assert(fd);
   fd_used_assert(fd);

   oldflags = fd_list[fd].flags;

   fd_list[fd].flags &= ~(flags);

   fdfprintf(stderr, "unset_fd_flags: %d %x [%x -> %x]\n", fd, flags, oldflags, fd_list[fd].flags);
   if(oldflags != fd_list[fd].flags)
      engine_change_fd_state(fd, fd_list[fd].flags);
}

void get_fd_info(int fd, int *type, unsigned int *flags, void **value)
{
   fd_range_assert(fd);
   fd_used_assert(fd);

   *type = fd_list[fd].type;
   *flags = fd_list[fd].flags;
   *value = fd_list[fd].value;
}

unsigned int get_fd_flags(int fd)
{
   fd_range_assert(fd);
   fd_used_assert(fd);

   return fd_list[fd].flags;
}

void init_fds()
{
   memset(fd_list, 0, sizeof(struct afd_entry) * MAXCONNECTIONS);
}

void set_fd_internal(int fd, void *ptr)
{
   fd_list[fd].internal = ptr;
}

void *get_fd_internal(int fd)
{
   return fd_list[fd].internal;
}

/////////////////////////////////////////////////////////

/*
 * check_client_fd
 * 
 * called whenever a state change is necessary on a client
 * ie, when ident and dns are finished
 */

void check_client_fd(aClient *cptr)
{
   if (DoingAuth(cptr))
   {
      unsigned int fdflags = get_fd_flags(cptr->authfd);

      if(!(fdflags & FDF_WANTREAD))
         set_fd_flags(cptr->authfd, FDF_WANTREAD);

      if((cptr->flags & FLAGS_WRAUTH) && !(fdflags & FDF_WANTWRITE))
         set_fd_flags(cptr->authfd, FDF_WANTWRITE);
      else if(!(cptr->flags & FLAGS_WRAUTH) && (fdflags & FDF_WANTWRITE))
         unset_fd_flags(cptr->authfd, FDF_WANTWRITE);

      return;
   }

   if (DoingDNS(cptr))
      return;

   set_fd_flags(cptr->fd, FDF_WANTREAD);
}