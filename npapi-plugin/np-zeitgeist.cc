/*
 * np-zeitgeist.c
 * This file is part of Zeitgeist dataprovider for Chrome.
 *
 * Copyright (C) 2010 - Michal Hruby <michal.mhr@gmail.com>
 *
 * Zeitgeist dataprovider for Chrome is free software; 
 * you can redistribute it and/or modify it under the terms of the GNU Lesser 
 * General Public License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * Zeitgeist dataprovider for Chrome is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "webkit/glue/plugins/nphostapi.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include <zeitgeist.h>

/*
#include <nspr.h>
#include <npapi.h>
#include <npruntime.h>
#include <npfunctions.h>
*/

static NPObject        *so       = NULL;
static NPNetscapeFuncs *npnfuncs = NULL;
static NPP              inst     = NULL;
static ZeitgeistLog    *zg_log   = NULL;
static char            *actor    = NULL;

static bool
hasMethod(NPObject* obj, NPIdentifier methodName) {
  char *name;

  name = npnfuncs->utf8fromidentifier(methodName);

  if (!strcmp(name, "insertEvent")) return true;
  else if (!strcmp(name, "setActor")) return true;

  return false;
}

static bool
invokeInsertEvent (NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  /* args should be: url, origin, mimetype, title, [interpretation] */
  const char *url, *origin, *mimetype, *title;
  const char *interpretation = NULL;
  const char *manifestation = NULL;
  const char *event_interpretation = NULL;
  ZeitgeistEvent *event;

  if(argCount < 4 || argCount > 6)
  {
    npnfuncs->setexception(obj, "exception during invocation");
    return false;
  }

  for (int i=0; i<argCount; i++)
  {
    if (!NPVARIANT_IS_STRING (args[i]))
    {
      npnfuncs->setexception(obj, "string argument expected");
      g_debug ("argument #%d must be string", i);
      return false;
    }
  }


  url = NPVARIANT_TO_STRING (args[0]).UTF8Characters;
  origin = NPVARIANT_TO_STRING (args[1]).UTF8Characters;
  mimetype = NPVARIANT_TO_STRING (args[2]).UTF8Characters;
  title = NPVARIANT_TO_STRING (args[3]).UTF8Characters;
  if (argCount > 4)
  {
    event_interpretation = NPVARIANT_TO_STRING (args[4]).UTF8Characters;
  }
  if (argCount > 5)
  {
    interpretation = NPVARIANT_TO_STRING (args[5]).UTF8Characters;
  }
  else
  {
    interpretation = ZEITGEIST_NFO_WEBSITE;
  }

  // determine manifestation from url
  manifestation = zeitgeist_manifestation_for_uri (url);
  if (!manifestation) manifestation = ZEITGEIST_NFO_REMOTE_DATA_OBJECT;

  g_debug ("URL: %s, origin: %s, mimeType: %s, title: %s, interpretation: %s",
           url,
           origin,
           mimetype,
           title,
           interpretation);

  event = zeitgeist_event_new_full (
      event_interpretation ? event_interpretation : ZEITGEIST_ZG_ACCESS_EVENT,
      ZEITGEIST_ZG_USER_ACTIVITY,
      actor ? actor : "application://google-chrome.desktop",
      zeitgeist_subject_new_full (
        url,
        interpretation,
        manifestation,
        mimetype,
        origin,
        title,
        "net"),
  NULL);

  zeitgeist_log_insert_events_no_reply (zg_log, event, NULL);

  VOID_TO_NPVARIANT (*result);
  
  return true;
}

static bool
invokeSetActor (NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  const char *actorName;

  if(argCount != 1 || !NPVARIANT_IS_STRING (args[0]))
  {
    npnfuncs->setexception(obj, "exception during invocation");
    return false;
  }

  actorName = NPVARIANT_TO_STRING (args[0]).UTF8Characters;
  g_debug ("setting actor to: \"%s\"", actorName);
  
  if (actor)
  {
    g_free(actor);
  }
  actor = g_strdup (actorName);

  VOID_TO_NPVARIANT (*result);
  
  return true;
}

static bool
invoke(NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result) {
  char *name;

  name = npnfuncs->utf8fromidentifier(methodName);

  if(name)
  {
    if (!strcmp (name, "insertEvent"))
    {
      return invokeInsertEvent(obj, args, argCount, result);
    }
    else if (!strcmp (name, "setActor"))
    {
      return invokeSetActor(obj, args, argCount, result);
    }
  }

  npnfuncs->setexception(obj, "exception during invocation");
  return false;
}

static bool
hasProperty(NPObject *obj, NPIdentifier propertyName) {
  char *name;

  name = npnfuncs->utf8fromidentifier(propertyName);

  if (name)
  {
    if (!strcmp(name, "APPLICATION")) return true;
    if (!strcmp(name, "ARCHIVE")) return true;
    if (!strcmp(name, "AUDIO")) return true;
    if (!strcmp(name, "BOOKMARK")) return true;
    if (!strcmp(name, "DOCUMENT")) return true;
    if (!strcmp(name, "IMAGE")) return true;
    if (!strcmp(name, "MESSAGE")) return true;
    if (!strcmp(name, "VIDEO")) return true;
    if (!strcmp(name, "WEBSITE")) return true;

    if (!strcmp(name, "ACCESS_EVENT")) return true;
    if (!strcmp(name, "LEAVE_EVENT")) return true;
  }
  return false;
}

static bool
getProperty(NPObject *obj, NPIdentifier propertyName, NPVariant *result) {
  char *name;

  name = npnfuncs->utf8fromidentifier(propertyName);

  // FIXME: move all of this into separate Interpretation object?
  if (name)
  {
    #define INSTALL_CONSTANT(pName, pValue) \
      if (!strcmp(name, pName)) \
      { \
        size_t str_len = strlen(pValue);\
        char *res_string = (char*)(npnfuncs->memalloc((uint32)(str_len + 1)));\
        strcpy(res_string, pValue);\
        STRINGN_TO_NPVARIANT(res_string, (uint32)str_len, *result); \
        return true; \
      }
    INSTALL_CONSTANT("APPLICATION", ZEITGEIST_NFO_APPLICATION);
    INSTALL_CONSTANT("ARCHIVE", ZEITGEIST_NFO_ARCHIVE);
    INSTALL_CONSTANT("AUDIO", ZEITGEIST_NFO_AUDIO);
    INSTALL_CONSTANT("BOOKMARK", ZEITGEIST_NFO_BOOKMARK);
    INSTALL_CONSTANT("DOCUMENT", ZEITGEIST_NFO_DOCUMENT);
    INSTALL_CONSTANT("IMAGE", ZEITGEIST_NFO_IMAGE);
    INSTALL_CONSTANT("MESSAGE", ZEITGEIST_NMO_MESSAGE);
    INSTALL_CONSTANT("VIDEO", ZEITGEIST_NFO_VIDEO);
    INSTALL_CONSTANT("WEBSITE", ZEITGEIST_NFO_WEBSITE);

    INSTALL_CONSTANT("ACCESS_EVENT", ZEITGEIST_ZG_ACCESS_EVENT);
    INSTALL_CONSTANT("LEAVE_EVENT", ZEITGEIST_ZG_LEAVE_EVENT);
  }

  return false;
}

static NPClass npcRefObject = {
  NP_CLASS_STRUCT_VERSION,
  NULL,
  NULL,
  NULL,
  hasMethod,
  invoke,
  invokeInsertEvent,
  hasProperty,
  getProperty,
  NULL,
  NULL,
};

static NPError
newInstance(NPMIMEType pluginType, NPP instance, uint16 mode,
             int16 argc, char *argn[], char *argv[],
             NPSavedData *saved)
{
  g_debug("np-zeitgeist: %s", __func__);
  inst = instance;
  // tell browser that we're windowless plugin
  npnfuncs->setvalue(instance, NPPVpluginWindowBool, (void*)FALSE);

  if(!zg_log)
  {
    zg_log = zeitgeist_log_new();
  }
  return NPERR_NO_ERROR;
}

static NPError
destroyInstance(NPP instance, NPSavedData **save)
{
  g_debug("np-zeitgeist: %s", __func__);
  if(so)
  {
    npnfuncs->releaseobject(so);
    so = NULL;
  }
  if(zg_log)
  {
    g_object_unref(zg_log);
    zg_log = NULL;
  }
  if(actor)
  {
    g_free(actor);
    actor = NULL;
  }
  return NPERR_NO_ERROR;
}

static NPError
getValue(NPP instance, NPPVariable variable, void *value)
{
  inst = instance;
  switch(variable)
  {
    default:
      g_debug("np-zeitgeist: getvalue - default");
      return NPERR_GENERIC_ERROR;
    case NPPVpluginNameString:
      *((char **)value) = "Zeitgeist Plugin";
      break;
    case NPPVpluginDescriptionString:
      *((char **)value) = "<a href=\"http://www.zeitgeist-project.com/\">Zeitgeist</a> NPAPI plugin.";
      break;
    case NPPVpluginScriptableNPObject:
      if(!so)
        so = npnfuncs->createobject(instance, &npcRefObject);
      npnfuncs->retainobject(so);
      *(NPObject **)value = so;
      break;
    case NPPVpluginNeedsXEmbed:
      *((NPBool *)value) = FALSE;
      break;
  }
  return NPERR_NO_ERROR;
}

static NPError /* expected by Safari on Darwin */
handleEvent(NPP instance, void *ev) {
  inst = instance;
  g_debug("np-zeitgeist: %s", __func__);
  return NPERR_NO_ERROR;
}

static NPError /* expected by Opera */
setWindow(NPP instance, NPWindow* pNPWindow) {
  inst = instance;
  g_debug("np-zeitgeist: %s", __func__);
  return NPERR_NO_ERROR;
}

/* EXPORT */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef OSCALL
#define OSCALL
#endif

NPError OSCALL
NP_GetEntryPoints(NPPluginFuncs *nppfuncs) {
  g_debug("np-zeitgeist: %s", __func__);
  nppfuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  nppfuncs->newp          = newInstance;
  nppfuncs->destroy       = destroyInstance;
  nppfuncs->getvalue      = getValue;
  nppfuncs->event         = handleEvent;
  nppfuncs->setwindow     = setWindow;

  return NPERR_NO_ERROR;
}

#ifndef HIBYTE
#define HIBYTE(x) ((((uint32)(x)) & 0xff00) >> 8)
#endif

NPError OSCALL
NP_Initialize(NPNetscapeFuncs *npnf, 
              NPPluginFuncs *nppfuncs)
{
  g_debug("np-zeitgeist: %s", __func__);
  if(npnf == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if(HIBYTE(npnf->version) > NP_VERSION_MAJOR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  npnfuncs = npnf;
  NP_GetEntryPoints(nppfuncs);
  return NPERR_NO_ERROR;
}

NPError
OSCALL NP_Shutdown() {
  g_debug("np-zeitgeist: %s", __func__);
  return NPERR_NO_ERROR;
}

char *
NP_GetMIMEDescription(void) {
  g_debug("np-zeitgeist: %s", __func__);
  return "application/x-zeitgeist-plugin::Zeitgeist NPAPI plugin";
}

NPError OSCALL /* needs to be present for WebKit based browsers */
NP_GetValue(void *npp, NPPVariable variable, void *value) {
  inst = (NPP)npp;
  return getValue((NPP)npp, variable, value);
}

#ifdef __cplusplus
}
#endif
