/*
 * Copyright (c) 1994-1996,1998-2004 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#include <errno.h>
#include <grp.h>
#ifdef HAVE_LOGIN_CAP_H
# include <login_cap.h>
#endif

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo$";
#endif /* lint */

#ifdef __TANDEM
# define ROOT_UID	65535
#else
# define ROOT_UID	0
#endif

/*
 * Prototypes
 */
static void runas_setup		__P((void));
static void fatal		__P((char *, int));

#ifdef HAVE_SETRESUID
/*
 * Set real and effective and saved uids and gids based on perm.
 * We always retain a saved uid of 0 unless we are headed for an exec().
 * We only flip the effective gid since it only changes for PERM_SUDOERS.
 * This version of set_perms() works fine with the "stay_setuid" option.
 */
void
set_perms(perm)
    int perm;
{
    int error;

    switch (perm) {
	case PERM_FULL_ROOT:
	case PERM_ROOT:
				if (setresuid(ROOT_UID, ROOT_UID, ROOT_UID))
				    fatal("setresuid(ROOT_UID, ROOT_UID, ROOT_UID) failed, your operating system may have a broken setresuid() function\nTry running configure with --disable-setresuid", 0);
			      	break;

	case PERM_USER:
    	    	    	        (void) setresgid(-1, user_gid, -1);
				if (setresuid(user_uid, user_uid, ROOT_UID))
				    fatal("setresuid(user_uid, user_uid, ROOT_UID)", 1);
			      	break;
				
	case PERM_FULL_USER:
				/* headed for exec() */
    	    	    	        (void) setgid(user_gid);
				if (setresuid(user_uid, user_uid, user_uid))
				    fatal("setresuid(user_uid, user_uid, user_uid)", 1);
			      	break;
				
	case PERM_RUNAS:
				if (setresuid(-1, runas_pw->pw_uid, -1))
				    fatal("unable to change to runas uid", 1);
			      	break;

	case PERM_FULL_RUNAS:
				/* headed for exec(), assume euid == ROOT_UID */
				runas_setup();
				error = setresuid(def_stay_setuid ?
				    user_uid : runas_pw->pw_uid,
				    runas_pw->pw_uid, runas_pw->pw_uid);
				if (error)
				    fatal("unable to change to runas uid", 1);
				break;

	case PERM_SUDOERS:
				/* assume euid == ROOT_UID, ruid == user */
				if (setresgid(-1, SUDOERS_GID, -1))
				    fatal("unable to change to sudoers gid", 1);

				/*
				 * If SUDOERS_UID == ROOT_UID and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == ROOT_UID) {
				    if ((SUDOERS_MODE & 040) && setresuid(ROOT_UID, 1, ROOT_UID))
					fatal("setresuid(ROOT_UID, 1, ROOT_UID)", 1);
				} else {
				    if (setresuid(ROOT_UID, SUDOERS_UID, ROOT_UID))
					fatal("setresuid(ROOT_UID, SUDOERS_UID, ROOT_UID)", 1);
				}
			      	break;
	case PERM_TIMESTAMP:
				if (setresuid(ROOT_UID, timestamp_uid, ROOT_UID))
				    fatal("setresuid(ROOT_UID, timestamp_uid, ROOT_UID)", 1);
			      	break;
    }
}

#else
# ifdef HAVE_SETREUID

/*
 * Set real and effective uids and gids based on perm.
 * We always retain a real or effective uid of ROOT_UID unless
 * we are headed for an exec().
 * This version of set_perms() works fine with the "stay_setuid" option.
 */
void
set_perms(perm)
    int perm;
{
    int error;

    switch (perm) {
	case PERM_FULL_ROOT:
	case PERM_ROOT:
				if (setreuid(-1, ROOT_UID))
				    fatal("setreuid(-1, ROOT_UID) failed, your operating system may have a broken setreuid() function\nTry running configure with --disable-setreuid", 0);
				if (setuid(ROOT_UID))
				    fatal("setuid(ROOT_UID)", 1);
			      	break;

	case PERM_USER:
    	    	    	        (void) setregid(-1, user_gid);
				if (setreuid(ROOT_UID, user_uid))
				    fatal("setreuid(ROOT_UID, user_uid)", 1);
			      	break;
				
	case PERM_FULL_USER:
				/* headed for exec() */
    	    	    	        (void) setgid(user_gid);
				if (setreuid(user_uid, user_uid))
				    fatal("setreuid(user_uid, user_uid)", 1);
			      	break;
				
	case PERM_RUNAS:
				if (setreuid(-1, runas_pw->pw_uid))
				    fatal("unable to change to runas uid", 1);
			      	break;

	case PERM_FULL_RUNAS:
				/* headed for exec(), assume euid == ROOT_UID */
				runas_setup();
				error = setreuid(def_stay_setuid ?
				    user_uid : runas_pw->pw_uid,
				    runas_pw->pw_uid);
				if (error)
				    fatal("unable to change to runas uid", 1);
				break;

	case PERM_SUDOERS:
				/* assume euid == ROOT_UID, ruid == user */
				if (setregid(-1, SUDOERS_GID))
				    fatal("unable to change to sudoers gid", 1);

				/*
				 * If SUDOERS_UID == ROOT_UID and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == ROOT_UID) {
				    if ((SUDOERS_MODE & 040) && setreuid(ROOT_UID, 1))
					fatal("setreuid(ROOT_UID, 1)", 1);
				} else {
				    if (setreuid(ROOT_UID, SUDOERS_UID))
					fatal("setreuid(ROOT_UID, SUDOERS_UID)", 1);
				}
			      	break;
	case PERM_TIMESTAMP:
				if (setreuid(ROOT_UID, timestamp_uid))
				    fatal("setreuid(ROOT_UID, timestamp_uid)", 1);
			      	break;
    }
}

# else /* !HAVE_SETRESUID && !HAVE_SETREUID */

/*
 * Set uids and gids based on perm via setuid() and setgid().
 * NOTE: does not support the "stay_setuid" or timestampowner options.
 *       Also, SUDOERS_UID and SUDOERS_GID are not used.
 */
void
set_perms(perm)
    int perm;
{

    switch (perm) {
	case PERM_FULL_ROOT:
	case PERM_ROOT:
				if (setuid(ROOT_UID))
					fatal("setuid(ROOT_UID)", 1);
				break;

	case PERM_FULL_USER:
    	    	    	        (void) setgid(user_gid);
				if (setuid(user_uid))
				    fatal("setuid(user_uid)", 1);
			      	break;
				
	case PERM_FULL_RUNAS:
				runas_setup();
				if (setuid(runas_pw->pw_uid))
				    fatal("unable to change to runas uid", 1);
				break;

	case PERM_USER:
	case PERM_SUDOERS:
	case PERM_RUNAS:
	case PERM_TIMESTAMP:
				/* Unsupported since we can't set euid. */
				break;
    }
}
# endif /* HAVE_SETREUID */
#endif /* HAVE_SETRESUID */

static void
runas_setup()
{
#ifdef HAVE_LOGIN_CAP_H
    int error, flags;
    extern login_cap_t *lc;
#endif

    if (runas_pw->pw_name != NULL) {
#ifdef HAVE_PAM
	pam_prep_user(runas_pw);
#endif /* HAVE_PAM */

#ifdef HAVE_LOGIN_CAP_H
	if (def_use_loginclass) {
	    /*
             * We don't have setusercontext() set the user since we
             * may only want to set the effective uid.  Depending on
             * sudoers and/or command line arguments we may not want
             * setusercontext() to call initgroups().
	     */
	    flags = LOGIN_SETRESOURCES|LOGIN_SETPRIORITY;
	    if (!def_preserve_groups)
		SET(flags, LOGIN_SETGROUP);
	    else if (setgid(runas_pw->pw_gid))
		perror("cannot set gid to runas gid");
	    error = setusercontext(lc, runas_pw,
		runas_pw->pw_uid, flags);
	    if (error) {
		if (runas_pw->pw_uid != ROOT_UID)
		    fatal("unable to set user context", 1);
		else
		    perror("unable to set user context");
	    }
	} else
#endif /* HAVE_LOGIN_CAP_H */
	{
	    if (setgid(runas_pw->pw_gid))
		perror("cannot set gid to runas gid");
#ifdef HAVE_INITGROUPS
	    /*
	     * Initialize group vector unless asked not to.
	     */
	    if (!def_preserve_groups &&
		initgroups(*user_runas, runas_pw->pw_gid) < 0)
		perror("cannot set group vector");
#endif /* HAVE_INITGROUPS */
	}
    }
}

static void
fatal(str, printerr)
    char *str;
    int printerr;
{

    if (str) {
	if (printerr)
	    perror(str);
	else {
	    fputs(str, stderr);
	    fputc('\n', stderr);
	}
    }
    exit(1);
}
