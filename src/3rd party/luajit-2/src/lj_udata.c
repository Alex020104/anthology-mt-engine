/*
** Userdata handling.
** Copyright (C) 2005-2015 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_udata_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_udata.h"

GCudata *lj_udata_new(lua_State *L, MSize sz, GCtab *env)
{
  GCudata *ud = lj_mem_newt(L, sizeof(GCudata) + sz, GCudata);
  global_State *g = G(L);
  newwhite(g, ud);  /* Not finalized. */
  ud->gct = ~LJ_TUDATA;
  ud->udtype = UDTYPE_USERDATA;
  ud->len = sz;
  /* NOBARRIER: The GCudata is new (marked white). */
  setgcrefnull(ud->metatable);
  setgcref(ud->env, obj2gco(env));
  setgcrefnull(ud->nextfin);
  setgcrefnull(ud->nextcand);
  {
    GCudata *tail = gcref(g->gc.udata) ? udref(g->gc.udata) : NULL;
    if (tail) {
      GCudata *head = udnext(tail);
      setgcref(ud->nextud, obj2gco(head));
      setgcref(ud->prevud, obj2gco(tail));
      setgcref(head->prevud, obj2gco(ud));
      setgcref(tail->nextud, obj2gco(ud));
    } else {
      setgcref(ud->nextud, obj2gco(ud));
      setgcref(ud->prevud, obj2gco(ud));
    }
    setgcref(g->gc.udata, obj2gco(ud));
    if (g->gc.state == GCSseparate)
      setgcref(g->gc.udscanstop, obj2gco(ud));
    else if (g->gc.incremental && g->gc.state == GCSatomic) {
      /* Atomic may be exposed at an engine step boundary. Rescan userdata
      ** allocated by the mutator before atomic actually executes. */
      setgcref(g->gc.udscan, obj2gco(ud));
      setgcref(g->gc.udscanstop, obj2gco(ud));
      g->gc.state = GCSseparate;
    }
  }
  /* Chain to userdata list (after main thread). */
  setgcrefr(ud->nextgc, mainthread(g)->nextgc);
  setgcref(mainthread(g)->nextgc, obj2gco(ud));
  return ud;
}

void LJ_FASTCALL lj_udata_free(global_State *g, GCudata *ud)
{
  GCudata *next = udnext(ud);
  GCudata *prev = udprev(ud);
  lua_assert(gcref(ud->nextfin) == NULL && gcref(ud->nextcand) == NULL);
  if (next == ud) {
    lua_assert(prev == ud && udref(g->gc.udata) == ud);
    setgcrefnull(g->gc.udata);
  } else {
    setgcref(prev->nextud, obj2gco(next));
    setgcref(next->prevud, obj2gco(prev));
    if (udref(g->gc.udata) == ud)
      setgcref(g->gc.udata, obj2gco(prev));
  }
  lj_mem_free(g, ud, sizeudata(ud));
}

