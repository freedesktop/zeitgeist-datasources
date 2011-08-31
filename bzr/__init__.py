# -.- coding: utf-8 -.-

# Zeitgeist
#
# Copyright © 2009 Markus Korn <thekorn@gmx.de>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Post-commit hook to submit the commit to Zeitgeist (http://www.zeitgeist-project.com)

Requires bzr 0.15 or higher.

Copyright (C) 2009, Markus Korn <thekorn@gmx.de>
Copyright (C) 2010, Stefano Candori <stefano.candori@gmail.com>
Copyright (C) 2011, Jelmer Vernooij <jelmer@samba.org>
Published under the GNU GPLv2 or later

Installation:
Copy this directory to ~/.bazaar/plugins/zeitgeist/*
"""

import time
from bzrlib import (
    branch,
    trace,
    )

_client = None
_client_checked = None

def get_client():
    global _client_checked, _client
    if _client_checked:
        return _client
    _client_checked = True
    try:
        from zeitgeist.client import ZeitgeistClient
    except ImportError:
        _client = None
    except RuntimeError, e:
        trace.info("Unable to connect to Zeitgeist, won't send events."
                   "Reason: '%s'" % e)
        _client = None
    else:
        _client = ZeitgeistClient()
    return _client


def post_commit(local, master, old_revno, old_revid, new_revno, new_revid):
    client = get_client()
    if client is None:
        return
    from zeitgeist.datamodel import Event, Subject, Interpretation, Manifestation
    revision = master.repository.get_revision(new_revid)
    if new_revno == 1:
        interpretation = Interpretation.CREATE_EVENT
    else:
        interpretation = Interpretation.MODIFY_EVENT
    _text  = _("Commited on: ")
    _text += master.base[7:-1] #don't considere file://
    _text += _(" Revision no. : ")
    _text += str(new_revno) + "\n"
    _text += revision.message.rstrip()

    subject = Subject.new_for_values(
        uri=unicode(master.base),
        interpretation=unicode(Interpretation.FOLDER),
        manifestation=unicode(Manifestation.FILE_DATA_OBJECT),
        text=unicode(_text),
        origin=unicode(master.base),
    )
    event = Event.new_for_values(
        timestamp=int(time.time()*1000),
        interpretation=unicode(interpretation),
        manifestation=unicode(Manifestation.USER_ACTIVITY),
        actor="application://bzr.desktop", #something usefull here, always olive-gtk?
        subjects=[subject,]
    )
    client.insert_event(event)


def post_pull(pull_result):
    client = get_client()
    if client is None:
        return
    from zeitgeist.datamodel import Event, Subject, Interpretation, Manifestation
    master = pull_result.master_branch
    revision = master.repository.get_revision(pull_result.new_revid)
    interpretation = Interpretation.RECEIVE_EVENT
    _text = _("Pulled ")
    _text += master.base[7:-1] #don't considere file://
    _text += (" to revision ")
    _text += str(master.revno())+":\n"
    _text += revision.get_summary()
    subject = Subject.new_for_values(
        uri=unicode(master.base),
        interpretation=unicode(Interpretation.FOLDER),
        manifestation=unicode(Manifestation.FILE_DATA_OBJECT),
        text=unicode(_text),
        origin=unicode(master.base),
    )
    event = Event.new_for_values(
        timestamp=int(time.time()*1000),
        interpretation=unicode(interpretation),
        manifestation=unicode(Manifestation.USER_ACTIVITY),
        actor="application://bzr.desktop", #something usefull here, always olive-gtk?
        subjects=[subject,]
    )
    client.insert_event(event)


branch.Branch.hooks.install_named_hook("post_commit", post_commit,
                                       "Zeitgeist dataprovider for bzr")
branch.Branch.hooks.install_named_hook("post_pull", post_pull,
                                       "Zeitgeist dataprovider for bzr")
