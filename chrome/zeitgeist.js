var plugin = document.embeds[0];
var tabInfo = {};
var tabIdTimeouts = {};

function onTabCreated (tab) {
	chrome.tabs.executeScript(tab.id, {file: "content_script.js"});
}

function onTabRemoved (tabid) {
	sendLeaveEvent(tabid);
}

function onTabUpdated (tabid, changeInfo, tab) {
	if (!changeInfo.url) return;
	window.clearTimeout(tabIdTimeouts[tabid])
 	tabIdTimeouts[tabid] = window.setTimeout(function(){
 		chrome.tabs.executeScript(tabid, {file: "content_script.js"});},
 		5000);
}

function onBookmarkCreated (bookmarkid, bookmark) {
	if (!bookmark.url) return; // bookmark folder
	var url = bookmark.url;
	var title = bookmark.title;
	var mimetype = "text/html"; // FIXME: really? use XHR to get it?
	plugin.insertEvent(url, url, mimetype, title, plugin.ACCESS_EVENT, plugin.BOOKMARK);
}

function sendAccessEvent (documentInfo, tabid) {
	console.log("Sending access event for " + documentInfo.url);
	var url = documentInfo.url;
	var origin = documentInfo.origin;
	var domain = documentInfo.domain;
	if (domain == null)
	{
		var s = url.split("/");
		s[s.length-1] = '';
		domain = s.join("/");
	}
	var mimetype = documentInfo.mimeType;
	var title = documentInfo.title;
	plugin.insertEvent(url,
	                   domain,
	                   mimetype ? mimetype : "text/html",
	                   title);

	documentInfo.sentAccess = true;
	tabInfo[tabid] = documentInfo;
}

function sendLeaveEvent (tabid) {
	var documentInfo = tabInfo[tabid];
	if (documentInfo == null || documentInfo.sentAccess != true) return;
	console.log("Sending leave event for " + documentInfo.url);

	var url = documentInfo.url;
	var origin = documentInfo.origin;
	var domain = documentInfo.domain;
	if (domain == null)
	{
		var s = url.split("/");
		s[s.length-1] = '';
		domain = s.join("/");
	}
	var mimetype = documentInfo.mimeType;
	var title = documentInfo.title;
	plugin.insertEvent(url,
	                   domain,
	                   mimetype ? mimetype : "text/html",
	                   title,
	                   plugin.LEAVE_EVENT);

	tabInfo[tabid] = null;
}

// this works in chrome 7,8,9
function onExtensionRequest (request, sender, sendResponse) {
	var id = sender.tab.id;
	if (tabInfo[id] == undefined || tabInfo[id].url != request.url) {
		/*if (tabInfo[id] != undefined) {
			console.log("Leaving " + tabInfo[id].url);
			sendLeaveEvent(id);
		}*/
		sendAccessEvent(request, id);
	}
}

var is_chromium = /chromium/.test( navigator.userAgent.toLowerCase() );
if (!is_chromium) plugin.setActor("application://google-chrome.desktop");
else plugin.setActor("application://chromium-browser.desktop");

chrome.extension.onRequest.addListener (onExtensionRequest);
chrome.bookmarks.onCreated.addListener (onBookmarkCreated);
chrome.tabs.onUpdated.addListener (onTabUpdated);
//chrome.tabs.onCreated.addListener (onTabCreated);
chrome.tabs.onRemoved.addListener (onTabRemoved);

chrome.windows.getAll({"populate" : true}, function (windows) {
    for (var i = 0; i < windows.length; i++) {
        var tabs = windows[i].tabs;
        for (var j = 0; j < tabs.length; j++) {
	    chrome.tabs.executeScript(tabs[j].id, {file: "content_script.js"});
        }
    }
});
