// msgraph.h - Outlook calendar + flagged email from Microsoft 365 (Microsoft Graph).
// Sign-in uses the OAuth "device code" flow: press "Sign in to Outlook" on the phone page, then enter
// the code it shows at microsoft.com/devicelogin. The refresh token is kept in flash (NVS).
#pragma once
#include "model.h"

bool msConfigured();   // client_id present in config.json
bool msSignedIn();     // we hold a refresh token
bool msStartLogin();   // request a device code (shown on the panel and the phone page)
int msPollLogin();     // 1 signed in, 0 still waiting, -1 failed/expired (call every few seconds while pending)
bool msLoginPending();
String msUserCode();
String msVerifyUrl();
String msStatus();     // one line for the phone page
void msSignOut();

bool msFetchCalendar(time_t now, std::vector<Meeting> &out);  // from 1 hour ago to 24 hours ahead
bool msFetchFlagged(std::vector<String> &out);                // subjects of flagged emails
// Teams presence: availability (Available, Busy, DoNotDisturb, BeRightBack, Away, Offline...)
// and activity (InACall, InAMeeting, Presenting...). Needs the Presence.Read permission.
bool msFetchPresence(String &availability, String &activity);
