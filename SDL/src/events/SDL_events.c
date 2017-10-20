/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* General event handling code for SDL */

#include "SDL.h"
#include "SDL_events.h"
#include "SDL_syswm.h"
#include "SDL_thread.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "../timer/SDL_timer_c.h"
#if !SDL_JOYSTICK_DISABLED
#include "../joystick/SDL_joystick_c.h"
#endif

/* Public data -- the event filter */
SDL_EventFilter SDL_EventOK = NULL;
void *SDL_EventOKParam;

typedef struct {
    Uint32 bits[8];
} SDL_DisabledEventBlock;

static SDL_DisabledEventBlock *SDL_disabled_events[256];
static Uint32 SDL_userevents = SDL_USEREVENT;

/* Private data -- event queue */
#define MAXEVENTS	128
static struct
{
    SDL_mutex *lock;
    int active;
    int head;
    int tail;
    SDL_Event event[MAXEVENTS];
    int wmmsg_next;
    struct SDL_SysWMmsg wmmsg[MAXEVENTS];
} SDL_EventQ;

/* Private data -- event locking structure */
static struct
{
    SDL_mutex *lock;
    int safe;
} SDL_EventLock;

/* Thread functions */
static SDL_Thread *SDL_EventThread = NULL;      /* Thread handle */
static SDL_threadID event_thread;     /* The event thread id */

void
SDL_Lock_EventThread(void)
{
    if (SDL_EventThread && (SDL_ThreadID() != event_thread)) {
        /* Grab lock and spin until we're sure event thread stopped */
        SDL_mutexP(SDL_EventLock.lock);
        while (!SDL_EventLock.safe) {
            SDL_Delay(1);
        }
    }
}

void
SDL_Unlock_EventThread(void)
{
    if (SDL_EventThread && (SDL_ThreadID() != event_thread)) {
        SDL_mutexV(SDL_EventLock.lock);
    }
}

static __inline__ SDL_bool
SDL_ShouldPollJoystick()
{
    if (SDL_numjoysticks &&
        (!SDL_disabled_events[SDL_JOYAXISMOTION >> 8] ||
         SDL_JoystickEventState(SDL_QUERY))) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static int SDLCALL
SDL_GobbleEvents(void *unused)
{
    event_thread = SDL_ThreadID();

    while (SDL_EventQ.active) {
        SDL_VideoDevice *_this = SDL_GetVideoDevice();

        /* Get events from the video subsystem */
        if (_this) {
            _this->PumpEvents(_this);
        }
#if !SDL_JOYSTICK_DISABLED
        /* Check for joystick state change */
        if (SDL_ShouldPollJoystick()) {
            SDL_JoystickUpdate();
        }
#endif

        /* Give up the CPU for the rest of our timeslice */
        SDL_EventLock.safe = 1;
        if (SDL_timer_running) {
            SDL_ThreadedTimerCheck();
        }
        SDL_Delay(1);

        /* Check for event locking.
           On the P of the lock mutex, if the lock is held, this thread
           will wait until the lock is released before continuing.  The
           safe flag will be set, meaning that the other thread can go
           about it's business.  The safe flag is reset before the V,
           so as soon as the mutex is free, other threads can see that
           it's not safe to interfere with the event thread.
         */
        SDL_mutexP(SDL_EventLock.lock);
        SDL_EventLock.safe = 0;
        SDL_mutexV(SDL_EventLock.lock);
    }
    SDL_SetTimerThreaded(0);
    event_thread = 0;
    return (0);
}

static int
SDL_StartEventThread(Uint32 flags)
{
    /* Reset everything to zero */
    SDL_EventThread = NULL;
    SDL_memset(&SDL_EventLock, 0, sizeof(SDL_EventLock));

    /* Create the lock and set ourselves active */
#if !SDL_THREADS_DISABLED
    SDL_EventQ.lock = SDL_CreateMutex();
    if (SDL_EventQ.lock == NULL) {
        return (-1);
    }
#endif /* !SDL_THREADS_DISABLED */
    SDL_EventQ.active = 1;

    if ((flags & SDL_INIT_EVENTTHREAD) == SDL_INIT_EVENTTHREAD) {
        SDL_EventLock.lock = SDL_CreateMutex();
        if (SDL_EventLock.lock == NULL) {
            return (-1);
        }
        SDL_EventLock.safe = 0;

        /* The event thread will handle timers too */
        SDL_SetTimerThreaded(2);
#if (defined(__WIN32__) && !defined(_WIN32_WCE)) && !defined(HAVE_LIBC)
#undef SDL_CreateThread
        SDL_EventThread =
            SDL_CreateThread(SDL_GobbleEvents, NULL, NULL, NULL);
#else
        SDL_EventThread = SDL_CreateThread(SDL_GobbleEvents, NULL);
#endif
        if (SDL_EventThread == NULL) {
            return (-1);
        }
    } else {
        event_thread = 0;
    }
    return (0);
}

static void
SDL_StopEventThread(void)
{
    SDL_EventQ.active = 0;
    if (SDL_EventThread) {
        SDL_WaitThread(SDL_EventThread, NULL);
        SDL_EventThread = NULL;
        SDL_DestroyMutex(SDL_EventLock.lock);
        SDL_EventLock.lock = NULL;
    }
    if (SDL_EventQ.lock) {
        SDL_DestroyMutex(SDL_EventQ.lock);
        SDL_EventQ.lock = NULL;
    }
}

SDL_threadID
SDL_EventThreadID(void)
{
    return (event_thread);
}

/* Public functions */

void
SDL_StopEventLoop(void)
{
    int i;

    /* Halt the event thread, if running */
    SDL_StopEventThread();

    /* Shutdown event handlers */
    SDL_KeyboardQuit();
    SDL_MouseQuit();
    SDL_QuitQuit();

    /* Clean out EventQ */
    SDL_EventQ.head = 0;
    SDL_EventQ.tail = 0;
    SDL_EventQ.wmmsg_next = 0;

    /* Clear disabled event state */
    for (i = 0; i < SDL_arraysize(SDL_disabled_events); ++i) {
        if (SDL_disabled_events[i]) {
            SDL_free(SDL_disabled_events[i]);
            SDL_disabled_events[i] = NULL;
        }
    }
}

/* This function (and associated calls) may be called more than once */
int
SDL_StartEventLoop(Uint32 flags)
{
    int retcode;

    /* Clean out the event queue */
    SDL_EventThread = NULL;
    SDL_EventQ.lock = NULL;
    SDL_StopEventLoop();

    /* No filter to start with, process most event types */
    SDL_EventOK = NULL;
    SDL_EventState(SDL_SYSWMEVENT, SDL_DISABLE);

    /* Initialize event handlers */
    retcode = 0;
    retcode += SDL_KeyboardInit();
    retcode += SDL_MouseInit();
    retcode += SDL_QuitInit();
    if (retcode < 0) {
        /* We don't expect them to fail, but... */
        return (-1);
    }

    /* Create the lock and event thread */
    if (SDL_StartEventThread(flags) < 0) {
        SDL_StopEventLoop();
        return (-1);
    }
    return (0);
}


/* Add an event to the event queue -- called with the queue locked */
static int
SDL_AddEvent(SDL_Event * event)
{
    int tail, added;

    tail = (SDL_EventQ.tail + 1) % MAXEVENTS;
    if (tail == SDL_EventQ.head) {
        /* Overflow, drop event */
        added = 0;
    } else {
        SDL_EventQ.event[SDL_EventQ.tail] = *event;
        if (event->type == SDL_SYSWMEVENT) {
            /* Note that it's possible to lose an event */
            int next = SDL_EventQ.wmmsg_next;
            SDL_EventQ.wmmsg[next] = *event->syswm.msg;
            SDL_EventQ.event[SDL_EventQ.tail].syswm.msg =
                &SDL_EventQ.wmmsg[next];
            SDL_EventQ.wmmsg_next = (next + 1) % MAXEVENTS;
        }
        SDL_EventQ.tail = tail;
        added = 1;
    }
    return (added);
}

/* Cut an event, and return the next valid spot, or the tail */
/*                           -- called with the queue locked */
static int
SDL_CutEvent(int spot)
{
    if (spot == SDL_EventQ.head) {
        SDL_EventQ.head = (SDL_EventQ.head + 1) % MAXEVENTS;
        return (SDL_EventQ.head);
    } else if ((spot + 1) % MAXEVENTS == SDL_EventQ.tail) {
        SDL_EventQ.tail = spot;
        return (SDL_EventQ.tail);
    } else
        /* We cut the middle -- shift everything over */
    {
        int here, next;

        /* This can probably be optimized with SDL_memcpy() -- careful! */
        if (--SDL_EventQ.tail < 0) {
            SDL_EventQ.tail = MAXEVENTS - 1;
        }
        for (here = spot; here != SDL_EventQ.tail; here = next) {
            next = (here + 1) % MAXEVENTS;
            SDL_EventQ.event[here] = SDL_EventQ.event[next];
        }
        return (spot);
    }
    /* NOTREACHED */
}

/* Lock the event queue, take a peep at it, and unlock it */
int
SDL_PeepEvents(SDL_Event * events, int numevents, SDL_eventaction action,
               Uint32 minType, Uint32 maxType)
{
    int i, used;

    /* Don't look after we've quit */
    if (!SDL_EventQ.active) {
        return (-1);
    }
    /* Lock the event queue */
    used = 0;
    if (SDL_mutexP(SDL_EventQ.lock) == 0) {
        if (action == SDL_ADDEVENT) {
            for (i = 0; i < numevents; ++i) {
                used += SDL_AddEvent(&events[i]);
            }
        } else {
            SDL_Event tmpevent;
            int spot;

            /* If 'events' is NULL, just see if they exist */
            if (events == NULL) {
                action = SDL_PEEKEVENT;
                numevents = 1;
                events = &tmpevent;
            }
            spot = SDL_EventQ.head;
            while ((used < numevents) && (spot != SDL_EventQ.tail)) {
                Uint32 type = SDL_EventQ.event[spot].type;
                if (minType <= type && type <= maxType) {
                    events[used++] = SDL_EventQ.event[spot];
                    if (action == SDL_GETEVENT) {
                        spot = SDL_CutEvent(spot);
                    } else {
                        spot = (spot + 1) % MAXEVENTS;
                    }
                } else {
                    spot = (spot + 1) % MAXEVENTS;
                }
            }
        }
        SDL_mutexV(SDL_EventQ.lock);
    } else {
        SDL_SetError("Couldn't lock event queue");
        used = -1;
    }
    return (used);
}

SDL_bool
SDL_HasEvent(Uint32 type)
{
    return (SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, type, type) > 0);
}

SDL_bool
SDL_HasEvents(Uint32 minType, Uint32 maxType)
{
    return (SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, minType, maxType) > 0);
}

void
SDL_FlushEvent(Uint32 type)
{
    SDL_FlushEvents(type, type);
}

void
SDL_FlushEvents(Uint32 minType, Uint32 maxType)
{
    /* Don't look after we've quit */
    if (!SDL_EventQ.active) {
        return;
    }

    /* Lock the event queue */
    if (SDL_mutexP(SDL_EventQ.lock) == 0) {
        int spot = SDL_EventQ.head;
        while (spot != SDL_EventQ.tail) {
            Uint32 type = SDL_EventQ.event[spot].type;
            if (minType <= type && type <= maxType) {
                spot = SDL_CutEvent(spot);
            } else {
                spot = (spot + 1) % MAXEVENTS;
            }
        }
        SDL_mutexV(SDL_EventQ.lock);
    }
}

/* Run the system dependent event loops */
void
SDL_PumpEvents(void)
{
    if (!SDL_EventThread) {
        SDL_VideoDevice *_this = SDL_GetVideoDevice();

        /* Get events from the video subsystem */
        if (_this) {
            _this->PumpEvents(_this);
        }
#if !SDL_JOYSTICK_DISABLED
        /* Check for joystick state change */
        if (SDL_ShouldPollJoystick()) {
            SDL_JoystickUpdate();
        }
#endif
    }
}

/* Public functions */

int
SDL_PollEvent(SDL_Event * event)
{
    return SDL_WaitEventTimeout(event, 0);
}

int
SDL_WaitEvent(SDL_Event * event)
{
    return SDL_WaitEventTimeout(event, -1);
}

int
SDL_WaitEventTimeout(SDL_Event * event, int timeout)
{
    Uint32 expiration = 0;

    if (timeout > 0)
        expiration = SDL_GetTicks() + timeout;

    for (;;) {
        SDL_PumpEvents();
        switch (SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
        case -1:
            return 0;
        case 1:
            return 1;
        case 0:
            if (timeout == 0) {
                /* Polling and no events, just return */
                return 0;
            }
            if (timeout > 0 && ((int) (SDL_GetTicks() - expiration) >= 0)) {
                /* Timeout expired and no events */
                return 0;
            }
            SDL_Delay(10);
            break;
        }
    }
}

int
SDL_PushEvent(SDL_Event * event)
{
    if (SDL_EventOK && !SDL_EventOK(SDL_EventOKParam, event)) {
        return 0;
    }
    if (SDL_PeepEvents(event, 1, SDL_ADDEVENT, 0, 0) <= 0) {
        return -1;
    }
    return 1;
}

void
SDL_SetEventFilter(SDL_EventFilter filter, void *userdata)
{
    SDL_Event bitbucket;

    /* Set filter and discard pending events */
    SDL_EventOK = filter;
    SDL_EventOKParam = userdata;
    while (SDL_PollEvent(&bitbucket) > 0);
}

SDL_bool
SDL_GetEventFilter(SDL_EventFilter * filter, void **userdata)
{
    if (filter) {
        *filter = SDL_EventOK;
    }
    if (userdata) {
        *userdata = SDL_EventOKParam;
    }
    return SDL_EventOK ? SDL_TRUE : SDL_FALSE;
}

void
SDL_FilterEvents(SDL_EventFilter filter, void *userdata)
{
    if (SDL_mutexP(SDL_EventQ.lock) == 0) {
        int spot;

        spot = SDL_EventQ.head;
        while (spot != SDL_EventQ.tail) {
            if (filter(userdata, &SDL_EventQ.event[spot])) {
                spot = (spot + 1) % MAXEVENTS;
            } else {
                spot = SDL_CutEvent(spot);
            }
        }
    }
    SDL_mutexV(SDL_EventQ.lock);
}

Uint8
SDL_EventState(Uint32 type, int state)
{
    Uint8 current_state;
    Uint8 hi = ((type >> 8) & 0xff);
    Uint8 lo = (type & 0xff);

    if (SDL_disabled_events[hi] &&
        (SDL_disabled_events[hi]->bits[lo/32] & (1 << (lo&31)))) {
        current_state = SDL_DISABLE;
    } else {
        current_state = SDL_ENABLE;
    }

    if (state != current_state)
    {
        switch (state) {
        case SDL_DISABLE:
            /* Disable this event type and discard pending events */
            if (!SDL_disabled_events[hi]) {
                SDL_disabled_events[hi] = (SDL_DisabledEventBlock*) SDL_calloc(1, sizeof(SDL_DisabledEventBlock));
                if (!SDL_disabled_events[hi]) {
                    /* Out of memory, nothing we can do... */
                    break;
                }
            }
            SDL_disabled_events[hi]->bits[lo/32] |= (1 << (lo&31));
            SDL_FlushEvent(type);
            break;
        case SDL_ENABLE:
            SDL_disabled_events[hi]->bits[lo/32] &= ~(1 << (lo&31));
            break;
        default:
            /* Querying state... */
            break;
        }
    }

    return current_state;
}

Uint32
SDL_RegisterEvents(int numevents)
{
    Uint32 event_base;

    if (SDL_userevents+numevents <= SDL_LASTEVENT) {
        event_base = SDL_userevents;
        SDL_userevents += numevents;
    } else {
        event_base = (Uint32)-1;
    }
    return event_base;
}

/* This is a generic event handler.
 */
int
SDL_SendSysWMEvent(SDL_SysWMmsg * message)
{
    int posted;

    posted = 0;
    if (SDL_GetEventState(SDL_SYSWMEVENT) == SDL_ENABLE) {
        SDL_Event event;
        SDL_memset(&event, 0, sizeof(event));
        event.type = SDL_SYSWMEVENT;
        event.syswm.msg = message;
        posted = (SDL_PushEvent(&event) > 0);
    }
    /* Update internal event state */
    return (posted);
}

/* vi: set ts=4 sw=4 expandtab: */
